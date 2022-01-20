/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Chih-hong Yang <chih-hong.yang@mediatek.com>
 */

#ifndef _MT6369_H_
#define _MT6369_H_

#define SKIP_VOW

/*************Register Bit Define*************/

/* MT6685_DCXO_EXTBUF5_CW0 */
#define MT6685_DCXO_EXTBUF5_CW0 0x79e
#define RG_XO_BBCK5_EN_M_SFT 2

/* MT6369 */
#define MT6369_HWCID0                        0x8
#define MT6369_SMT_CON0                      0x21
#define MT6369_SMT_CON1                      0x22
#define MT6369_SMT_CON2                      0x23
#define MT6369_SMT_CON3                      0x24
#define MT6369_DRV_CON0                      0x27
#define MT6369_DRV_CON1                      0x28
#define MT6369_DRV_CON2                      0x29
#define MT6369_DRV_CON3                      0x2a
#define MT6369_DRV_CON4                      0x2b
#define MT6369_DRV_CON5                      0x2c
#define MT6369_DRV_CON6                      0x2d
#define MT6369_GPIO_DIR0                     0x88
#define MT6369_GPIO_DIR0_SET                 0x89
#define MT6369_GPIO_DIR0_CLR                 0x8a
#define MT6369_GPIO_DIR1                     0x8b
#define MT6369_GPIO_DIR1_SET                 0x8c
#define MT6369_GPIO_DIR1_CLR                 0x8d
#define MT6369_GPIO_DIR2                     0x8e
#define MT6369_GPIO_DIR2_SET                 0x8f
#define MT6369_GPIO_DIR2_CLR                 0x90
#define MT6369_GPIO_MODE3                    0xc4
#define MT6369_GPIO_MODE3_SET                0xc5
#define MT6369_GPIO_MODE3_CLR                0xc6
#define MT6369_GPIO_MODE4                    0xc7
#define MT6369_GPIO_MODE4_SET                0xc8
#define MT6369_GPIO_MODE4_CLR                0xc9
#define MT6369_GPIO_MODE5                    0xca
#define MT6369_GPIO_MODE5_SET                0xcb
#define MT6369_GPIO_MODE5_CLR                0xcc
#define MT6369_GPIO_MODE6                    0xcd
#define MT6369_GPIO_MODE6_SET                0xce
#define MT6369_GPIO_MODE6_CLR                0xcf
#define MT6369_GPIO_MODE7                    0xd0
#define MT6369_GPIO_MODE7_SET                0xd1
#define MT6369_GPIO_MODE7_CLR                0xd2
#define MT6369_GPIO_MODE8                    0xd3
#define MT6369_GPIO_MODE8_SET                0xd4
#define MT6369_GPIO_MODE8_CLR                0xd5
#define MT6369_TOP_CKPDN_CON0                0x10c
#define MT6369_TOP_CKHWEN_CON0               0x121
#define MT6369_OTP_CON0                      0x38b
#define MT6369_OTP_CON8                      0x395
#define MT6369_OTP_CON11                     0x398
#define MT6369_OTP_CON12_L                   0x399
#define MT6369_OTP_CON12_H                   0x39a
#define MT6369_OTP_CON13                     0x39b
#define MT6369_AUXADC_RQST0                  0x1108
#define MT6369_AUXADC_AVG_CON8               0x11a0
#define MT6369_AUXADC_ACCDET0                0x11bf
#define MT6369_AUXADC_ACCDET1                0x11c0
#define MT6369_LDO_VUSB_OP_EN2               0x1c2a
#define MT6369_LDO_VANT18_CON0               0x1cb2
#define MT6369_LDO_VAUD28_CON0               0x1c31

/* audio register */
#define MT6369_AUD_TOP_ANA_ID                0x2300
#define MT6369_AUD_TOP_DIG_ID                0x2301
#define MT6369_AUD_TOP_ANA_REV               0x2302
#define MT6369_AUD_TOP_DIG_REV               0x2303
#define MT6369_AUD_TOP_DSN_DBI               0x2304
#define MT6369_AUD_TOP_DSN_ESP               0x2305
#define MT6369_AUD_TOP_DSN_FPI               0x2306
#define MT6369_AUD_TOP_DSN_DXI               0x2307
#define MT6369_AUD_TOP_CKPDN_TPM0            0x2308
#define MT6369_AUD_TOP_CKPDN_TPM1            0x2309
#define MT6369_AUD_TOP_CKPDN_TPM2            0x230a
#define MT6369_AUD_TOP_CKPDN_TPM3            0x230b
#define MT6369_AUD_TOP_CKPDN_CON0            0x230c
#define MT6369_AUD_TOP_CKPDN_CON0_SET        0x230d
#define MT6369_AUD_TOP_CKPDN_CON0_CLR        0x230e
#define MT6369_AUD_TOP_CKPDN_CON1            0x230f
#define MT6369_AUD_TOP_CKPDN_CON1_SET        0x2310
#define MT6369_AUD_TOP_CKPDN_CON1_CLR        0x2311
#define MT6369_AUD_TOP_CKSEL_CON0            0x2312
#define MT6369_AUD_TOP_CKSEL_CON0_SET        0x2313
#define MT6369_AUD_TOP_CKSEL_CON0_CLR        0x2314
#define MT6369_AUD_TOP_CKTST_CON0            0x2315
#define MT6369_AUD_TOP_CLK_HWEN_CON0         0x2316
#define MT6369_AUD_TOP_CLK_HWEN_CON0_SET     0x2317
#define MT6369_AUD_TOP_CLK_HWEN_CON0_CLR     0x2318
#define MT6369_AUD_TOP_RST_CON0              0x2319
#define MT6369_AUD_TOP_RST_CON0_SET          0x231a
#define MT6369_AUD_TOP_RST_CON0_CLR          0x231b
#define MT6369_AUD_TOP_RST_BANK_CON0         0x231c
#define MT6369_AUD_TOP_INT_CON0              0x231d
#define MT6369_AUD_TOP_INT_CON0_SET          0x231e
#define MT6369_AUD_TOP_INT_CON0_CLR          0x231f
#define MT6369_AUD_TOP_INT_MASK_CON0         0x2320
#define MT6369_AUD_TOP_INT_MASK_CON0_SET     0x2321
#define MT6369_AUD_TOP_INT_MASK_CON0_CLR     0x2322
#define MT6369_AUD_TOP_INT_STATUS0           0x2323
#define MT6369_AUD_TOP_INT_RAW_STATUS0       0x2324
#define MT6369_AUD_TOP_INT_MISC_CON0         0x2325
#define MT6369_AUD_TOP_MON_CON0              0x2326
#define MT6369_AUD_TOP_MON_CON1              0x2327
#define MT6369_AUDIO_DIG_1_ANA_ID            0x2380
#define MT6369_AUDIO_DIG_1_DIG_ID            0x2381
#define MT6369_AUDIO_DIG_1_ANA_REV           0x2382
#define MT6369_AUDIO_DIG_1_DIG_REV           0x2383
#define MT6369_AUDIO_DIG_1_DSN_DBI           0x2384
#define MT6369_AUDIO_DIG_1_DSN_ESP           0x2385
#define MT6369_AUDIO_DIG_1_DSN_FPI           0x2386
#define MT6369_AUDIO_DIG_1_DSN_DXI           0x2387
#define MT6369_AFE_UL_DL_CON0                0x2388
#define MT6369_AFE_DL_SRC2_CON0              0x2389
#define MT6369_AFE_UL_SRC_CON0               0x238a
#define MT6369_AFE_UL_SRC_CON1               0x238b
#define MT6369_AFE_ADDA6_L_SRC_CON0          0x238c
#define MT6369_AFE_ADDA6_UL_SRC_CON1         0x238d
#define MT6369_AFE_TOP_CON0                  0x238e
#define MT6369_AUDIO_TOP_CON0                0x238f
#define MT6369_AFE_MON_DEBUG0                0x2390
#define MT6369_AFE_MON_DEBUG1                0x2391
#define MT6369_AFUNC_AUD_CON0                0x2392
#define MT6369_AFUNC_AUD_CON1                0x2393
#define MT6369_AFUNC_AUD_CON2                0x2394
#define MT6369_AFUNC_AUD_CON3                0x2395
#define MT6369_AFUNC_AUD_CON4                0x2396
#define MT6369_AFUNC_AUD_CON5                0x2397
#define MT6369_AFUNC_AUD_CON6                0x2398
#define MT6369_AFUNC_AUD_CON7                0x2399
#define MT6369_AFUNC_AUD_CON8                0x239a
#define MT6369_AFUNC_AUD_CON9                0x239b
#define MT6369_AFUNC_AUD_CON10               0x239c
#define MT6369_AFUNC_AUD_CON11               0x239d
#define MT6369_AFUNC_AUD_CON12               0x239e
#define MT6369_AFUNC_AUD_CON13               0x239f
#define MT6369_AFUNC_AUD_CON14               0x23a0
#define MT6369_AFUNC_AUD_CON15               0x23a1
#define MT6369_AFUNC_AUD_CON16               0x23a2
#define MT6369_AFUNC_AUD_CON17               0x23a3
#define MT6369_AFUNC_AUD_CON18               0x23a4
#define MT6369_AFUNC_AUD_CON19               0x23a5
#define MT6369_AFUNC_AUD_CON20               0x23a6
#define MT6369_AFUNC_AUD_CON21               0x23a7
#define MT6369_AFUNC_AUD_CON22               0x23a8
#define MT6369_AFUNC_AUD_MON0                0x23a9
#define MT6369_AFUNC_AUD_MON1                0x23aa
#define MT6369_AFUNC_AUD_MON2                0x23ab
#define MT6369_AFUNC_AUD_MON3                0x23ac
#define MT6369_AUDRC_TUNE_MON0               0x23ad
#define MT6369_AUDRC_TUNE_MON1               0x23ae
#define MT6369_AFE_ADDA_MTKAIF_FIFO_CFG0     0x23af
#define MT6369_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 0x23b0
#define MT6369_AFE_ADDA_MTKAIF_MON0          0x23b1
#define MT6369_AFE_ADDA_MTKAIF_MON1          0x23b2
#define MT6369_AFE_ADDA_MTKAIF_MON2          0x23b3
#define MT6369_AFE_ADDA_MTKAIF_MON3          0x23b4
#define MT6369_AFE_ADDA_MTKAIF_MON4          0x23b5
#define MT6369_AFE_ADDA_MTKAIF_MON5          0x23b6
#define MT6369_AFE_ADDA_MTKAIF_MON6          0x23b7
#define MT6369_AFE_ADDA_MTKAIF_MON7          0x23b8
#define MT6369_AFE_ADDA_MTKAIF_MON8          0x23b9
#define MT6369_AFE_ADDA_MTKAIF_MON9          0x23ba
#define MT6369_AFE_ADDA_MTKAIF_MON10         0x23bb
#define MT6369_AFE_ADDA_MTKAIF_CFG0          0x23bc
#define MT6369_AFE_ADDA_MTKAIF_CFG1          0x23bd
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG0       0x23be
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG1       0x23bf
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG2       0x23c0
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG3       0x23c1
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG4       0x23c2
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG5       0x23c3
#define MT6369_AFE_ADDA_MTKAIF_RX_CFG6       0x23c4
#define MT6369_AFE_ADDA_MTKAIF_SYNCWORD_CFG0 0x23c5
#define MT6369_AFE_ADDA_MTKAIF_SYNCWORD_CFG1 0x23c6
#define MT6369_AFE_ADDA_MTKAIF_SYNCWORD_CFG2 0x23c7
#define MT6369_AFE_SGEN_CFG0                 0x23c8
#define MT6369_AFE_SGEN_CFG1                 0x23c9
#define MT6369_AFE_SGEN_CFG2                 0x23ca
#define MT6369_AFE_ADC_ASYNC_FIFO_CFG0       0x23cb
#define MT6369_AFE_ADC_ASYNC_FIFO_CFG1       0x23cc
#define MT6369_AFE_DCCLK_CFG0                0x23cd
#define MT6369_AFE_DCCLK_CFG1                0x23ce
#define MT6369_AFE_DCCLK_CFG2                0x23cf
#define MT6369_AUDIO_DIG_CFG0                0x23d0
#define MT6369_AUDIO_DIG_CFG1                0x23d1
#define MT6369_AUDIO_DIG_CFG2                0x23d2
#define MT6369_AFE_AUD_PAD_TOP               0x23d3
#define MT6369_AFE_AUD_PAD_TOP_MON0          0x23d4
#define MT6369_AFE_AUD_PAD_TOP_MON1          0x23d5
#define MT6369_AFE_AUD_PAD_TOP_MON2          0x23d6
#define MT6369_AFE_DL_NLE_CFG                0x23d7
#define MT6369_AFE_DL_NLE_MON0               0x23d8
#define MT6369_AFE_DL_NLE_MON1               0x23d9
#define MT6369_AFE_CG_EN_MON                 0x23da
#define MT6369_AFE_MIC_ARRAY_CFG0            0x23db
#define MT6369_AFE_MIC_ARRAY_CFG1            0x23dc
#define MT6369_AFE_CHOP_CFG0                 0x23dd
#define MT6369_AFE_MTKAIF_MUX_CFG0           0x23de
#define MT6369_AFE_MTKAIF_MUX_CFG1           0x23df
#define MT6369_AUDIO_DIG_2_ANA_ID            0x2400
#define MT6369_AUDIO_DIG_2_DIG_ID            0x2401
#define MT6369_AUDIO_DIG_2_ANA_REV           0x2402
#define MT6369_AUDIO_DIG_2_DIG_REV           0x2403
#define MT6369_AUDIO_DIG_2_DSN_DBI           0x2404
#define MT6369_AUDIO_DIG_2_DSN_ESP           0x2405
#define MT6369_AUDIO_DIG_2_DSN_FPI           0x2406
#define MT6369_AUDIO_DIG_2_DSN_DXI           0x2407
#define MT6369_AFE_PMIC_NEWIF_CFG0           0x2408
#define MT6369_AFE_PMIC_NEWIF_CFG1           0x2409
#define MT6369_AFE_VOW_TOP_CON0              0x240a
#define MT6369_AFE_VOW_TOP_CON1              0x240b
#define MT6369_AFE_VOW_TOP_CON2              0x240c
#define MT6369_AFE_VOW_TOP_CON3              0x240d
#define MT6369_AFE_VOW_TOP_CON4              0x240e
#define MT6369_AFE_VOW_TOP_CON5              0x240f
#define MT6369_AFE_VOW_TOP_CON6              0x2410
#define MT6369_AFE_VOW_TOP_CON7              0x2411
#define MT6369_AFE_VOW_TOP_CON8              0x2412
#define MT6369_AFE_VOW_TOP_CON9              0x2413
#define MT6369_AFE_VOW_TOP_MON0              0x2414
#define MT6369_AFE_VOW_VAD_CFG0              0x2415
#define MT6369_AFE_VOW_VAD_CFG1              0x2416
#define MT6369_AFE_VOW_VAD_CFG2              0x2417
#define MT6369_AFE_VOW_VAD_CFG3              0x2418
#define MT6369_AFE_VOW_VAD_CFG4              0x2419
#define MT6369_AFE_VOW_VAD_CFG5              0x241a
#define MT6369_AFE_VOW_VAD_CFG6              0x241b
#define MT6369_AFE_VOW_VAD_CFG7              0x241c
#define MT6369_AFE_VOW_VAD_CFG8              0x241d
#define MT6369_AFE_VOW_VAD_CFG9              0x241e
#define MT6369_AFE_VOW_VAD_CFG10             0x241f
#define MT6369_AFE_VOW_VAD_CFG11             0x2420
#define MT6369_AFE_VOW_VAD_CFG12             0x2421
#define MT6369_AFE_VOW_VAD_CFG13             0x2422
#define MT6369_AFE_VOW_VAD_CFG14             0x2423
#define MT6369_AFE_VOW_VAD_CFG15             0x2424
#define MT6369_AFE_VOW_VAD_CFG16             0x2425
#define MT6369_AFE_VOW_VAD_CFG17             0x2426
#define MT6369_AFE_VOW_VAD_CFG18             0x2427
#define MT6369_AFE_VOW_VAD_CFG19             0x2428
#define MT6369_AFE_VOW_VAD_CFG20             0x2429
#define MT6369_AFE_VOW_VAD_CFG21             0x242a
#define MT6369_AFE_VOW_VAD_CFG22             0x242b
#define MT6369_AFE_VOW_VAD_CFG23             0x242c
#define MT6369_AFE_VOW_VAD_CFG24             0x242d
#define MT6369_AFE_VOW_VAD_MON0              0x242e
#define MT6369_AFE_VOW_VAD_MON1              0x242f
#define MT6369_AFE_VOW_VAD_MON2              0x2430
#define MT6369_AFE_VOW_VAD_MON3              0x2431
#define MT6369_AFE_VOW_VAD_MON4              0x2432
#define MT6369_AFE_VOW_VAD_MON5              0x2433
#define MT6369_AFE_VOW_VAD_MON6              0x2434
#define MT6369_AFE_VOW_VAD_MON7              0x2435
#define MT6369_AFE_VOW_VAD_MON8              0x2436
#define MT6369_AFE_VOW_VAD_MON9              0x2437
#define MT6369_AFE_VOW_VAD_MON10             0x2438
#define MT6369_AFE_VOW_VAD_MON11             0x2439
#define MT6369_AFE_VOW_VAD_MON12             0x243a
#define MT6369_AFE_VOW_VAD_MON13             0x243b
#define MT6369_AFE_VOW_VAD_MON14             0x243c
#define MT6369_AFE_VOW_VAD_MON15             0x243d
#define MT6369_AFE_VOW_VAD_MON16             0x243e
#define MT6369_AFE_VOW_VAD_MON17             0x243f
#define MT6369_AFE_VOW_VAD_MON18             0x2440
#define MT6369_AFE_VOW_VAD_MON19             0x2441
#define MT6369_AFE_VOW_VAD_MON20             0x2442
#define MT6369_AFE_VOW_VAD_MON21             0x2443
#define MT6369_AFE_VOW_VAD_MON22             0x2444
#define MT6369_AFE_VOW_VAD_MON23             0x2445
#define MT6369_AFE_VOW_TGEN_CFG0             0x2446
#define MT6369_AFE_VOW_TGEN_CFG1             0x2447
#define MT6369_AFE_VOW_TGEN_CFG2             0x2448
#define MT6369_AFE_VOW_TGEN_CFG3             0x2449
#define MT6369_AFE_VOW_HPF_CFG0              0x244a
#define MT6369_AFE_VOW_HPF_CFG1              0x244b
#define MT6369_AFE_VOW_HPF_CFG2              0x244c
#define MT6369_AFE_VOW_HPF_CFG3              0x244d
#define MT6369_AUDIO_DIG_3_ANA_ID            0x2480
#define MT6369_AUDIO_DIG_3_DIG_ID            0x2481
#define MT6369_AUDIO_DIG_3_ANA_REV           0x2482
#define MT6369_AUDIO_DIG_3_DIG_REV           0x2483
#define MT6369_AUDIO_DIG_3_DSN_DBI           0x2484
#define MT6369_AUDIO_DIG_3_DSN_ESP           0x2485
#define MT6369_AUDIO_DIG_3_DSN_FPI           0x2486
#define MT6369_AUDIO_DIG_3_DSN_DXI           0x2487
#define MT6369_AFE_NCP_CFG0                  0x2488
#define MT6369_AFE_NCP_CFG1                  0x2489
#define MT6369_AFE_NCP_CFG2                  0x248a
#define MT6369_AFE_NCP_CFG3                  0x248b
#define MT6369_AFE_NCP_CFG4                  0x248c
#define MT6369_AUDNCP_CLKDIV_CON0            0x248d
#define MT6369_AUDNCP_CLKDIV_CON1            0x248e
#define MT6369_AUDNCP_CLKDIV_CON2            0x248f
#define MT6369_AUDNCP_CLKDIV_CON3            0x2490
#define MT6369_AUDNCP_CLKDIV_CON4            0x2491
#define MT6369_AUDENC_ANA_ID                 0x2500
#define MT6369_AUDENC_DIG_ID                 0x2501
#define MT6369_AUDENC_ANA_REV                0x2502
#define MT6369_AUDENC_DIG_REV                0x2503
#define MT6369_AUDENC_DBI                    0x2504
#define MT6369_AUDENC_ESP                    0x2505
#define MT6369_AUDENC_FPI                    0x2506
#define MT6369_AUDENC_DXI                    0x2507
#define MT6369_AUDENC_ANA_CON0               0x2508
#define MT6369_AUDENC_ANA_CON1               0x2509
#define MT6369_AUDENC_ANA_CON2               0x250a
#define MT6369_AUDENC_ANA_CON3               0x250b
#define MT6369_AUDENC_ANA_CON4               0x250c
#define MT6369_AUDENC_ANA_CON5               0x250d
#define MT6369_AUDENC_ANA_CON6               0x250e
#define MT6369_AUDENC_ANA_CON7               0x250f
#define MT6369_AUDENC_ANA_CON8               0x2510
#define MT6369_AUDENC_ANA_CON9               0x2511
#define MT6369_AUDENC_ANA_CON10              0x2512
#define MT6369_AUDENC_ANA_CON11              0x2513
#define MT6369_AUDENC_ANA_CON12              0x2514
#define MT6369_AUDENC_ANA_CON13              0x2515
#define MT6369_AUDENC_ANA_CON14              0x2516
#define MT6369_AUDENC_ANA_CON15              0x2517
#define MT6369_AUDENC_ANA_CON16              0x2518
#define MT6369_AUDENC_ANA_CON17              0x2519
#define MT6369_AUDENC_ANA_CON18              0x251a
#define MT6369_AUDENC_ANA_CON19              0x251b
#define MT6369_AUDENC_ANA_CON20              0x251c
#define MT6369_AUDENC_ANA_CON21              0x251d
#define MT6369_AUDENC_ANA_CON22              0x251e
#define MT6369_AUDENC_ANA_CON23              0x251f
#define MT6369_AUDENC_ANA_CON24              0x2520
#define MT6369_AUDENC_ANA_CON25              0x2521
#define MT6369_AUDENC_ANA_CON26              0x2522
#define MT6369_AUDENC_ANA_CON27              0x2523
#define MT6369_AUDENC_ANA_CON28              0x2524
#define MT6369_AUDENC_ANA_CON29              0x2525
#define MT6369_AUDENC_ANA_CON30              0x2526
#define MT6369_AUDENC_ANA_CON31              0x2527
#define MT6369_AUDENC_ANA_CON32              0x2528
#define MT6369_AUDENC_ANA_CON33              0x2529
#define MT6369_AUDENC_ANA_CON34              0x252a
#define MT6369_VOWPLL_ANA_CON0               0x252b
#define MT6369_VOWPLL_ANA_CON1               0x252c
#define MT6369_VOWPLL_ANA_CON2               0x252d
#define MT6369_VOWPLL_ANA_CON3               0x252e
#define MT6369_VOWPLL_ANA_CON4               0x252f
#define MT6369_VOWPLL_ANA_CON5               0x2530
#define MT6369_VOWPLL_ANA_CON6               0x2531
#define MT6369_VOWPLL_ANA_CON7               0x2532
#define MT6369_VOWPLL_ANA_CON8               0x2533
#define MT6369_AUDDEC_ANA_ID                 0x2580
#define MT6369_AUDDEC_DIG_ID                 0x2581
#define MT6369_AUDDEC_ANA_REV                0x2582
#define MT6369_AUDDEC_DIG_REV                0x2583
#define MT6369_AUDDEC_DBI                    0x2584
#define MT6369_AUDDEC_ESP                    0x2585
#define MT6369_AUDDEC_FPI                    0x2586
#define MT6369_AUDDEC_DXI                    0x2587
#define MT6369_AUDDEC_ANA_CON0               0x2588
#define MT6369_AUDDEC_ANA_CON1               0x2589
#define MT6369_AUDDEC_ANA_CON2               0x258a
#define MT6369_AUDDEC_ANA_CON3               0x258b
#define MT6369_AUDDEC_ANA_CON4               0x258c
#define MT6369_AUDDEC_ANA_CON5               0x258d
#define MT6369_AUDDEC_ANA_CON6               0x258e
#define MT6369_AUDDEC_ANA_CON7               0x258f
#define MT6369_AUDDEC_ANA_CON8               0x2590
#define MT6369_AUDDEC_ANA_CON9               0x2591
#define MT6369_AUDDEC_ANA_CON10              0x2592
#define MT6369_AUDDEC_ANA_CON11              0x2593
#define MT6369_AUDDEC_ANA_CON12              0x2594
#define MT6369_AUDDEC_ANA_CON13              0x2595
#define MT6369_AUDDEC_ANA_CON14              0x2596
#define MT6369_AUDDEC_ANA_CON15              0x2597
#define MT6369_AUDDEC_ANA_CON16              0x2598
#define MT6369_AUDDEC_ANA_CON17              0x2599
#define MT6369_AUDDEC_ANA_CON18              0x259a
#define MT6369_AUDDEC_ANA_CON19              0x259b
#define MT6369_AUDDEC_ANA_CON20              0x259c
#define MT6369_AUDDEC_ANA_CON21              0x259d
#define MT6369_AUDDEC_ANA_CON22              0x259e
#define MT6369_AUDDEC_ANA_CON23              0x259f
#define MT6369_AUDDEC_ANA_CON24              0x25a0
#define MT6369_AUDDEC_ANA_CON25              0x25a1
#define MT6369_AUDDEC_ANA_CON26              0x25a2
#define MT6369_AUDDEC_ANA_CON27              0x25a3
#define MT6369_AUDDEC_ANA_CON28              0x25a4
#define MT6369_AUDZCD_ANA_ID                 0x2600
#define MT6369_AUDZCD_DIG_ID                 0x2601
#define MT6369_AUDZCD_ANA_REV                0x2602
#define MT6369_AUDZCD_DIG_REV                0x2603
#define MT6369_AUDZCD_DSN_DBI                0x2604
#define MT6369_AUDZCD_DSN_ESP                0x2605
#define MT6369_AUDZCD_DSN_FPI                0x2606
#define MT6369_AUDZCD_DSN_DXI                0x2607
#define MT6369_ZCD_CON0                      0x2608
#define MT6369_ZCD_CON1                      0x2609
#define MT6369_ZCD_CON2                      0x260a
#define MT6369_ZCD_CON3                      0x260b
#define MT6369_ZCD_CON4                      0x260c
#define MT6369_ZCD_CON5                      0x260d
#define MT6369_ZCD_CON6                      0x260e
#define MT6369_ZCD_CON7                      0x260f
#define MT6369_ZCD_CON8                      0x2610
#define MT6369_ACCDET_ANA_ID                 0x2680
#define MT6369_ACCDET_DIG_ID                 0x2681
#define MT6369_ACCDET_ANA_REV                0x2682
#define MT6369_ACCDET_DIG_REV                0x2683
#define MT6369_ACCDET_DSN_DBI                0x2684
#define MT6369_ACCDET_DSN_ESP                0x2685
#define MT6369_ACCDET_DSN_FPI                0x2686
#define MT6369_ACCDET_DSN_DXI                0x2687
#define MT6369_ACCDET_CON0                   0x2688
#define MT6369_ACCDET_CON1                   0x2689
#define MT6369_ACCDET_CON2                   0x268a
#define MT6369_ACCDET_CON3                   0x268b
#define MT6369_ACCDET_CON4                   0x268c
#define MT6369_ACCDET_CON5                   0x268d
#define MT6369_ACCDET_CON6                   0x268e
#define MT6369_ACCDET_CON7                   0x268f
#define MT6369_ACCDET_CON8                   0x2690
#define MT6369_ACCDET_CON9                   0x2691
#define MT6369_ACCDET_CON10                  0x2692
#define MT6369_ACCDET_CON11                  0x2693
#define MT6369_ACCDET_CON12                  0x2694
#define MT6369_ACCDET_CON13                  0x2695
#define MT6369_ACCDET_CON14                  0x2696
#define MT6369_ACCDET_CON15                  0x2697
#define MT6369_ACCDET_CON16                  0x2698
#define MT6369_ACCDET_CON17                  0x2699
#define MT6369_ACCDET_CON18                  0x269a
#define MT6369_ACCDET_CON19                  0x269b
#define MT6369_ACCDET_CON20                  0x269c
#define MT6369_ACCDET_CON21                  0x269d
#define MT6369_ACCDET_CON22                  0x269e
#define MT6369_ACCDET_CON23                  0x269f
#define MT6369_ACCDET_CON24                  0x26a0
#define MT6369_ACCDET_CON25                  0x26a1
#define MT6369_ACCDET_CON26                  0x26a2
#define MT6369_ACCDET_CON27                  0x26a3
#define MT6369_ACCDET_CON28                  0x26a4
#define MT6369_ACCDET_CON29                  0x26a5
#define MT6369_ACCDET_CON30                  0x26a6
#define MT6369_ACCDET_CON31                  0x26a7
#define MT6369_ACCDET_CON32                  0x26a8
#define MT6369_ACCDET_CON33                  0x26a9
#define MT6369_ACCDET_CON34                  0x26aa
#define MT6369_ACCDET_CON35                  0x26ab
#define MT6369_ACCDET_CON36                  0x26ac
#define MT6369_ACCDET_CON37                  0x26ad
#define MT6369_ACCDET_CON38                  0x26ae
#define MT6369_ACCDET_CON39                  0x26af
#define MT6369_ACCDET_CON40                  0x26b0
#define MT6369_ACCDET_CON41                  0x26b1
#define MT6369_ACCDET_CON42                  0x26b2
#define MT6369_ACCDET_CON43                  0x26b3
#define MT6369_ACCDET_CON44                  0x26b4
#define MT6369_ACCDET_CON45                  0x26b5
#define MT6369_ACCDET_CON46                  0x26b6
#define MT6369_ACCDET_CON47                  0x26b7
#define MT6369_ACCDET_CON48                  0x26b8
#define MT6369_ACCDET_CON49                  0x26b9
#define MT6369_ACCDET_CON50                  0x26ba
#define MT6369_ACCDET_CON51                  0x26bb
#define MT6369_ACCDET_CON52                  0x26bc
#define MT6369_ACCDET_CON53                  0x26bd
#define MT6369_ACCDET_CON54                  0x26be
#define MT6369_ACCDET_CON55                  0x26bf
#define MT6369_ACCDET_CON56                  0x26c0
#define MT6369_ACCDET_CON57                  0x26c1
#define MT6369_ACCDET_CON58                  0x26c2
#define MT6369_ACCDET_CON59                  0x26c3
#define MT6369_ACCDET_CON60                  0x26c4
#define MT6369_ACCDET_CON61                  0x26c5
#define MT6369_ACCDET_CON62                  0x26c6
#define MT6369_ACCDET_CON63                  0x26c7
#define MT6369_ACCDET_CON64                  0x26c8
#define MT6369_ACCDET_CON65                  0x26c9
#define MT6369_ACCDET_CON66                  0x26ca
#define MT6369_ACCDET_CON67                  0x26cb
#define MT6369_ACCDET_CON68                  0x26cc
#define MT6369_ACCDET_CON69                  0x26cd
#define MT6369_ACCDET_CON70                  0x26ce
#define MT6369_ACCDET_CON71                  0x26cf
#define MT6369_ACCDET_CON72                  0x26d0
#define MT6369_ACCDET_CON73                  0x26d1


/* LDO_VANT18_CON0 */
#define RG_LDO_VANT18_EN_SFT                             0
#define RG_LDO_VANT18_EN_MASK                            0x1
#define RG_LDO_VANT18_EN_MASK_SFT                        (0x1 << 0)
#define RG_LDO_VANT18_LP_SFT                             1
#define RG_LDO_VANT18_LP_MASK                            0x1
#define RG_LDO_VANT18_LP_MASK_SFT                        (0x1 << 1)

/* LDO_VAUD28_CON0 */
#define RG_LDO_VAUD28_EN_SFT                             0
#define RG_LDO_VAUD28_EN_MASK                            0x1
#define RG_LDO_VAUD28_EN_MASK_SFT                        (0x1 << 0)
#define RG_LDO_VAUD28_LP_SFT                             1
#define RG_LDO_VAUD28_LP_MASK                            0x1
#define RG_LDO_VAUD28_LP_MASK_SFT                        (0x1 << 1)

/* AUD_TOP_ANA_ID */
#define AUD_TOP_ANA_ID_SFT                               0
#define AUD_TOP_ANA_ID_MASK                              0xff
#define AUD_TOP_ANA_ID_MASK_SFT                          (0xff << 0)

/* AUD_TOP_DIG_ID */
#define AUD_TOP_DIG_ID_SFT                               0
#define AUD_TOP_DIG_ID_MASK                              0xff
#define AUD_TOP_DIG_ID_MASK_SFT                          (0xff << 0)

/* AUD_TOP_ANA_REV */
#define AUD_TOP_ANA_MINOR_REV_SFT                        0
#define AUD_TOP_ANA_MINOR_REV_MASK                       0xf
#define AUD_TOP_ANA_MINOR_REV_MASK_SFT                   (0xf << 0)
#define AUD_TOP_ANA_MAJOR_REV_SFT                        4
#define AUD_TOP_ANA_MAJOR_REV_MASK                       0xf
#define AUD_TOP_ANA_MAJOR_REV_MASK_SFT                   (0xf << 4)

/* AUD_TOP_DIG_REV */
#define AUD_TOP_DIG_MINOR_REV_SFT                        0
#define AUD_TOP_DIG_MINOR_REV_MASK                       0xf
#define AUD_TOP_DIG_MINOR_REV_MASK_SFT                   (0xf << 0)
#define AUD_TOP_DIG_MAJOR_REV_SFT                        4
#define AUD_TOP_DIG_MAJOR_REV_MASK                       0xf
#define AUD_TOP_DIG_MAJOR_REV_MASK_SFT                   (0xf << 4)

/* AUD_TOP_DSN_DBI */
#define AUD_TOP_DSN_CBS_SFT                              0
#define AUD_TOP_DSN_CBS_MASK                             0x3
#define AUD_TOP_DSN_CBS_MASK_SFT                         (0x3 << 0)
#define AUD_TOP_DSN_BIX_SFT                              2
#define AUD_TOP_DSN_BIX_MASK                             0x3
#define AUD_TOP_DSN_BIX_MASK_SFT                         (0x3 << 2)

/* AUD_TOP_DSN_ESP */
#define AUD_TOP_DSN_ESP_SFT                              0
#define AUD_TOP_DSN_ESP_MASK                             0xff
#define AUD_TOP_DSN_ESP_MASK_SFT                         (0xff << 0)

/* AUD_TOP_DSN_FPI */
#define AUD_TOP_DSN_FPI_SFT                              0
#define AUD_TOP_DSN_FPI_MASK                             0xff
#define AUD_TOP_DSN_FPI_MASK_SFT                         (0xff << 0)

/* AUD_TOP_DSN_DXI */
#define AUD_TOP_DSN_DXI_SFT                              0
#define AUD_TOP_DSN_DXI_MASK                             0xff
#define AUD_TOP_DSN_DXI_MASK_SFT                         (0xff << 0)

/* AUD_TOP_CKPDN_TPM0 */
#define AUD_TOP_CLK_OFFSET_SFT                           0
#define AUD_TOP_CLK_OFFSET_MASK                          0xff
#define AUD_TOP_CLK_OFFSET_MASK_SFT                      (0xff << 0)

/* AUD_TOP_CKPDN_TPM1 */
#define AUD_TOP_RST_OFFSET_SFT                           0
#define AUD_TOP_RST_OFFSET_MASK                          0xff
#define AUD_TOP_RST_OFFSET_MASK_SFT                      (0xff << 0)

/* AUD_TOP_CKPDN_TPM2 */
#define AUD_TOP_INT_OFFSET_SFT                           0
#define AUD_TOP_INT_OFFSET_MASK                          0xff
#define AUD_TOP_INT_OFFSET_MASK_SFT                      (0xff << 0)

/* AUD_TOP_CKPDN_TPM3 */
#define AUD_TOP_INT_LEN_SFT                              0
#define AUD_TOP_INT_LEN_MASK                             0xff
#define AUD_TOP_INT_LEN_MASK_SFT                         (0xff << 0)

/* AUD_TOP_CKPDN_CON0 */
#define RG_PAD_AUD_CLK_MISO_CK_PDN_SFT                   7
#define RG_PAD_AUD_CLK_MISO_CK_PDN_MASK                  0x1
#define RG_PAD_AUD_CLK_MISO_CK_PDN_MASK_SFT              (0x1 << 7)
#define RG_AUDNCP_CK_PDN_SFT                             6
#define RG_AUDNCP_CK_PDN_MASK                            0x1
#define RG_AUDNCP_CK_PDN_MASK_SFT                        (0x1 << 6)
#define RG_ZCD13M_CK_PDN_SFT                             5
#define RG_ZCD13M_CK_PDN_MASK                            0x1
#define RG_ZCD13M_CK_PDN_MASK_SFT                        (0x1 << 5)
#define RG_AUDIF_CK_PDN_SFT                              2
#define RG_AUDIF_CK_PDN_MASK                             0x1
#define RG_AUDIF_CK_PDN_MASK_SFT                         (0x1 << 2)
#define RG_AUD_CK_PDN_SFT                                1
#define RG_AUD_CK_PDN_MASK                               0x1
#define RG_AUD_CK_PDN_MASK_SFT                           (0x1 << 1)
#define RG_ACCDET_CK_PDN_SFT                             0
#define RG_ACCDET_CK_PDN_MASK                            0x1
#define RG_ACCDET_CK_PDN_MASK_SFT                        (0x1 << 0)

/* AUD_TOP_CKPDN_CON0_SET */
#define RG_AUD_TOP_CKPDN_CON1_SET_SFT                    0
#define RG_AUD_TOP_CKPDN_CON1_SET_MASK                   0x3f
#define RG_AUD_TOP_CKPDN_CON1_SET_MASK_SFT               (0x3f << 0)

/* AUD_TOP_CKPDN_CON0_CLR */
#define RG_AUD_TOP_CKPDN_CON1_CLR_SFT                    0
#define RG_AUD_TOP_CKPDN_CON1_CLR_MASK                   0x3f
#define RG_AUD_TOP_CKPDN_CON1_CLR_MASK_SFT               (0x3f << 0)

/* AUD_TOP_CKPDN_CON1 */
#define RG_VOW13M_CK_PDN_SFT                             5
#define RG_VOW13M_CK_PDN_MASK                            0x1
#define RG_VOW13M_CK_PDN_MASK_SFT                        (0x1 << 5)
#define RG_VOW32K_CK_PDN_SFT                             4
#define RG_VOW32K_CK_PDN_MASK                            0x1
#define RG_VOW32K_CK_PDN_MASK_SFT                        (0x1 << 4)
#define RG_AUD_INTRP_CK_PDN_SFT                          0
#define RG_AUD_INTRP_CK_PDN_MASK                         0x1
#define RG_AUD_INTRP_CK_PDN_MASK_SFT                     (0x1 << 0)

/* AUD_TOP_CKPDN_CON1_SET */
#define RG_AUD_TOP_CKPDN_CON0_SET_SFT                    0
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK                   0xff
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK_SFT               (0xff << 0)

/* AUD_TOP_CKPDN_CON1_CLR */
#define RG_AUD_TOP_CKPDN_CON0_CLR_SFT                    0
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK                   0xff
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK_SFT               (0xff << 0)

/* AUD_TOP_CKSEL_CON0 */
#define RG_AUDIF_CK_CKSEL_SFT                            3
#define RG_AUDIF_CK_CKSEL_MASK                           0x1
#define RG_AUDIF_CK_CKSEL_MASK_SFT                       (0x1 << 3)
#define RG_AUD_CK_CKSEL_SFT                              2
#define RG_AUD_CK_CKSEL_MASK                             0x1
#define RG_AUD_CK_CKSEL_MASK_SFT                         (0x1 << 2)

/* AUD_TOP_CKSEL_CON0_SET */
#define RG_AUD_TOP_CKSEL_CON0_SET_SFT                    0
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK                   0xf
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK_SFT               (0xf << 0)

/* AUD_TOP_CKSEL_CON0_CLR */
#define RG_AUD_TOP_CKSEL_CON0_CLR_SFT                    0
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK                   0xf
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK_SFT               (0xf << 0)

/* AUD_TOP_CKTST_CON0 */
#define RG_VOW13M_CK_TSTSEL_SFT                          6
#define RG_VOW13M_CK_TSTSEL_MASK                         0x1
#define RG_VOW13M_CK_TSTSEL_MASK_SFT                     (0x1 << 6)
#define RG_VOW13M_CK_TST_DIS_SFT                         5
#define RG_VOW13M_CK_TST_DIS_MASK                        0x1
#define RG_VOW13M_CK_TST_DIS_MASK_SFT                    (0x1 << 5)
#define RG_AUD26M_CK_TSTSEL_SFT                          4
#define RG_AUD26M_CK_TSTSEL_MASK                         0x1
#define RG_AUD26M_CK_TSTSEL_MASK_SFT                     (0x1 << 4)
#define RG_AUDIF_CK_TSTSEL_SFT                           3
#define RG_AUDIF_CK_TSTSEL_MASK                          0x1
#define RG_AUDIF_CK_TSTSEL_MASK_SFT                      (0x1 << 3)
#define RG_AUD_CK_TSTSEL_SFT                             2
#define RG_AUD_CK_TSTSEL_MASK                            0x1
#define RG_AUD_CK_TSTSEL_MASK_SFT                        (0x1 << 2)
#define RG_RTC32K_CK_TSTSEL_SFT                          1
#define RG_RTC32K_CK_TSTSEL_MASK                         0x1
#define RG_RTC32K_CK_TSTSEL_MASK_SFT                     (0x1 << 1)
#define RG_AUD26M_CK_TST_DIS_SFT                         0
#define RG_AUD26M_CK_TST_DIS_MASK                        0x1
#define RG_AUD26M_CK_TST_DIS_MASK_SFT                    (0x1 << 0)

/* AUD_TOP_CLK_HWEN_CON0 */
#define RG_AUD_INTRP_CK_PDN_HWEN_SFT                     0
#define RG_AUD_INTRP_CK_PDN_HWEN_MASK                    0x1
#define RG_AUD_INTRP_CK_PDN_HWEN_MASK_SFT                (0x1 << 0)

/* AUD_TOP_CLK_HWEN_CON0_SET */
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_SFT            0
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_MASK           0xff
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_MASK_SFT       (0xff << 0)

/* AUD_TOP_CLK_HWEN_CON0_CLR */
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_SFT           0
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_MASK          0xff
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_MASK_SFT      (0xff << 0)

/* AUD_TOP_RST_CON0 */
#define RG_AUDNCP_RST_SFT                                3
#define RG_AUDNCP_RST_MASK                               0x1
#define RG_AUDNCP_RST_MASK_SFT                           (0x1 << 3)
#define RG_ZCD_RST_SFT                                   2
#define RG_ZCD_RST_MASK                                  0x1
#define RG_ZCD_RST_MASK_SFT                              (0x1 << 2)
#define RG_ACCDET_RST_SFT                                1
#define RG_ACCDET_RST_MASK                               0x1
#define RG_ACCDET_RST_MASK_SFT                           (0x1 << 1)
#define RG_AUDIO_RST_SFT                                 0
#define RG_AUDIO_RST_MASK                                0x1
#define RG_AUDIO_RST_MASK_SFT                            (0x1 << 0)

/* AUD_TOP_RST_CON0_SET */
#define RG_AUD_TOP_RST_CON0_SET_SFT                      0
#define RG_AUD_TOP_RST_CON0_SET_MASK                     0xf
#define RG_AUD_TOP_RST_CON0_SET_MASK_SFT                 (0xf << 0)

/* AUD_TOP_RST_CON0_CLR */
#define RG_AUD_TOP_RST_CON0_CLR_SFT                      0
#define RG_AUD_TOP_RST_CON0_CLR_MASK                     0xf
#define RG_AUD_TOP_RST_CON0_CLR_MASK_SFT                 (0xf << 0)

/* AUD_TOP_RST_BANK_CON0 */
#define BANK_AUDZCD_SWRST_SFT                            2
#define BANK_AUDZCD_SWRST_MASK                           0x1
#define BANK_AUDZCD_SWRST_MASK_SFT                       (0x1 << 2)
#define BANK_AUDIO_SWRST_SFT                             1
#define BANK_AUDIO_SWRST_MASK                            0x1
#define BANK_AUDIO_SWRST_MASK_SFT                        (0x1 << 1)
#define BANK_ACCDET_SWRST_SFT                            0
#define BANK_ACCDET_SWRST_MASK                           0x1
#define BANK_ACCDET_SWRST_MASK_SFT                       (0x1 << 0)

/* AUD_TOP_INT_CON0 */
#define RG_INT_EN_AUDIO_SFT                              0
#define RG_INT_EN_AUDIO_MASK                             0x1
#define RG_INT_EN_AUDIO_MASK_SFT                         (0x1 << 0)
#define RG_INT_EN_ACCDET_SFT                             1
#define RG_INT_EN_ACCDET_MASK                            0x1
#define RG_INT_EN_ACCDET_MASK_SFT                        (0x1 << 1)
#define RG_INT_EN_ACCDET_EINT0_SFT                       2
#define RG_INT_EN_ACCDET_EINT0_MASK                      0x1
#define RG_INT_EN_ACCDET_EINT0_MASK_SFT                  (0x1 << 2)
#define RG_INT_EN_ACCDET_EINT1_SFT                       3
#define RG_INT_EN_ACCDET_EINT1_MASK                      0x1
#define RG_INT_EN_ACCDET_EINT1_MASK_SFT                  (0x1 << 3)

/* AUD_TOP_INT_CON0_SET */
#define RG_AUD_INT_CON0_SET_SFT                          0
#define RG_AUD_INT_CON0_SET_MASK                         0xff
#define RG_AUD_INT_CON0_SET_MASK_SFT                     (0xff << 0)

/* AUD_TOP_INT_CON0_CLR */
#define RG_AUD_INT_CON0_CLR_SFT                          0
#define RG_AUD_INT_CON0_CLR_MASK                         0xff
#define RG_AUD_INT_CON0_CLR_MASK_SFT                     (0xff << 0)

/* AUD_TOP_INT_MASK_CON0 */
#define RG_INT_MASK_AUDIO_SFT                            0
#define RG_INT_MASK_AUDIO_MASK                           0x1
#define RG_INT_MASK_AUDIO_MASK_SFT                       (0x1 << 0)
#define RG_INT_MASK_ACCDET_SFT                           1
#define RG_INT_MASK_ACCDET_MASK                          0x1
#define RG_INT_MASK_ACCDET_MASK_SFT                      (0x1 << 1)
#define RG_INT_MASK_ACCDET_EINT0_SFT                     2
#define RG_INT_MASK_ACCDET_EINT0_MASK                    0x1
#define RG_INT_MASK_ACCDET_EINT0_MASK_SFT                (0x1 << 2)
#define RG_INT_MASK_ACCDET_EINT1_SFT                     3
#define RG_INT_MASK_ACCDET_EINT1_MASK                    0x1
#define RG_INT_MASK_ACCDET_EINT1_MASK_SFT                (0x1 << 3)

/* AUD_TOP_INT_MASK_CON0_SET */
#define RG_AUD_INT_MASK_CON0_SET_SFT                     0
#define RG_AUD_INT_MASK_CON0_SET_MASK                    0xff
#define RG_AUD_INT_MASK_CON0_SET_MASK_SFT                (0xff << 0)

/* AUD_TOP_INT_MASK_CON0_CLR */
#define RG_AUD_INT_MASK_CON0_CLR_SFT                     0
#define RG_AUD_INT_MASK_CON0_CLR_MASK                    0xff
#define RG_AUD_INT_MASK_CON0_CLR_MASK_SFT                (0xff << 0)

/* AUD_TOP_INT_STATUS0 */
#define RG_INT_STATUS_AUDIO_SFT                          0
#define RG_INT_STATUS_AUDIO_MASK                         0x1
#define RG_INT_STATUS_AUDIO_MASK_SFT                     (0x1 << 0)
#define RG_INT_STATUS_ACCDET_SFT                         1
#define RG_INT_STATUS_ACCDET_MASK                        0x1
#define RG_INT_STATUS_ACCDET_MASK_SFT                    (0x1 << 1)
#define RG_INT_STATUS_ACCDET_EINT0_SFT                   2
#define RG_INT_STATUS_ACCDET_EINT0_MASK                  0x1
#define RG_INT_STATUS_ACCDET_EINT0_MASK_SFT              (0x1 << 2)
#define RG_INT_STATUS_ACCDET_EINT1_SFT                   3
#define RG_INT_STATUS_ACCDET_EINT1_MASK                  0x1
#define RG_INT_STATUS_ACCDET_EINT1_MASK_SFT              (0x1 << 3)

/* AUD_TOP_INT_RAW_STATUS0 */
#define RG_INT_RAW_STATUS_AUDIO_SFT                      0
#define RG_INT_RAW_STATUS_AUDIO_MASK                     0x1
#define RG_INT_RAW_STATUS_AUDIO_MASK_SFT                 (0x1 << 0)
#define RG_INT_RAW_STATUS_ACCDET_SFT                     1
#define RG_INT_RAW_STATUS_ACCDET_MASK                    0x1
#define RG_INT_RAW_STATUS_ACCDET_MASK_SFT                (0x1 << 1)
#define RG_INT_RAW_STATUS_ACCDET_EINT0_SFT               2
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK              0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK_SFT          (0x1 << 2)
#define RG_INT_RAW_STATUS_ACCDET_EINT1_SFT               3
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK              0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK_SFT          (0x1 << 3)

/* AUD_TOP_INT_MISC_CON0 */
#define RG_AUD_TOP_INT_POLARITY_SFT                      0
#define RG_AUD_TOP_INT_POLARITY_MASK                     0x1
#define RG_AUD_TOP_INT_POLARITY_MASK_SFT                 (0x1 << 0)

/* AUD_TOP_MON_CON0 */
#define RG_AUD_TOP_MON_SEL_SFT                           0
#define RG_AUD_TOP_MON_SEL_MASK                          0x7
#define RG_AUD_TOP_MON_SEL_MASK_SFT                      (0x7 << 0)
#define RG_AUD_CLK_INT_MON_FLAG_EN_SFT                   3
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK                  0x1
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK_SFT              (0x1 << 3)

/* AUD_TOP_MON_CON1 */
#define RG_AUD_CLK_INT_MON_FLAG_SEL_SFT                  0
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK                 0xff
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK_SFT             (0xff << 0)

/* AUDIO_DIG_1_ANA_ID */
#define AUDIO_DIG_1_ANA_ID_SFT                           0
#define AUDIO_DIG_1_ANA_ID_MASK                          0xff
#define AUDIO_DIG_1_ANA_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_1_DIG_ID */
#define AUDIO_DIG_1_DIG_ID_SFT                           0
#define AUDIO_DIG_1_DIG_ID_MASK                          0xff
#define AUDIO_DIG_1_DIG_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_1_ANA_REV */
#define AUDIO_DIG_1_ANA_MINOR_REV_SFT                    0
#define AUDIO_DIG_1_ANA_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_1_ANA_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_1_ANA_MAJOR_REV_SFT                    4
#define AUDIO_DIG_1_ANA_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_1_ANA_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_1_DIG_REV */
#define AUDIO_DIG_1_DIG_MINOR_REV_SFT                    0
#define AUDIO_DIG_1_DIG_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_1_DIG_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_1_DIG_MAJOR_REV_SFT                    4
#define AUDIO_DIG_1_DIG_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_1_DIG_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_1_DSN_DBI */
#define AUDIO_DIG_1_DSN_CBS_SFT                          0
#define AUDIO_DIG_1_DSN_CBS_MASK                         0x3
#define AUDIO_DIG_1_DSN_CBS_MASK_SFT                     (0x3 << 0)
#define AUDIO_DIG_1_DSN_BIX_SFT                          2
#define AUDIO_DIG_1_DSN_BIX_MASK                         0x3
#define AUDIO_DIG_1_DSN_BIX_MASK_SFT                     (0x3 << 2)

/* AUDIO_DIG_1_DSN_ESP */
#define AUDIO_DIG_1_DSN_ESP_SFT                          0
#define AUDIO_DIG_1_DSN_ESP_MASK                         0xff
#define AUDIO_DIG_1_DSN_ESP_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_1_DSN_FPI */
#define AUDIO_DIG_1_DSN_FPI_SFT                          0
#define AUDIO_DIG_1_DSN_FPI_MASK                         0xff
#define AUDIO_DIG_1_DSN_FPI_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_1_DSN_DXI */
#define AUDIO_DIG_1_DSN_DXI_SFT                          0
#define AUDIO_DIG_1_DSN_DXI_MASK                         0xff
#define AUDIO_DIG_1_DSN_DXI_MASK_SFT                     (0xff << 0)

/* AFE_UL_DL_CON0 */
#define UL_LR_SWAP_SFT                                   7
#define UL_LR_SWAP_MASK                                  0x1
#define UL_LR_SWAP_MASK_SFT                              (0x1 << 7)
#define DL_LR_SWAP_SFT                                   6
#define DL_LR_SWAP_MASK                                  0x1
#define DL_LR_SWAP_MASK_SFT                              (0x1 << 6)
#define AFE_ON_SFT                                       0
#define AFE_ON_MASK                                      0x1
#define AFE_ON_MASK_SFT                                  (0x1 << 0)

/* AFE_DL_SRC2_CON0 */
#define DL_2_SRC_ON_TMP_CTL_PRE_SFT                      0
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK                     0x1
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK_SFT                 (0x1 << 0)

/* AFE_UL_SRC_CON0 */
#define C_TWO_DIGITAL_MIC_CTL_SFT                        7
#define C_TWO_DIGITAL_MIC_CTL_MASK                       0x1
#define C_TWO_DIGITAL_MIC_CTL_MASK_SFT                   (0x1 << 7)
#define C_DIGMIC_PHASE_SEL_CH1_CTL_SFT                   4
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK                  0x7
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK_SFT              (0x7 << 4)
#define C_DIGMIC_PHASE_SEL_CH2_CTL_SFT                   0
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK                  0x7
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK_SFT              (0x7 << 0)

/* AFE_UL_SRC_CON1 */
#define PMIC6369_DMIC_LOW_POWER_MODE_CTL_SFT                      6
#define PMIC6369_DMIC_LOW_POWER_MODE_CTL_MASK                     0x3
#define PMIC6369_DMIC_LOW_POWER_MODE_CTL_MASK_SFT                 (0x3 << 6)
#define DIGMIC_4P33M_SEL_CTL_SFT                         5
#define DIGMIC_4P33M_SEL_CTL_MASK                        0x1
#define DIGMIC_4P33M_SEL_CTL_MASK_SFT                    (0x1 << 5)
#define PMIC6369_DIGMIC_3P25M_1P625M_SEL_CTL_SFT                  4
#define PMIC6369_DIGMIC_3P25M_1P625M_SEL_CTL_MASK                 0x1
#define PMIC6369_DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT             (0x1 << 4)
#define UL_LOOP_BACK_MODE_CTL_SFT                        2
#define UL_LOOP_BACK_MODE_CTL_MASK                       0x1
#define UL_LOOP_BACK_MODE_CTL_MASK_SFT                   (0x1 << 2)
#define UL_SDM_3_LEVEL_CTL_SFT                           1
#define UL_SDM_3_LEVEL_CTL_MASK                          0x1
#define UL_SDM_3_LEVEL_CTL_MASK_SFT                      (0x1 << 1)
#define UL_SRC_ON_TMP_CTL_SFT                            0
#define UL_SRC_ON_TMP_CTL_MASK                           0x1
#define UL_SRC_ON_TMP_CTL_MASK_SFT                       (0x1 << 0)

/* AFE_ADDA6_L_SRC_CON0 */
#define ADDA6_C_TWO_DIGITAL_MIC_CTL_SFT                  7
#define ADDA6_C_TWO_DIGITAL_MIC_CTL_MASK                 0x1
#define ADDA6_C_TWO_DIGITAL_MIC_CTL_MASK_SFT             (0x1 << 7)
#define ADDA6_C_DIGMIC_PHASE_SEL_CH1_CTL_SFT             4
#define ADDA6_C_DIGMIC_PHASE_SEL_CH1_CTL_MASK            0x7
#define ADDA6_C_DIGMIC_PHASE_SEL_CH1_CTL_MASK_SFT        (0x7 << 4)
#define ADDA6_C_DIGMIC_PHASE_SEL_CH2_CTL_SFT             0
#define ADDA6_C_DIGMIC_PHASE_SEL_CH2_CTL_MASK            0x7
#define ADDA6_C_DIGMIC_PHASE_SEL_CH2_CTL_MASK_SFT        (0x7 << 0)

/* AFE_ADDA6_UL_SRC_CON1 */
#define ADDA6_DMIC_LOW_POWER_MODE_CTL_SFT                6
#define ADDA6_DMIC_LOW_POWER_MODE_CTL_MASK               0x3
#define ADDA6_DMIC_LOW_POWER_MODE_CTL_MASK_SFT           (0x3 << 6)
#define ADDA6_DIGMIC_4P33M_SEL_CTL_SFT                   5
#define ADDA6_DIGMIC_4P33M_SEL_CTL_MASK                  0x1
#define ADDA6_DIGMIC_4P33M_SEL_CTL_MASK_SFT              (0x1 << 5)
#define ADDA6_DIGMIC_3P25M_1P625M_SEL_CTL_SFT            4
#define ADDA6_DIGMIC_3P25M_1P625M_SEL_CTL_MASK           0x1
#define ADDA6_DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT       (0x1 << 4)
#define ADDA6_UL_LOOP_BACK_MODE_CTL_SFT                  2
#define ADDA6_UL_LOOP_BACK_MODE_CTL_MASK                 0x1
#define ADDA6_UL_LOOP_BACK_MODE_CTL_MASK_SFT             (0x1 << 2)
#define ADDA6_UL_SDM_3_LEVEL_CTL_SFT                     1
#define ADDA6_UL_SDM_3_LEVEL_CTL_MASK                    0x1
#define ADDA6_UL_SDM_3_LEVEL_CTL_MASK_SFT                (0x1 << 1)
#define ADDA6_UL_SRC_ON_TMP_CTL_SFT                      0
#define ADDA6_UL_SRC_ON_TMP_CTL_MASK                     0x1
#define ADDA6_UL_SRC_ON_TMP_CTL_MASK_SFT                 (0x1 << 0)

/* AFE_TOP_CON0 */
#define ADDA6_MTKAIF_SINE_ON_SFT                         4
#define ADDA6_MTKAIF_SINE_ON_MASK                        0x1
#define ADDA6_MTKAIF_SINE_ON_MASK_SFT                    (0x1 << 4)
#define ADDA6_UL_SINE_ON_SFT                             3
#define ADDA6_UL_SINE_ON_MASK                            0x1
#define ADDA6_UL_SINE_ON_MASK_SFT                        (0x1 << 3)
#define MTKAIF_SINE_ON_SFT                               2
#define MTKAIF_SINE_ON_MASK                              0x1
#define MTKAIF_SINE_ON_MASK_SFT                          (0x1 << 2)
#define UL_SINE_ON_SFT                                   1
#define UL_SINE_ON_MASK                                  0x1
#define UL_SINE_ON_MASK_SFT                              (0x1 << 1)
#define DL_SINE_ON_SFT                                   0
#define DL_SINE_ON_MASK                                  0x1
#define DL_SINE_ON_MASK_SFT                              (0x1 << 0)

/* AUDIO_TOP_CON0 */
#define PDN_AFE_CTL_SFT                                  7
#define PDN_AFE_CTL_MASK                                 0x1
#define PDN_AFE_CTL_MASK_SFT                             (0x1 << 7)
#define PDN_DAC_CTL_SFT                                  6
#define PDN_DAC_CTL_MASK                                 0x1
#define PDN_DAC_CTL_MASK_SFT                             (0x1 << 6)
#define PDN_ADC_CTL_SFT                                  5
#define PDN_ADC_CTL_MASK                                 0x1
#define PDN_ADC_CTL_MASK_SFT                             (0x1 << 5)
#define PDN_ADDA6_ADC_CTL_SFT                            4
#define PDN_ADDA6_ADC_CTL_MASK                           0x1
#define PDN_ADDA6_ADC_CTL_MASK_SFT                       (0x1 << 4)
#define PWR_CLK_DIS_CTL_SFT                              2
#define PWR_CLK_DIS_CTL_MASK                             0x1
#define PWR_CLK_DIS_CTL_MASK_SFT                         (0x1 << 2)
#define PDN_AFE_TESTMODEL_CTL_SFT                        1
#define PDN_AFE_TESTMODEL_CTL_MASK                       0x1
#define PDN_AFE_TESTMODEL_CTL_MASK_SFT                   (0x1 << 1)
#define PDN_AFE_DL_PREDIST_CTL_SFT                       0
#define PDN_AFE_DL_PREDIST_CTL_MASK                      0x1
#define PDN_AFE_DL_PREDIST_CTL_MASK_SFT                  (0x1 << 0)

/* AFE_MON_DEBUG0 */
#define AFE_MON_SEL_SFT                                  0
#define AFE_MON_SEL_MASK                                 0xff
#define AFE_MON_SEL_MASK_SFT                             (0xff << 0)

/* AFE_MON_DEBUG1 */
#define AUDIO_SYS_TOP_MON_SWAP_SFT                       6
#define AUDIO_SYS_TOP_MON_SWAP_MASK                      0x3
#define AUDIO_SYS_TOP_MON_SWAP_MASK_SFT                  (0x3 << 6)
#define AUDIO_SYS_TOP_MON_SEL_SFT                        0
#define AUDIO_SYS_TOP_MON_SEL_MASK                       0x1f
#define AUDIO_SYS_TOP_MON_SEL_MASK_SFT                   (0x1f << 0)

/* AFUNC_AUD_CON0 */
#define CCI_SPLT_SCRMB_ON_SFT                            7
#define CCI_SPLT_SCRMB_ON_MASK                           0x1
#define CCI_SPLT_SCRMB_ON_MASK_SFT                       (0x1 << 7)
#define CCI_AUD_IDAC_TEST_EN_SFT                         6
#define CCI_AUD_IDAC_TEST_EN_MASK                        0x1
#define CCI_AUD_IDAC_TEST_EN_MASK_SFT                    (0x1 << 6)
#define CCI_ZERO_PAD_DISABLE_SFT                         5
#define CCI_ZERO_PAD_DISABLE_MASK                        0x1
#define CCI_ZERO_PAD_DISABLE_MASK_SFT                    (0x1 << 5)
#define CCI_AUD_SPLIT_TEST_EN_SFT                        4
#define CCI_AUD_SPLIT_TEST_EN_MASK                       0x1
#define CCI_AUD_SPLIT_TEST_EN_MASK_SFT                   (0x1 << 4)
#define CCI_AUD_SDM_MUTEL_SFT                            3
#define CCI_AUD_SDM_MUTEL_MASK                           0x1
#define CCI_AUD_SDM_MUTEL_MASK_SFT                       (0x1 << 3)
#define CCI_AUD_SDM_MUTER_SFT                            2
#define CCI_AUD_SDM_MUTER_MASK                           0x1
#define CCI_AUD_SDM_MUTER_MASK_SFT                       (0x1 << 2)
#define CCI_AUD_SDM_7BIT_SEL_SFT                         1
#define CCI_AUD_SDM_7BIT_SEL_MASK                        0x1
#define CCI_AUD_SDM_7BIT_SEL_MASK_SFT                    (0x1 << 1)
#define CCI_SCRAMBLER_EN_SFT                             0
#define CCI_SCRAMBLER_EN_MASK                            0x1
#define CCI_SCRAMBLER_EN_MASK_SFT                        (0x1 << 0)

/* AFUNC_AUD_CON1 */
#define CCI_AUD_ANACK_SEL_SFT                            7
#define CCI_AUD_ANACK_SEL_MASK                           0x1
#define CCI_AUD_ANACK_SEL_MASK_SFT                       (0x1 << 7)
#define CCI_AUDIO_FIFO_WPTR_SFT                          4
#define CCI_AUDIO_FIFO_WPTR_MASK                         0x7
#define CCI_AUDIO_FIFO_WPTR_MASK_SFT                     (0x7 << 4)
#define CCI_SCRAMBLER_CG_EN_SFT                          3
#define CCI_SCRAMBLER_CG_EN_MASK                         0x1
#define CCI_SCRAMBLER_CG_EN_MASK_SFT                     (0x1 << 3)
#define CCI_LCH_INV_SFT                                  2
#define CCI_LCH_INV_MASK                                 0x1
#define CCI_LCH_INV_MASK_SFT                             (0x1 << 2)
#define CCI_RAND_EN_SFT                                  1
#define CCI_RAND_EN_MASK                                 0x1
#define CCI_RAND_EN_MASK_SFT                             (0x1 << 1)
#define CCI_SPLT_SCRMB_CLK_ON_SFT                        0
#define CCI_SPLT_SCRMB_CLK_ON_MASK                       0x1
#define CCI_SPLT_SCRMB_CLK_ON_MASK_SFT                   (0x1 << 0)

/* AFUNC_AUD_CON2 */
#define AUD_SDM_TEST_R_SFT                               0
#define AUD_SDM_TEST_R_MASK                              0xff
#define AUD_SDM_TEST_R_MASK_SFT                          (0xff << 0)

/* AFUNC_AUD_CON3 */
#define AUD_SDM_TEST_L_SFT                               0
#define AUD_SDM_TEST_L_MASK                              0xff
#define AUD_SDM_TEST_L_MASK_SFT                          (0xff << 0)

/* AFUNC_AUD_CON4 */
#define CCI_AUD_DAC_ANA_MUTE_SFT                         7
#define CCI_AUD_DAC_ANA_MUTE_MASK                        0x1
#define CCI_AUD_DAC_ANA_MUTE_MASK_SFT                    (0x1 << 7)
#define CCI_AUD_DAC_ANA_RSTB_SEL_SFT                     6
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK                    0x1
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK_SFT                (0x1 << 6)
#define CCI_AUDIO_FIFO_CLKIN_INV_SFT                     4
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK                    0x1
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK_SFT                (0x1 << 4)
#define CCI_AUDIO_FIFO_ENABLE_SFT                        3
#define CCI_AUDIO_FIFO_ENABLE_MASK                       0x1
#define CCI_AUDIO_FIFO_ENABLE_MASK_SFT                   (0x1 << 3)
#define CCI_ACD_MODE_SFT                                 2
#define CCI_ACD_MODE_MASK                                0x1
#define CCI_ACD_MODE_MASK_SFT                            (0x1 << 2)
#define CCI_AFIFO_CLK_PWDB_SFT                           1
#define CCI_AFIFO_CLK_PWDB_MASK                          0x1
#define CCI_AFIFO_CLK_PWDB_MASK_SFT                      (0x1 << 1)
#define CCI_ACD_FUNC_RSTB_SFT                            0
#define CCI_ACD_FUNC_RSTB_MASK                           0x1
#define CCI_ACD_FUNC_RSTB_MASK_SFT                       (0x1 << 0)

/* AFUNC_AUD_CON5 */
#define DIGMIC_TESTCK_SRC_SEL_SFT                        4
#define DIGMIC_TESTCK_SRC_SEL_MASK                       0x7
#define DIGMIC_TESTCK_SRC_SEL_MASK_SFT                   (0x7 << 4)
#define DIGMIC_TESTCK_SEL_SFT                            0
#define DIGMIC_TESTCK_SEL_MASK                           0x1
#define DIGMIC_TESTCK_SEL_MASK_SFT                       (0x1 << 0)

/* AFUNC_AUD_CON6 */
#define SDM_ANA13M_TESTCK_SEL_SFT                        7
#define SDM_ANA13M_TESTCK_SEL_MASK                       0x1
#define SDM_ANA13M_TESTCK_SEL_MASK_SFT                   (0x1 << 7)
#define SDM_ANA13M_TESTCK_SRC_SEL_SFT                    4
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK                   0x7
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK_SFT               (0x7 << 4)
#define SDM_TESTCK_SRC_SEL_SFT                           0
#define SDM_TESTCK_SRC_SEL_MASK                          0x7
#define SDM_TESTCK_SRC_SEL_MASK_SFT                      (0x7 << 0)

/* AFUNC_AUD_CON7 */
#define UL_FIFO_WCLK_INV_SFT                             7
#define UL_FIFO_WCLK_INV_MASK                            0x1
#define UL_FIFO_WCLK_INV_MASK_SFT                        (0x1 << 7)
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_SFT             6
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK            0x1
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK_SFT        (0x1 << 6)
#define UL_FIFO_WDATA_TESTEN_SFT                         5
#define UL_FIFO_WDATA_TESTEN_MASK                        0x1
#define UL_FIFO_WDATA_TESTEN_MASK_SFT                    (0x1 << 5)
#define UL_FIFO_WDATA_TESTSRC_SEL_SFT                    4
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK                   0x1
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK_SFT               (0x1 << 4)
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_SFT                 3
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK                0x1
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK_SFT            (0x1 << 3)
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_SFT             0
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK            0x7
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK_SFT        (0x7 << 0)

/* AFUNC_AUD_CON8 */
#define R_AUD_DAC_NEG_LARGE_MONO_SFT                     0
#define R_AUD_DAC_NEG_LARGE_MONO_MASK                    0xff
#define R_AUD_DAC_NEG_LARGE_MONO_MASK_SFT                (0xff << 0)

/* AFUNC_AUD_CON9 */
#define R_AUD_DAC_POS_LARGE_MONO_SFT                     0
#define R_AUD_DAC_POS_LARGE_MONO_MASK                    0xff
#define R_AUD_DAC_POS_LARGE_MONO_MASK_SFT                (0xff << 0)

/* AFUNC_AUD_CON10 */
#define R_AUD_DAC_POS_TINY_MONO_SFT                      6
#define R_AUD_DAC_POS_TINY_MONO_MASK                     0x3
#define R_AUD_DAC_POS_TINY_MONO_MASK_SFT                 (0x3 << 6)
#define R_AUD_DAC_NEG_TINY_MONO_SFT                      4
#define R_AUD_DAC_NEG_TINY_MONO_MASK                     0x3
#define R_AUD_DAC_NEG_TINY_MONO_MASK_SFT                 (0x3 << 4)
#define R_AUD_DAC_MONO_SEL_SFT                           3
#define R_AUD_DAC_MONO_SEL_MASK                          0x1
#define R_AUD_DAC_MONO_SEL_MASK_SFT                      (0x1 << 3)
#define R_AUD_DAC_3TH_SEL_SFT                            1
#define R_AUD_DAC_3TH_SEL_MASK                           0x1
#define R_AUD_DAC_3TH_SEL_MASK_SFT                       (0x1 << 1)
#define R_AUD_DAC_SW_RSTB_SFT                            0
#define R_AUD_DAC_SW_RSTB_MASK                           0x1
#define R_AUD_DAC_SW_RSTB_MASK_SFT                       (0x1 << 0)

/* AFUNC_AUD_CON11 */
#define R_AUD_DAC_POS_SMALL_MONO_SFT                     4
#define R_AUD_DAC_POS_SMALL_MONO_MASK                    0xf
#define R_AUD_DAC_POS_SMALL_MONO_MASK_SFT                (0xf << 4)
#define R_AUD_DAC_NEG_SMALL_MONO_SFT                     0
#define R_AUD_DAC_NEG_SMALL_MONO_MASK                    0xf
#define R_AUD_DAC_NEG_SMALL_MONO_MASK_SFT                (0xf << 0)

/* AFUNC_AUD_CON12 */
#define UL2_FIFO_DIGMIC_WDATA_TESTSRC_SEL_SFT            6
#define UL2_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK           0x1
#define UL2_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK_SFT       (0x1 << 6)
#define UL2_FIFO_WDATA_TESTEN_SFT                        5
#define UL2_FIFO_WDATA_TESTEN_MASK                       0x1
#define UL2_FIFO_WDATA_TESTEN_MASK_SFT                   (0x1 << 5)
#define UL2_FIFO_WDATA_TESTSRC_SEL_SFT                   4
#define UL2_FIFO_WDATA_TESTSRC_SEL_MASK                  0x1
#define UL2_FIFO_WDATA_TESTSRC_SEL_MASK_SFT              (0x1 << 4)
#define UL2_FIFO_WCLK_6P5M_TESTCK_SEL_SFT                3
#define UL2_FIFO_WCLK_6P5M_TESTCK_SEL_MASK               0x1
#define UL2_FIFO_WCLK_6P5M_TESTCK_SEL_MASK_SFT           (0x1 << 3)
#define UL2_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_SFT            0
#define UL2_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK           0x7
#define UL2_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK_SFT       (0x7 << 0)

/* AFUNC_AUD_CON13 */
#define UL2_DIGMIC_TESTCK_SRC_SEL_SFT                    4
#define UL2_DIGMIC_TESTCK_SRC_SEL_MASK                   0x7
#define UL2_DIGMIC_TESTCK_SRC_SEL_MASK_SFT               (0x7 << 4)
#define UL2_DIGMIC_TESTCK_SEL_SFT                        1
#define UL2_DIGMIC_TESTCK_SEL_MASK                       0x1
#define UL2_DIGMIC_TESTCK_SEL_MASK_SFT                   (0x1 << 1)
#define UL2_FIFO_WCLK_INV_SFT                            0
#define UL2_FIFO_WCLK_INV_MASK                           0x1
#define UL2_FIFO_WCLK_INV_MASK_SFT                       (0x1 << 0)

/* AFUNC_AUD_CON14 */
#define SPLITTER2_DITHER_GAIN_SFT                        4
#define SPLITTER2_DITHER_GAIN_MASK                       0xf
#define SPLITTER2_DITHER_GAIN_MASK_SFT                   (0xf << 4)
#define SPLITTER1_DITHER_GAIN_SFT                        0
#define SPLITTER1_DITHER_GAIN_MASK                       0xf
#define SPLITTER1_DITHER_GAIN_MASK_SFT                   (0xf << 0)

/* AFUNC_AUD_CON15 */
#define SPLITTER2_DITHER_EN_SFT                          1
#define SPLITTER2_DITHER_EN_MASK                         0x1
#define SPLITTER2_DITHER_EN_MASK_SFT                     (0x1 << 1)
#define SPLITTER1_DITHER_EN_SFT                          0
#define SPLITTER1_DITHER_EN_MASK                         0x1
#define SPLITTER1_DITHER_EN_MASK_SFT                     (0x1 << 0)

/* AFUNC_AUD_CON16 */
#define CCI_SPLT_SCRMB_ON_2ND_SFT                        7
#define CCI_SPLT_SCRMB_ON_2ND_MASK                       0x1
#define CCI_SPLT_SCRMB_ON_2ND_MASK_SFT                   (0x1 << 7)
#define CCI_AUD_IDAC_TEST_EN_2ND_SFT                     6
#define CCI_AUD_IDAC_TEST_EN_2ND_MASK                    0x1
#define CCI_AUD_IDAC_TEST_EN_2ND_MASK_SFT                (0x1 << 6)
#define CCI_ZERO_PAD_DISABLE_2ND_SFT                     5
#define CCI_ZERO_PAD_DISABLE_2ND_MASK                    0x1
#define CCI_ZERO_PAD_DISABLE_2ND_MASK_SFT                (0x1 << 5)
#define CCI_AUD_SPLIT_TEST_EN_2ND_SFT                    4
#define CCI_AUD_SPLIT_TEST_EN_2ND_MASK                   0x1
#define CCI_AUD_SPLIT_TEST_EN_2ND_MASK_SFT               (0x1 << 4)
#define CCI_AUD_SDM_MUTEL_2ND_SFT                        3
#define CCI_AUD_SDM_MUTEL_2ND_MASK                       0x1
#define CCI_AUD_SDM_MUTEL_2ND_MASK_SFT                   (0x1 << 3)
#define CCI_AUD_SDM_MUTER_2ND_SFT                        2
#define CCI_AUD_SDM_MUTER_2ND_MASK                       0x1
#define CCI_AUD_SDM_MUTER_2ND_MASK_SFT                   (0x1 << 2)
#define CCI_AUD_SDM_7BIT_SEL_2ND_SFT                     1
#define CCI_AUD_SDM_7BIT_SEL_2ND_MASK                    0x1
#define CCI_AUD_SDM_7BIT_SEL_2ND_MASK_SFT                (0x1 << 1)
#define CCI_SCRAMBLER_EN_2ND_SFT                         0
#define CCI_SCRAMBLER_EN_2ND_MASK                        0x1
#define CCI_SCRAMBLER_EN_2ND_MASK_SFT                    (0x1 << 0)

/* AFUNC_AUD_CON17 */
#define CCI_AUD_ANACK_SEL_2ND_SFT                        7
#define CCI_AUD_ANACK_SEL_2ND_MASK                       0x1
#define CCI_AUD_ANACK_SEL_2ND_MASK_SFT                   (0x1 << 7)
#define CCI_AUDIO_FIFO_WPTR_2ND_SFT                      4
#define CCI_AUDIO_FIFO_WPTR_2ND_MASK                     0x7
#define CCI_AUDIO_FIFO_WPTR_2ND_MASK_SFT                 (0x7 << 4)
#define CCI_SCRAMBLER_CG_EN_2ND_SFT                      3
#define CCI_SCRAMBLER_CG_EN_2ND_MASK                     0x1
#define CCI_SCRAMBLER_CG_EN_2ND_MASK_SFT                 (0x1 << 3)
#define CCI_LCH_INV_2ND_SFT                              2
#define CCI_LCH_INV_2ND_MASK                             0x1
#define CCI_LCH_INV_2ND_MASK_SFT                         (0x1 << 2)
#define CCI_RAND_EN_2ND_SFT                              1
#define CCI_RAND_EN_2ND_MASK                             0x1
#define CCI_RAND_EN_2ND_MASK_SFT                         (0x1 << 1)
#define CCI_SPLT_SCRMB_CLK_ON_2ND_SFT                    0
#define CCI_SPLT_SCRMB_CLK_ON_2ND_MASK                   0x1
#define CCI_SPLT_SCRMB_CLK_ON_2ND_MASK_SFT               (0x1 << 0)

/* AFUNC_AUD_CON18 */
#define AUD_SDM_TEST_R_2ND_SFT                           0
#define AUD_SDM_TEST_R_2ND_MASK                          0xff
#define AUD_SDM_TEST_R_2ND_MASK_SFT                      (0xff << 0)

/* AFUNC_AUD_CON19 */
#define AUD_SDM_TEST_L_2ND_SFT                           0
#define AUD_SDM_TEST_L_2ND_MASK                          0xff
#define AUD_SDM_TEST_L_2ND_MASK_SFT                      (0xff << 0)

/* AFUNC_AUD_CON20 */
#define CCI_AUD_DAC_ANA_MUTE_2ND_SFT                     7
#define CCI_AUD_DAC_ANA_MUTE_2ND_MASK                    0x1
#define CCI_AUD_DAC_ANA_MUTE_2ND_MASK_SFT                (0x1 << 7)
#define CCI_AUD_DAC_ANA_RSTB_SEL_2ND_SFT                 6
#define CCI_AUD_DAC_ANA_RSTB_SEL_2ND_MASK                0x1
#define CCI_AUD_DAC_ANA_RSTB_SEL_2ND_MASK_SFT            (0x1 << 6)
#define CCI_AUDIO_FIFO_CLKIN_INV_2ND_SFT                 4
#define CCI_AUDIO_FIFO_CLKIN_INV_2ND_MASK                0x1
#define CCI_AUDIO_FIFO_CLKIN_INV_2ND_MASK_SFT            (0x1 << 4)
#define CCI_AUDIO_FIFO_ENABLE_2ND_SFT                    3
#define CCI_AUDIO_FIFO_ENABLE_2ND_MASK                   0x1
#define CCI_AUDIO_FIFO_ENABLE_2ND_MASK_SFT               (0x1 << 3)
#define CCI_ACD_MODE_2ND_SFT                             2
#define CCI_ACD_MODE_2ND_MASK                            0x1
#define CCI_ACD_MODE_2ND_MASK_SFT                        (0x1 << 2)
#define CCI_AFIFO_CLK_PWDB_2ND_SFT                       1
#define CCI_AFIFO_CLK_PWDB_2ND_MASK                      0x1
#define CCI_AFIFO_CLK_PWDB_2ND_MASK_SFT                  (0x1 << 1)
#define CCI_ACD_FUNC_RSTB_2ND_SFT                        0
#define CCI_ACD_FUNC_RSTB_2ND_MASK                       0x1
#define CCI_ACD_FUNC_RSTB_2ND_MASK_SFT                   (0x1 << 0)

/* AFUNC_AUD_CON21 */
#define SPLITTER2_DITHER_GAIN_2ND_SFT                    4
#define SPLITTER2_DITHER_GAIN_2ND_MASK                   0xf
#define SPLITTER2_DITHER_GAIN_2ND_MASK_SFT               (0xf << 4)
#define SPLITTER1_DITHER_GAIN_2ND_SFT                    0
#define SPLITTER1_DITHER_GAIN_2ND_MASK                   0xf
#define SPLITTER1_DITHER_GAIN_2ND_MASK_SFT               (0xf << 0)

/* AFUNC_AUD_CON22 */
#define SPLITTER2_DITHER_EN_2ND_SFT                      1
#define SPLITTER2_DITHER_EN_2ND_MASK                     0x1
#define SPLITTER2_DITHER_EN_2ND_MASK_SFT                 (0x1 << 1)
#define SPLITTER1_DITHER_EN_2ND_SFT                      0
#define SPLITTER1_DITHER_EN_2ND_MASK                     0x1
#define SPLITTER1_DITHER_EN_2ND_MASK_SFT                 (0x1 << 0)

/* AFUNC_AUD_MON0 */
#define AUD_SCR_OUT_R_SFT                                0
#define AUD_SCR_OUT_R_MASK                               0xff
#define AUD_SCR_OUT_R_MASK_SFT                           (0xff << 0)

/* AFUNC_AUD_MON1 */
#define AUD_SCR_OUT_L_SFT                                0
#define AUD_SCR_OUT_L_MASK                               0xff
#define AUD_SCR_OUT_L_MASK_SFT                           (0xff << 0)

/* AFUNC_AUD_MON2 */
#define AUD_SCR_OUT_R_2ND_SFT                            0
#define AUD_SCR_OUT_R_2ND_MASK                           0xff
#define AUD_SCR_OUT_R_2ND_MASK_SFT                       (0xff << 0)

/* AFUNC_AUD_MON3 */
#define AUD_SCR_OUT_L_2ND_SFT                            0
#define AUD_SCR_OUT_L_2ND_MASK                           0xff
#define AUD_SCR_OUT_L_2ND_MASK_SFT                       (0xff << 0)

/* AUDRC_TUNE_MON0 */
#define RGS_AUDRCTUNE0READ_SFT                           0
#define RGS_AUDRCTUNE0READ_MASK                          0x1f
#define RGS_AUDRCTUNE0READ_MASK_SFT                      (0x1f << 0)

/* AUDRC_TUNE_MON1 */
#define ASYNC_TEST_OUT_BCK_SFT                           7
#define ASYNC_TEST_OUT_BCK_MASK                          0x1
#define ASYNC_TEST_OUT_BCK_MASK_SFT                      (0x1 << 7)
#define RGS_AUDRCTUNE1READ_SFT                           0
#define RGS_AUDRCTUNE1READ_MASK                          0x1f
#define RGS_AUDRCTUNE1READ_MASK_SFT                      (0x1f << 0)

/* AFE_ADDA_MTKAIF_FIFO_CFG0 */
#define AFE_RESERVED_SFT                                 1
#define AFE_RESERVED_MASK                                0x7f
#define AFE_RESERVED_MASK_SFT                            (0x7f << 1)
#define RG_MTKAIF_RXIF_FIFO_INTEN_SFT                    0
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK                   0x1
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK_SFT               (0x1 << 0)

/* AFE_ADDA_MTKAIF_FIFO_LOG_MON1 */
#define MTKAIF_RXIF_WR_FULL_STATUS_SFT                   1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK                  0x1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK_SFT              (0x1 << 1)
#define MTKAIF_RXIF_RD_EMPTY_STATUS_SFT                  0
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK                 0x1
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK_SFT             (0x1 << 0)

/* AFE_ADDA_MTKAIF_MON0 */
#define MTKAIFRX_FIFO_RD_PTR_SFT                         4
#define MTKAIFRX_FIFO_RD_PTR_MASK                        0xf
#define MTKAIFRX_FIFO_RD_PTR_MASK_SFT                    (0xf << 4)
#define MTKAFIRX_FIFO_RDACTIVE_SFT                       3
#define MTKAFIRX_FIFO_RDACTIVE_MASK                      0x1
#define MTKAFIRX_FIFO_RDACTIVE_MASK_SFT                  (0x1 << 3)
#define MTKAIFRX_FIFO_STARTED_SFT                        2
#define MTKAIFRX_FIFO_STARTED_MASK                       0x1
#define MTKAIFRX_FIFO_STARTED_MASK_SFT                   (0x1 << 2)
#define MTKAIFRX_FIFO_WR_FULL_SFT                        1
#define MTKAIFRX_FIFO_WR_FULL_MASK                       0x1
#define MTKAIFRX_FIFO_WR_FULL_MASK_SFT                   (0x1 << 1)
#define MTKAIFRX_FIFO_RD_EMPTY_SFT                       0
#define MTKAIFRX_FIFO_RD_EMPTY_MASK                      0x1
#define MTKAIFRX_FIFO_RD_EMPTY_MASK_SFT                  (0x1 << 0)

/* AFE_ADDA_MTKAIF_MON1 */
#define MTKAIFTX_V3_SYNC_OUT_SFT                         7
#define MTKAIFTX_V3_SYNC_OUT_MASK                        0x1
#define MTKAIFTX_V3_SYNC_OUT_MASK_SFT                    (0x1 << 7)
#define MTKAIFTX_V3_SDATA_OUT2_SFT                       5
#define MTKAIFTX_V3_SDATA_OUT2_MASK                      0x1
#define MTKAIFTX_V3_SDATA_OUT2_MASK_SFT                  (0x1 << 5)
#define MTKAIFTX_V3_SDATA_OUT1_SFT                       4
#define MTKAIFTX_V3_SDATA_OUT1_MASK                      0x1
#define MTKAIFTX_V3_SDATA_OUT1_MASK_SFT                  (0x1 << 4)
#define MTKAIFRX_FIFO_WR_PTR_SFT                         0
#define MTKAIFRX_FIFO_WR_PTR_MASK                        0xf
#define MTKAIFRX_FIFO_WR_PTR_MASK_SFT                    (0xf << 0)

/* AFE_ADDA_MTKAIF_MON2 */
#define MTKAIF_RXIF_INVALID_CYCLE_SFT                    0
#define MTKAIF_RXIF_INVALID_CYCLE_MASK                   0xff
#define MTKAIF_RXIF_INVALID_CYCLE_MASK_SFT               (0xff << 0)

/* AFE_ADDA_MTKAIF_MON3 */
#define MTKAIFRX_V3_SYNC_IN_SFT                          7
#define MTKAIFRX_V3_SYNC_IN_MASK                         0x1
#define MTKAIFRX_V3_SYNC_IN_MASK_SFT                     (0x1 << 7)
#define MTKAIFRX_V3_SDATA_IN3_SFT                        6
#define MTKAIFRX_V3_SDATA_IN3_MASK                       0x1
#define MTKAIFRX_V3_SDATA_IN3_MASK_SFT                   (0x1 << 6)
#define MTKAIFRX_V3_SDATA_IN2_SFT                        5
#define MTKAIFRX_V3_SDATA_IN2_MASK                       0x1
#define MTKAIFRX_V3_SDATA_IN2_MASK_SFT                   (0x1 << 5)
#define MTKAIFRX_V3_SDATA_IN1_SFT                        4
#define MTKAIFRX_V3_SDATA_IN1_MASK                       0x1
#define MTKAIFRX_V3_SDATA_IN1_MASK_SFT                   (0x1 << 4)
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_SFT                 3
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK                0x1
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK_SFT            (0x1 << 3)
#define MTKAIF_RXIF_INVALID_FLAG_SFT                     0
#define MTKAIF_RXIF_INVALID_FLAG_MASK                    0x1
#define MTKAIF_RXIF_INVALID_FLAG_MASK_SFT                (0x1 << 0)

/* AFE_ADDA_MTKAIF_MON4 */
#define MTKAIF_TXIF_IN_CH1_SFT                           0
#define MTKAIF_TXIF_IN_CH1_MASK                          0xff
#define MTKAIF_TXIF_IN_CH1_MASK_SFT                      (0xff << 0)

/* AFE_ADDA_MTKAIF_MON5 */
#define MTKAIF_TXIF_IN_CH2_SFT                           0
#define MTKAIF_TXIF_IN_CH2_MASK                          0xff
#define MTKAIF_TXIF_IN_CH2_MASK_SFT                      (0xff << 0)

/* AFE_ADDA_MTKAIF_MON6 */
#define ADDA6_MTKAIF_TXIF_IN_CH1_SFT                     0
#define ADDA6_MTKAIF_TXIF_IN_CH1_MASK                    0xff
#define ADDA6_MTKAIF_TXIF_IN_CH1_MASK_SFT                (0xff << 0)

/* AFE_ADDA_MTKAIF_MON7 */
#define ADDA6_MTKAIF_TXIF_IN_CH2_SFT                     0
#define ADDA6_MTKAIF_TXIF_IN_CH2_MASK                    0xff
#define ADDA6_MTKAIF_TXIF_IN_CH2_MASK_SFT                (0xff << 0)

/* AFE_ADDA_MTKAIF_MON8 */
#define MTKAIF_RXIF_OUT_CH1_SFT                          0
#define MTKAIF_RXIF_OUT_CH1_MASK                         0xff
#define MTKAIF_RXIF_OUT_CH1_MASK_SFT                     (0xff << 0)

/* AFE_ADDA_MTKAIF_MON9 */
#define MTKAIF_RXIF_OUT_CH2_SFT                          0
#define MTKAIF_RXIF_OUT_CH2_MASK                         0xff
#define MTKAIF_RXIF_OUT_CH2_MASK_SFT                     (0xff << 0)

/* AFE_ADDA_MTKAIF_MON10 */
#define MTKAIF_RXIF_OUT_CH3_SFT                          0
#define MTKAIF_RXIF_OUT_CH3_MASK                         0xff
#define MTKAIF_RXIF_OUT_CH3_MASK_SFT                     (0xff << 0)

/* AFE_ADDA_MTKAIF_CFG0 */
#define RG_MTKAIF_BYPASS_SRC_MODE_SFT                    6
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK                   0x3
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK_SFT               (0x3 << 6)
#define RG_MTKAIF_BYPASS_SRC_TEST_SFT                    5
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK                   0x1
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK_SFT               (0x1 << 5)
#define RG_MTKAIF_TXIF_PROTOCOL2_SFT                     4
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK                    0x1
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK_SFT                (0x1 << 4)
#define RG_ADDA6_MTKAIF_PMIC_TXIF_8TO5_SFT               3
#define RG_ADDA6_MTKAIF_PMIC_TXIF_8TO5_MASK              0x1
#define RG_ADDA6_MTKAIF_PMIC_TXIF_8TO5_MASK_SFT          (0x1 << 3)
#define RG_MTKAIF_PMIC_TXIF_8TO5_SFT                     2
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK                    0x1
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK_SFT                (0x1 << 2)
#define RG_MTKAIF_LOOPBACK_TEST2_SFT                     1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK                    0x1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK_SFT                (0x1 << 1)
#define RG_MTKAIF_LOOPBACK_TEST1_SFT                     0
#define RG_MTKAIF_LOOPBACK_TEST1_MASK                    0x1
#define RG_MTKAIF_LOOPBACK_TEST1_MASK_SFT                (0x1 << 0)

/* AFE_ADDA_MTKAIF_CFG1 */
#define RG_MTKAIF_RXIF_CLKINV_SFT                        7
#define RG_MTKAIF_RXIF_CLKINV_MASK                       0x1
#define RG_MTKAIF_RXIF_CLKINV_MASK_SFT                   (0x1 << 7)
#define RG_ADDA6_MTKAIF_TXIF_PROTOCOL2_SFT               1
#define RG_ADDA6_MTKAIF_TXIF_PROTOCOL2_MASK              0x1
#define RG_ADDA6_MTKAIF_TXIF_PROTOCOL2_MASK_SFT          (0x1 << 1)
#define RG_MTKAIF_RXIF_PROTOCOL2_SFT                     0
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK                    0x1
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK_SFT                (0x1 << 0)

/* AFE_ADDA_MTKAIF_RX_CFG0 */
#define RG_MTKAIF_RXIF_FIFO_RSP_SFT                      4
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK                     0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK_SFT                 (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_SFT                     3
#define RG_MTKAIF_RXIF_DETECT_ON_MASK                    0x1
#define RG_MTKAIF_RXIF_DETECT_ON_MASK_SFT                (0x1 << 3)
#define RG_MTKAIF_RXIF_DATA_MODE_SFT                     0
#define RG_MTKAIF_RXIF_DATA_MODE_MASK                    0x1
#define RG_MTKAIF_RXIF_DATA_MODE_MASK_SFT                (0x1 << 0)

/* AFE_ADDA_MTKAIF_RX_CFG1 */
#define RG_MTKAIF_RXIF_VOICE_MODE_SFT                    4
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK                   0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK_SFT               (0xf << 4)
#define RG_MTKAIF_RXIF_DATA_BIT_SFT                      0
#define RG_MTKAIF_RXIF_DATA_BIT_MASK                     0x7
#define RG_MTKAIF_RXIF_DATA_BIT_MASK_SFT                 (0x7 << 0)

/* AFE_ADDA_MTKAIF_RX_CFG2 */
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_SFT              4
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK             0xf
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK_SFT         (0xf << 4)
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_SFT          0
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK         0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK_SFT     (0xf << 0)

/* AFE_ADDA_MTKAIF_RX_CFG3 */
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_SFT             4
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK            0xf
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK_SFT        (0xf << 4)
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_SFT      0
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK     0xf
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK_SFT (0xf << 0)

/* AFE_ADDA_MTKAIF_RX_CFG4 */
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_L_SFT              0
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_L_MASK             0xff
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_L_MASK_SFT         (0xff << 0)

/* AFE_ADDA_MTKAIF_RX_CFG5 */
#define RG_MTKAIF_RXIF_P2_INPUT_SEL_SFT                  7
#define RG_MTKAIF_RXIF_P2_INPUT_SEL_MASK                 0x1
#define RG_MTKAIF_RXIF_P2_INPUT_SEL_MASK_SFT             (0x1 << 7)
#define RG_MTKAIF_RXIF_SYNC_WORD2_DISABLE_SFT            6
#define RG_MTKAIF_RXIF_SYNC_WORD2_DISABLE_MASK           0x1
#define RG_MTKAIF_RXIF_SYNC_WORD2_DISABLE_MASK_SFT       (0x1 << 6)
#define RG_MTKAIF_RXIF_SYNC_WORD1_DISABLE_SFT            5
#define RG_MTKAIF_RXIF_SYNC_WORD1_DISABLE_MASK           0x1
#define RG_MTKAIF_RXIF_SYNC_WORD1_DISABLE_MASK_SFT       (0x1 << 5)
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_SFT               4
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK              0x1
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK_SFT          (0x1 << 4)
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_H_SFT              0
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_H_MASK             0xf
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_H_MASK_SFT         (0xf << 0)

/* AFE_ADDA_MTKAIF_RX_CFG6 */
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_SFT              7
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_MASK             0x1
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_MASK_SFT         (0x1 << 7)
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_SFT            4
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK           0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK_SFT       (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_SFT           3
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK          0x1
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK_SFT      (0x1 << 3)

/* AFE_ADDA_MTKAIF_SYNCWORD_CFG0 */
#define RG_MTKAIF_RX_SYNC_WORD2_SFT                      4
#define RG_MTKAIF_RX_SYNC_WORD2_MASK                     0x7
#define RG_MTKAIF_RX_SYNC_WORD2_MASK_SFT                 (0x7 << 4)
#define RG_MTKAIF_RX_SYNC_WORD1_SFT                      0
#define RG_MTKAIF_RX_SYNC_WORD1_MASK                     0x7
#define RG_MTKAIF_RX_SYNC_WORD1_MASK_SFT                 (0x7 << 0)

/* AFE_ADDA_MTKAIF_SYNCWORD_CFG1 */
#define RG_ADDA_MTKAIF_TX_SYNC_WORD2_SFT                 4
#define RG_ADDA_MTKAIF_TX_SYNC_WORD2_MASK                0x7
#define RG_ADDA_MTKAIF_TX_SYNC_WORD2_MASK_SFT            (0x7 << 4)
#define RG_ADDA_MTKAIF_TX_SYNC_WORD1_SFT                 0
#define RG_ADDA_MTKAIF_TX_SYNC_WORD1_MASK                0x7
#define RG_ADDA_MTKAIF_TX_SYNC_WORD1_MASK_SFT            (0x7 << 0)

/* AFE_ADDA_MTKAIF_SYNCWORD_CFG2 */
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD2_SFT                4
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD2_MASK               0x7
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD2_MASK_SFT           (0x7 << 4)
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD1_SFT                0
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD1_MASK               0x7
#define RG_ADDA6_MTKAIF_TX_SYNC_WORD1_MASK_SFT           (0x7 << 0)

/* AFE_SGEN_CFG0 */
#define PMIC6369_C_DAC_EN_CTL_SFT                                 7
#define PMIC6369_C_DAC_EN_CTL_MASK                                0x1
#define PMIC6369_C_DAC_EN_CTL_MASK_SFT                            (0x1 << 7)
#define PMIC6369_C_MUTE_SW_CTL_SFT                                6
#define PMIC6369_C_MUTE_SW_CTL_MASK                               0x1
#define PMIC6369_C_MUTE_SW_CTL_MASK_SFT                           (0x1 << 6)
#define R_AUD_SDM_MUTE_L_SFT                             5
#define R_AUD_SDM_MUTE_L_MASK                            0x1
#define R_AUD_SDM_MUTE_L_MASK_SFT                        (0x1 << 5)
#define R_AUD_SDM_MUTE_R_SFT                             4
#define R_AUD_SDM_MUTE_R_MASK                            0x1
#define R_AUD_SDM_MUTE_R_MASK_SFT                        (0x1 << 4)
#define R_AUD_SDM_MUTE_L_2ND_SFT                         3
#define R_AUD_SDM_MUTE_L_2ND_MASK                        0x1
#define R_AUD_SDM_MUTE_L_2ND_MASK_SFT                    (0x1 << 3)
#define R_AUD_SDM_MUTE_R_2ND_SFT                         2
#define R_AUD_SDM_MUTE_R_2ND_MASK                        0x1
#define R_AUD_SDM_MUTE_R_2ND_MASK_SFT                    (0x1 << 2)

/* AFE_SGEN_CFG1 */
#define PMIC6369_C_AMP_DIV_CH1_CTL_SFT                            0
#define PMIC6369_C_AMP_DIV_CH1_CTL_MASK                           0xf
#define PMIC6369_C_AMP_DIV_CH1_CTL_MASK_SFT                       (0xf << 0)

/* AFE_SGEN_CFG2 */
#define C_SGEN_RCH_INV_5BIT_SFT                          7
#define C_SGEN_RCH_INV_5BIT_MASK                         0x1
#define C_SGEN_RCH_INV_5BIT_MASK_SFT                     (0x1 << 7)
#define C_SGEN_RCH_INV_8BIT_SFT                          6
#define C_SGEN_RCH_INV_8BIT_MASK                         0x1
#define C_SGEN_RCH_INV_8BIT_MASK_SFT                     (0x1 << 6)
#define PMIC6369_C_FREQ_DIV_CH1_CTL_SFT                           0
#define PMIC6369_C_FREQ_DIV_CH1_CTL_MASK                          0x1f
#define PMIC6369_C_FREQ_DIV_CH1_CTL_MASK_SFT                      (0x1f << 0)

/* AFE_ADC_ASYNC_FIFO_CFG0 */
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_SFT                 5
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK                0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK_SFT            (0x1 << 5)
#define RG_UL_ASYNC_FIFO_SOFT_RST_SFT                    4
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK                   0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK_SFT               (0x1 << 4)
#define RG_AMIC_UL_ADC_CLK_SEL_SFT                       1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK                      0x1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK_SFT                  (0x1 << 1)

/* AFE_ADC_ASYNC_FIFO_CFG1 */
#define RG_UL2_ASYNC_FIFO_SOFT_RST_EN_SFT                5
#define RG_UL2_ASYNC_FIFO_SOFT_RST_EN_MASK               0x1
#define RG_UL2_ASYNC_FIFO_SOFT_RST_EN_MASK_SFT           (0x1 << 5)
#define RG_UL2_ASYNC_FIFO_SOFT_RST_SFT                   4
#define RG_UL2_ASYNC_FIFO_SOFT_RST_MASK                  0x1
#define RG_UL2_ASYNC_FIFO_SOFT_RST_MASK_SFT              (0x1 << 4)

/* AFE_DCCLK_CFG0 */
#define DCCLK_DIV_H_SFT                                  5
#define DCCLK_DIV_H_MASK                                 0x7
#define DCCLK_DIV_H_MASK_SFT                             (0x7 << 5)
#define DCCLK_INV_SFT                                    4
#define DCCLK_INV_MASK                                   0x1
#define DCCLK_INV_MASK_SFT                               (0x1 << 4)
#define DCCLK_REF_CK_SEL_SFT                             2
#define DCCLK_REF_CK_SEL_MASK                            0x3
#define DCCLK_REF_CK_SEL_MASK_SFT                        (0x3 << 2)
#define DCCLK_PDN_SFT                                    1
#define DCCLK_PDN_MASK                                   0x1
#define DCCLK_PDN_MASK_SFT                               (0x1 << 1)
#define DCCLK_GEN_ON_SFT                                 0
#define DCCLK_GEN_ON_MASK                                0x1
#define DCCLK_GEN_ON_MASK_SFT                            (0x1 << 0)

/* AFE_DCCLK_CFG1 */
#define DCCLK_DIV_L_SFT                                  0
#define DCCLK_DIV_L_MASK                                 0xff
#define DCCLK_DIV_L_MASK_SFT                             (0xff << 0)

/* AFE_DCCLK_CFG2 */
#define RESYNC_SRC_SEL_SFT                               6
#define RESYNC_SRC_SEL_MASK                              0x3
#define RESYNC_SRC_SEL_MASK_SFT                          (0x3 << 6)
#define RESYNC_SRC_CK_INV_SFT                            5
#define RESYNC_SRC_CK_INV_MASK                           0x1
#define RESYNC_SRC_CK_INV_MASK_SFT                       (0x1 << 5)
#define DCCLK_RESYNC_BYPASS_SFT                          4
#define DCCLK_RESYNC_BYPASS_MASK                         0x1
#define DCCLK_RESYNC_BYPASS_MASK_SFT                     (0x1 << 4)
#define DCCLK_PHASE_SEL_SFT                              0
#define DCCLK_PHASE_SEL_MASK                             0xf
#define DCCLK_PHASE_SEL_MASK_SFT                         (0xf << 0)

/* AUDIO_DIG_CFG0 */
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT             7
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK            0x1
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT        (0x1 << 7)
#define RG_AUD_PAD_TOP_PHASE_MODE_SFT                    0
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK                   0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK_SFT               (0x7f << 0)

/* AUDIO_DIG_CFG1 */
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT            7
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK           0x1
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT       (0x1 << 7)
#define RG_AUD_PAD_TOP_PHASE_MODE2_SFT                   0
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK                  0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK_SFT              (0x7f << 0)

/* AUDIO_DIG_CFG2 */
#define RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_SFT            7
#define RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_MASK           0x1
#define RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_MASK_SFT       (0x1 << 7)
#define RG_AUD_PAD_TOP_PHASE_MODE3_SFT                   0
#define RG_AUD_PAD_TOP_PHASE_MODE3_MASK                  0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE3_MASK_SFT              (0x7f << 0)

/* AFE_AUD_PAD_TOP */
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_SFT                   4
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK                  0x7
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK_SFT              (0x7 << 4)
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_SFT          1
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK         0x1
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK_SFT     (0x1 << 1)
#define RG_AUD_PAD_TOP_TX_FIFO_ON_SFT                    0
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK                   0x1
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK_SFT               (0x1 << 0)

/* AFE_AUD_PAD_TOP_MON0 */
#define PAD_AUD_DAT_MISO3_SFT                            2
#define PAD_AUD_DAT_MISO3_MASK                           0x1
#define PAD_AUD_DAT_MISO3_MASK_SFT                       (0x1 << 2)
#define PAD_AUD_DAT_MISO2_SFT                            1
#define PAD_AUD_DAT_MISO2_MASK                           0x1
#define PAD_AUD_DAT_MISO2_MASK_SFT                       (0x1 << 1)
#define PAD_AUD_DAT_MISO1_SFT                            0
#define PAD_AUD_DAT_MISO1_MASK                           0x1
#define PAD_AUD_DAT_MISO1_MASK_SFT                       (0x1 << 0)

/* AFE_AUD_PAD_TOP_MON1 */
#define MTKAIFTX_FIFO_RD_PTR_SFT                         4
#define MTKAIFTX_FIFO_RD_PTR_MASK                        0xf
#define MTKAIFTX_FIFO_RD_PTR_MASK_SFT                    (0xf << 4)
#define MTKAFITX_FIFO_RDACTIVE_SFT                       3
#define MTKAFITX_FIFO_RDACTIVE_MASK                      0x1
#define MTKAFITX_FIFO_RDACTIVE_MASK_SFT                  (0x1 << 3)
#define MTKAIFTX_FIFO_STARTED_SFT                        2
#define MTKAIFTX_FIFO_STARTED_MASK                       0x1
#define MTKAIFTX_FIFO_STARTED_MASK_SFT                   (0x1 << 2)
#define MTKAIFTX_FIFO_WR_FULL_SFT                        1
#define MTKAIFTX_FIFO_WR_FULL_MASK                       0x1
#define MTKAIFTX_FIFO_WR_FULL_MASK_SFT                   (0x1 << 1)
#define MTKAIFTX_FIFO_RD_EMPTY_SFT                       0
#define MTKAIFTX_FIFO_RD_EMPTY_MASK                      0x1
#define MTKAIFTX_FIFO_RD_EMPTY_MASK_SFT                  (0x1 << 0)

/* AFE_AUD_PAD_TOP_MON2 */
#define MTKAIFTX_FIFO_WR_PTR_SFT                         0
#define MTKAIFTX_FIFO_WR_PTR_MASK                        0xf
#define MTKAIFTX_FIFO_WR_PTR_MASK_SFT                    (0xf << 0)

/* AFE_DL_NLE_CFG */
#define NLE_RCH_HPGAIN_SEL_SFT                           5
#define NLE_RCH_HPGAIN_SEL_MASK                          0x1
#define NLE_RCH_HPGAIN_SEL_MASK_SFT                      (0x1 << 5)
#define NLE_RCH_CH_SEL_SFT                               4
#define NLE_RCH_CH_SEL_MASK                              0x1
#define NLE_RCH_CH_SEL_MASK_SFT                          (0x1 << 4)
#define NLE_RCH_ON_SFT                                   3
#define NLE_RCH_ON_MASK                                  0x1
#define NLE_RCH_ON_MASK_SFT                              (0x1 << 3)
#define NLE_LCH_HPGAIN_SEL_SFT                           2
#define NLE_LCH_HPGAIN_SEL_MASK                          0x1
#define NLE_LCH_HPGAIN_SEL_MASK_SFT                      (0x1 << 2)
#define NLE_LCH_CH_SEL_SFT                               1
#define NLE_LCH_CH_SEL_MASK                              0x1
#define NLE_LCH_CH_SEL_MASK_SFT                          (0x1 << 1)
#define NLE_LCH_ON_SFT                                   0
#define NLE_LCH_ON_MASK                                  0x1
#define NLE_LCH_ON_MASK_SFT                              (0x1 << 0)

/* AFE_DL_NLE_MON0 */
#define RCH_VALID_SFT                                    6
#define RCH_VALID_MASK                                   0x1
#define RCH_VALID_MASK_SFT                               (0x1 << 6)
#define RCH_CH_INFO_SFT                                  5
#define RCH_CH_INFO_MASK                                 0x1
#define RCH_CH_INFO_MASK_SFT                             (0x1 << 5)
#define RCH_GAIN_SFT                                     0
#define RCH_GAIN_MASK                                    0x1f
#define RCH_GAIN_MASK_SFT                                (0x1f << 0)

/* AFE_DL_NLE_MON1 */
#define LCH_VALID_SFT                                    6
#define LCH_VALID_MASK                                   0x1
#define LCH_VALID_MASK_SFT                               (0x1 << 6)
#define LCH_CH_INFO_SFT                                  5
#define LCH_CH_INFO_MASK                                 0x1
#define LCH_CH_INFO_MASK_SFT                             (0x1 << 5)
#define LCH_GAIN_SFT                                     0
#define LCH_GAIN_MASK                                    0x1f
#define LCH_GAIN_MASK_SFT                                (0x1f << 0)

/* AFE_CG_EN_MON */
#define CK_CG_EN_MON_SFT                                 0
#define CK_CG_EN_MON_MASK                                0x3f
#define CK_CG_EN_MON_MASK_SFT                            (0x3f << 0)

/* AFE_MIC_ARRAY_CFG0 */
#define RG_AMIC_ADC3_SOURCE_SEL_SFT                      6
#define RG_AMIC_ADC3_SOURCE_SEL_MASK                     0x3
#define RG_AMIC_ADC3_SOURCE_SEL_MASK_SFT                 (0x3 << 6)
#define RG_DMIC_ADC1_SOURCE_SEL_SFT                      4
#define RG_DMIC_ADC1_SOURCE_SEL_MASK                     0x3
#define RG_DMIC_ADC1_SOURCE_SEL_MASK_SFT                 (0x3 << 4)
#define RG_DMIC_ADC2_SOURCE_SEL_SFT                      2
#define RG_DMIC_ADC2_SOURCE_SEL_MASK                     0x3
#define RG_DMIC_ADC2_SOURCE_SEL_MASK_SFT                 (0x3 << 2)
#define RG_DMIC_ADC3_SOURCE_SEL_SFT                      0
#define RG_DMIC_ADC3_SOURCE_SEL_MASK                     0x3
#define RG_DMIC_ADC3_SOURCE_SEL_MASK_SFT                 (0x3 << 0)

/* AFE_MIC_ARRAY_CFG1 */
#define RG_AMIC_ADC1_SOURCE_SEL_SFT                      2
#define RG_AMIC_ADC1_SOURCE_SEL_MASK                     0x3
#define RG_AMIC_ADC1_SOURCE_SEL_MASK_SFT                 (0x3 << 2)
#define RG_AMIC_ADC2_SOURCE_SEL_SFT                      0
#define RG_AMIC_ADC2_SOURCE_SEL_MASK                     0x3
#define RG_AMIC_ADC2_SOURCE_SEL_MASK_SFT                 (0x3 << 0)

/* AFE_CHOP_CFG0 */
#define RG_CHOP_DIV_SEL_SFT                              2
#define RG_CHOP_DIV_SEL_MASK                             0x1f
#define RG_CHOP_DIV_SEL_MASK_SFT                         (0x1f << 2)
#define RG_CHOP_DIV_EN_SFT                               1
#define RG_CHOP_DIV_EN_MASK                              0x1
#define RG_CHOP_DIV_EN_MASK_SFT                          (0x1 << 1)
#define RG_CHOP_CK_EN_SFT                                0
#define RG_CHOP_CK_EN_MASK                               0x1
#define RG_CHOP_CK_EN_MASK_SFT                           (0x1 << 0)

/* AFE_MTKAIF_MUX_CFG0 */
#define RG_ADDA_EN_SEL_SFT                               4
#define RG_ADDA_EN_SEL_MASK                              0x1
#define RG_ADDA_EN_SEL_MASK_SFT                          (0x1 << 4)
#define RG_ADDA_CH2_SEL_SFT                              2
#define RG_ADDA_CH2_SEL_MASK                             0x3
#define RG_ADDA_CH2_SEL_MASK_SFT                         (0x3 << 2)
#define RG_ADDA_CH1_SEL_SFT                              0
#define RG_ADDA_CH1_SEL_MASK                             0x3
#define RG_ADDA_CH1_SEL_MASK_SFT                         (0x3 << 0)

/* AFE_MTKAIF_MUX_CFG1 */
#define RG_ADDA6_EN_SEL_SFT                              4
#define RG_ADDA6_EN_SEL_MASK                             0x1
#define RG_ADDA6_EN_SEL_MASK_SFT                         (0x1 << 4)
#define RG_ADDA6_CH2_SEL_SFT                             2
#define RG_ADDA6_CH2_SEL_MASK                            0x3
#define RG_ADDA6_CH2_SEL_MASK_SFT                        (0x3 << 2)
#define RG_ADDA6_CH1_SEL_SFT                             0
#define RG_ADDA6_CH1_SEL_MASK                            0x3
#define RG_ADDA6_CH1_SEL_MASK_SFT                        (0x3 << 0)

/* AUDIO_DIG_2_ANA_ID */
#define AUDIO_DIG_2_ANA_ID_SFT                           0
#define AUDIO_DIG_2_ANA_ID_MASK                          0xff
#define AUDIO_DIG_2_ANA_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_2_DIG_ID */
#define AUDIO_DIG_2_DIG_ID_SFT                           0
#define AUDIO_DIG_2_DIG_ID_MASK                          0xff
#define AUDIO_DIG_2_DIG_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_2_ANA_REV */
#define AUDIO_DIG_2_ANA_MINOR_REV_SFT                    0
#define AUDIO_DIG_2_ANA_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_2_ANA_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_2_ANA_MAJOR_REV_SFT                    4
#define AUDIO_DIG_2_ANA_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_2_ANA_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_2_DIG_REV */
#define AUDIO_DIG_2_DIG_MINOR_REV_SFT                    0
#define AUDIO_DIG_2_DIG_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_2_DIG_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_2_DIG_MAJOR_REV_SFT                    4
#define AUDIO_DIG_2_DIG_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_2_DIG_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_2_DSN_DBI */
#define AUDIO_DIG_2_DSN_CBS_SFT                          0
#define AUDIO_DIG_2_DSN_CBS_MASK                         0x3
#define AUDIO_DIG_2_DSN_CBS_MASK_SFT                     (0x3 << 0)
#define AUDIO_DIG_2_DSN_BIX_SFT                          2
#define AUDIO_DIG_2_DSN_BIX_MASK                         0x3
#define AUDIO_DIG_2_DSN_BIX_MASK_SFT                     (0x3 << 2)

/* AUDIO_DIG_2_DSN_ESP */
#define AUDIO_DIG_2_DSN_ESP_SFT                          0
#define AUDIO_DIG_2_DSN_ESP_MASK                         0xff
#define AUDIO_DIG_2_DSN_ESP_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_2_DSN_FPI */
#define AUDIO_DIG_2_DSN_FPI_SFT                          0
#define AUDIO_DIG_2_DSN_FPI_MASK                         0xff
#define AUDIO_DIG_2_DSN_FPI_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_2_DSN_DXI */
#define AUDIO_DIG_2_DSN_DXI_SFT                          0
#define AUDIO_DIG_2_DSN_DXI_MASK                         0xff
#define AUDIO_DIG_2_DSN_DXI_MASK_SFT                     (0xff << 0)

/* AFE_PMIC_NEWIF_CFG0 */
#define RG_UP8X_SYNC_WORD_L_SFT                          0
#define RG_UP8X_SYNC_WORD_L_MASK                         0xff
#define RG_UP8X_SYNC_WORD_L_MASK_SFT                     (0xff << 0)

/* AFE_PMIC_NEWIF_CFG1 */
#define RG_UP8X_SYNC_WORD_H_SFT                          0
#define RG_UP8X_SYNC_WORD_H_MASK                         0xff
#define RG_UP8X_SYNC_WORD_H_MASK_SFT                     (0xff << 0)

/* AFE_VOW_TOP_CON0 */
#define VOW_INTR_SW_MODE_SFT                             7
#define VOW_INTR_SW_MODE_MASK                            0x1
#define VOW_INTR_SW_MODE_MASK_SFT                        (0x1 << 7)
#define VOW_INTR_SW_VAL_SFT                              6
#define VOW_INTR_SW_VAL_MASK                             0x1
#define VOW_INTR_SW_VAL_MASK_SFT                         (0x1 << 6)
#define RG_VOW_INTR_MODE_SEL_SFT                         2
#define RG_VOW_INTR_MODE_SEL_MASK                        0x3
#define RG_VOW_INTR_MODE_SEL_MASK_SFT                    (0x3 << 2)

/* AFE_VOW_TOP_CON1 */
#define PDN_VOW_SFT                                      7
#define PDN_VOW_MASK                                     0x1
#define PDN_VOW_MASK_SFT                                 (0x1 << 7)
#define VOW_DMIC_CK_SEL_SFT                              5
#define VOW_DMIC_CK_SEL_MASK                             0x3
#define VOW_DMIC_CK_SEL_MASK_SFT                         (0x3 << 5)
#define MAIN_DMIC_CK_VOW_SEL_SFT                         4
#define MAIN_DMIC_CK_VOW_SEL_MASK                        0x1
#define MAIN_DMIC_CK_VOW_SEL_MASK_SFT                    (0x1 << 4)
#define VOW_CIC_MODE_SEL_SFT                             2
#define VOW_CIC_MODE_SEL_MASK                            0x3
#define VOW_CIC_MODE_SEL_MASK_SFT                        (0x3 << 2)
#define VOW_SDM_3_LEVEL_SFT                              1
#define VOW_SDM_3_LEVEL_MASK                             0x1
#define VOW_SDM_3_LEVEL_MASK_SFT                         (0x1 << 1)
#define VOW_LOOP_BACK_MODE_SFT                           0
#define VOW_LOOP_BACK_MODE_MASK                          0x1
#define VOW_LOOP_BACK_MODE_MASK_SFT                      (0x1 << 0)

/* AFE_VOW_TOP_CON2 */
#define VOW_ADC_CK_PDN_CH1_SFT                           7
#define VOW_ADC_CK_PDN_CH1_MASK                          0x1
#define VOW_ADC_CK_PDN_CH1_MASK_SFT                      (0x1 << 7)
#define VOW_CK_PDN_CH1_SFT                               6
#define VOW_CK_PDN_CH1_MASK                              0x1
#define VOW_CK_PDN_CH1_MASK_SFT                          (0x1 << 6)
#define VOW_ADC_CLK_INV_CH1_SFT                          5
#define VOW_ADC_CLK_INV_CH1_MASK                         0x1
#define VOW_ADC_CLK_INV_CH1_MASK_SFT                     (0x1 << 5)
#define VOW_INTR_SOURCE_SEL_CH1_SFT                      4
#define VOW_INTR_SOURCE_SEL_CH1_MASK                     0x1
#define VOW_INTR_SOURCE_SEL_CH1_MASK_SFT                 (0x1 << 4)
#define VOW_INTR_CLR_CH1_SFT                             3
#define VOW_INTR_CLR_CH1_MASK                            0x1
#define VOW_INTR_CLR_CH1_MASK_SFT                        (0x1 << 3)
#define S_N_VALUE_RST_CH1_SFT                            2
#define S_N_VALUE_RST_CH1_MASK                           0x1
#define S_N_VALUE_RST_CH1_MASK_SFT                       (0x1 << 2)
#define SAMPLE_BASE_MODE_CH1_SFT                         1
#define SAMPLE_BASE_MODE_CH1_MASK                        0x1
#define SAMPLE_BASE_MODE_CH1_MASK_SFT                    (0x1 << 1)
#define VOW_ON_CH1_SFT                                   0
#define VOW_ON_CH1_MASK                                  0x1
#define VOW_ON_CH1_MASK_SFT                              (0x1 << 0)

/* AFE_VOW_TOP_CON3 */
#define VOW_DMIC0_CK_PDN_SFT                             7
#define VOW_DMIC0_CK_PDN_MASK                            0x1
#define VOW_DMIC0_CK_PDN_MASK_SFT                        (0x1 << 7)
#define VOW_DIGMIC_ON_CH1_SFT                            6
#define VOW_DIGMIC_ON_CH1_MASK                           0x1
#define VOW_DIGMIC_ON_CH1_MASK_SFT                       (0x1 << 6)
#define VOW_CK_DIV_RST_CH1_SFT                           5
#define VOW_CK_DIV_RST_CH1_MASK                          0x1
#define VOW_CK_DIV_RST_CH1_MASK_SFT                      (0x1 << 5)
#define VOW_DIGMIC_CK_PHASE_SEL_CH1_SFT                  0
#define VOW_DIGMIC_CK_PHASE_SEL_CH1_MASK                 0x1f
#define VOW_DIGMIC_CK_PHASE_SEL_CH1_MASK_SFT             (0x1f << 0)

/* AFE_VOW_TOP_CON4 */
#define VOW_ADC_CK_PDN_CH2_SFT                           7
#define VOW_ADC_CK_PDN_CH2_MASK                          0x1
#define VOW_ADC_CK_PDN_CH2_MASK_SFT                      (0x1 << 7)
#define VOW_CK_PDN_CH2_SFT                               6
#define VOW_CK_PDN_CH2_MASK                              0x1
#define VOW_CK_PDN_CH2_MASK_SFT                          (0x1 << 6)
#define VOW_ADC_CLK_INV_CH2_SFT                          5
#define VOW_ADC_CLK_INV_CH2_MASK                         0x1
#define VOW_ADC_CLK_INV_CH2_MASK_SFT                     (0x1 << 5)
#define VOW_INTR_SOURCE_SEL_CH2_SFT                      4
#define VOW_INTR_SOURCE_SEL_CH2_MASK                     0x1
#define VOW_INTR_SOURCE_SEL_CH2_MASK_SFT                 (0x1 << 4)
#define VOW_INTR_CLR_CH2_SFT                             3
#define VOW_INTR_CLR_CH2_MASK                            0x1
#define VOW_INTR_CLR_CH2_MASK_SFT                        (0x1 << 3)
#define S_N_VALUE_RST_CH2_SFT                            2
#define S_N_VALUE_RST_CH2_MASK                           0x1
#define S_N_VALUE_RST_CH2_MASK_SFT                       (0x1 << 2)
#define SAMPLE_BASE_MODE_CH2_SFT                         1
#define SAMPLE_BASE_MODE_CH2_MASK                        0x1
#define SAMPLE_BASE_MODE_CH2_MASK_SFT                    (0x1 << 1)
#define VOW_ON_CH2_SFT                                   0
#define VOW_ON_CH2_MASK                                  0x1
#define VOW_ON_CH2_MASK_SFT                              (0x1 << 0)

/* AFE_VOW_TOP_CON5 */
#define VOW_DMIC1_CK_PDN_SFT                             7
#define VOW_DMIC1_CK_PDN_MASK                            0x1
#define VOW_DMIC1_CK_PDN_MASK_SFT                        (0x1 << 7)
#define VOW_DIGMIC_ON_CH2_SFT                            6
#define VOW_DIGMIC_ON_CH2_MASK                           0x1
#define VOW_DIGMIC_ON_CH2_MASK_SFT                       (0x1 << 6)
#define VOW_CK_DIV_RST_CH2_SFT                           5
#define VOW_CK_DIV_RST_CH2_MASK                          0x1
#define VOW_CK_DIV_RST_CH2_MASK_SFT                      (0x1 << 5)
#define VOW_DIGMIC_CK_PHASE_SEL_CH2_SFT                  0
#define VOW_DIGMIC_CK_PHASE_SEL_CH2_MASK                 0x1f
#define VOW_DIGMIC_CK_PHASE_SEL_CH2_MASK_SFT             (0x1f << 0)

/* AFE_VOW_TOP_CON6 */
#define VOW_TXIF_SCK_DIV_SFT                             3
#define VOW_TXIF_SCK_DIV_MASK                            0x1f
#define VOW_TXIF_SCK_DIV_MASK_SFT                        (0x1f << 3)
#define VOW_P2_SNRDET_AUTO_PDN_SFT                       0
#define VOW_P2_SNRDET_AUTO_PDN_MASK                      0x1
#define VOW_P2_SNRDET_AUTO_PDN_MASK_SFT                  (0x1 << 0)

/* AFE_VOW_TOP_CON7 */
#define VOW_TXIF_SCK_INV_SFT                             7
#define VOW_TXIF_SCK_INV_MASK                            0x1
#define VOW_TXIF_SCK_INV_MASK_SFT                        (0x1 << 7)
#define VOW_ADC_TESTCK_SRC_SEL_SFT                       4
#define VOW_ADC_TESTCK_SRC_SEL_MASK                      0x7
#define VOW_ADC_TESTCK_SRC_SEL_MASK_SFT                  (0x7 << 4)
#define VOW_ADC_TESTCK_SEL_SFT                           3
#define VOW_ADC_TESTCK_SEL_MASK                          0x1
#define VOW_ADC_TESTCK_SEL_MASK_SFT                      (0x1 << 3)
#define VOW_TXIF_MONO_SFT                                1
#define VOW_TXIF_MONO_MASK                               0x1
#define VOW_TXIF_MONO_MASK_SFT                           (0x1 << 1)

/* AFE_VOW_TOP_CON8 */
#define RG_VOW_AMIC_ADC1_SOURCE_SEL_SFT                  4
#define RG_VOW_AMIC_ADC1_SOURCE_SEL_MASK                 0x3
#define RG_VOW_AMIC_ADC1_SOURCE_SEL_MASK_SFT             (0x3 << 4)
#define RG_VOW_AMIC_ADC2_SOURCE_SEL_SFT                  0
#define RG_VOW_AMIC_ADC2_SOURCE_SEL_MASK                 0x3
#define RG_VOW_AMIC_ADC2_SOURCE_SEL_MASK_SFT             (0x3 << 0)

/* AFE_VOW_TOP_CON9 */
#define RG_BUCK_DVFS_DONE_SW_CTL_SFT                     7
#define RG_BUCK_DVFS_DONE_SW_CTL_MASK                    0x1
#define RG_BUCK_DVFS_DONE_SW_CTL_MASK_SFT                (0x1 << 7)
#define RG_BUCK_DVFS_DONE_HW_MODE_SFT                    6
#define RG_BUCK_DVFS_DONE_HW_MODE_MASK                   0x1
#define RG_BUCK_DVFS_DONE_HW_MODE_MASK_SFT               (0x1 << 6)
#define RG_BUCK_DVFS_HW_CNT_THR_SFT                      0
#define RG_BUCK_DVFS_HW_CNT_THR_MASK                     0x3f
#define RG_BUCK_DVFS_HW_CNT_THR_MASK_SFT                 (0x3f << 0)

/* AFE_VOW_TOP_MON0 */
#define BUCK_DVFS_DONE_SFT                               7
#define BUCK_DVFS_DONE_MASK                              0x1
#define BUCK_DVFS_DONE_MASK_SFT                          (0x1 << 7)
#define VOW_INTR_SFT                                     4
#define VOW_INTR_MASK                                    0x1
#define VOW_INTR_MASK_SFT                                (0x1 << 4)
#define VOW_INTR_FLAG_CH1_SFT                            1
#define VOW_INTR_FLAG_CH1_MASK                           0x1
#define VOW_INTR_FLAG_CH1_MASK_SFT                       (0x1 << 1)
#define VOW_INTR_FLAG_CH2_SFT                            0
#define VOW_INTR_FLAG_CH2_MASK                           0x1
#define VOW_INTR_FLAG_CH2_MASK_SFT                       (0x1 << 0)

/* AFE_VOW_VAD_CFG0 */
#define AMPREF_CH1_L_SFT                                 0
#define AMPREF_CH1_L_MASK                                0xff
#define AMPREF_CH1_L_MASK_SFT                            (0xff << 0)

/* AFE_VOW_VAD_CFG1 */
#define AMPREF_CH1_H_SFT                                 0
#define AMPREF_CH1_H_MASK                                0xff
#define AMPREF_CH1_H_MASK_SFT                            (0xff << 0)

/* AFE_VOW_VAD_CFG2 */
#define AMPREF_CH2_L_SFT                                 0
#define AMPREF_CH2_L_MASK                                0xff
#define AMPREF_CH2_L_MASK_SFT                            (0xff << 0)

/* AFE_VOW_VAD_CFG3 */
#define AMPREF_CH2_H_SFT                                 0
#define AMPREF_CH2_H_MASK                                0xff
#define AMPREF_CH2_H_MASK_SFT                            (0xff << 0)

/* AFE_VOW_VAD_CFG4 */
#define TIMERINI_CH1_L_SFT                               0
#define TIMERINI_CH1_L_MASK                              0xff
#define TIMERINI_CH1_L_MASK_SFT                          (0xff << 0)

/* AFE_VOW_VAD_CFG5 */
#define TIMERINI_CH1_H_SFT                               0
#define TIMERINI_CH1_H_MASK                              0xff
#define TIMERINI_CH1_H_MASK_SFT                          (0xff << 0)

/* AFE_VOW_VAD_CFG6 */
#define TIMERINI_CH2_L_SFT                               0
#define TIMERINI_CH2_L_MASK                              0xff
#define TIMERINI_CH2_L_MASK_SFT                          (0xff << 0)

/* AFE_VOW_VAD_CFG7 */
#define TIMERINI_CH2_H_SFT                               0
#define TIMERINI_CH2_H_MASK                              0xff
#define TIMERINI_CH2_H_MASK_SFT                          (0xff << 0)

/* AFE_VOW_VAD_CFG8 */
#define B_INI_CH1_SFT                                    4
#define B_INI_CH1_MASK                                   0x7
#define B_INI_CH1_MASK_SFT                               (0x7 << 4)
#define A_INI_CH1_SFT                                    0
#define A_INI_CH1_MASK                                   0x7
#define A_INI_CH1_MASK_SFT                               (0x7 << 0)

/* AFE_VOW_VAD_CFG9 */
#define VOW_IRQ_LATCH_SNR_EN_CH1_SFT                     7
#define VOW_IRQ_LATCH_SNR_EN_CH1_MASK                    0x1
#define VOW_IRQ_LATCH_SNR_EN_CH1_MASK_SFT                (0x1 << 7)
#define B_DEFAULT_CH1_SFT                                4
#define B_DEFAULT_CH1_MASK                               0x7
#define B_DEFAULT_CH1_MASK_SFT                           (0x7 << 4)
#define A_DEFAULT_CH1_SFT                                0
#define A_DEFAULT_CH1_MASK                               0x7
#define A_DEFAULT_CH1_MASK_SFT                           (0x7 << 0)

/* AFE_VOW_VAD_CFG10 */
#define B_INI_CH2_SFT                                    4
#define B_INI_CH2_MASK                                   0x7
#define B_INI_CH2_MASK_SFT                               (0x7 << 4)
#define A_INI_CH2_SFT                                    0
#define A_INI_CH2_MASK                                   0x7
#define A_INI_CH2_MASK_SFT                               (0x7 << 0)

/* AFE_VOW_VAD_CFG11 */
#define VOW_IRQ_LATCH_SNR_EN_CH2_SFT                     7
#define VOW_IRQ_LATCH_SNR_EN_CH2_MASK                    0x1
#define VOW_IRQ_LATCH_SNR_EN_CH2_MASK_SFT                (0x1 << 7)
#define B_DEFAULT_CH2_SFT                                4
#define B_DEFAULT_CH2_MASK                               0x7
#define B_DEFAULT_CH2_MASK_SFT                           (0x7 << 4)
#define A_DEFAULT_CH2_SFT                                0
#define A_DEFAULT_CH2_MASK                               0x7
#define A_DEFAULT_CH2_MASK_SFT                           (0x7 << 0)

/* AFE_VOW_VAD_CFG12 */
#define K_ALPHA_RISE_CH1_SFT                             4
#define K_ALPHA_RISE_CH1_MASK                            0xf
#define K_ALPHA_RISE_CH1_MASK_SFT                        (0xf << 4)
#define K_ALPHA_FALL_CH1_SFT                             0
#define K_ALPHA_FALL_CH1_MASK                            0xf
#define K_ALPHA_FALL_CH1_MASK_SFT                        (0xf << 0)

/* AFE_VOW_VAD_CFG13 */
#define K_BETA_RISE_CH1_SFT                              4
#define K_BETA_RISE_CH1_MASK                             0xf
#define K_BETA_RISE_CH1_MASK_SFT                         (0xf << 4)
#define K_BETA_FALL_CH1_SFT                              0
#define K_BETA_FALL_CH1_MASK                             0xf
#define K_BETA_FALL_CH1_MASK_SFT                         (0xf << 0)

/* AFE_VOW_VAD_CFG14 */
#define K_ALPHA_RISE_CH2_SFT                             4
#define K_ALPHA_RISE_CH2_MASK                            0xf
#define K_ALPHA_RISE_CH2_MASK_SFT                        (0xf << 4)
#define K_ALPHA_FALL_CH2_SFT                             0
#define K_ALPHA_FALL_CH2_MASK                            0xf
#define K_ALPHA_FALL_CH2_MASK_SFT                        (0xf << 0)

/* AFE_VOW_VAD_CFG15 */
#define K_BETA_RISE_CH2_SFT                              4
#define K_BETA_RISE_CH2_MASK                             0xf
#define K_BETA_RISE_CH2_MASK_SFT                         (0xf << 4)
#define K_BETA_FALL_CH2_SFT                              0
#define K_BETA_FALL_CH2_MASK                             0xf
#define K_BETA_FALL_CH2_MASK_SFT                         (0xf << 0)

/* AFE_VOW_VAD_CFG16 */
#define N_MIN_CH1_L_SFT                                  0
#define N_MIN_CH1_L_MASK                                 0xff
#define N_MIN_CH1_L_MASK_SFT                             (0xff << 0)

/* AFE_VOW_VAD_CFG17 */
#define N_MIN_CH1_H_SFT                                  0
#define N_MIN_CH1_H_MASK                                 0xff
#define N_MIN_CH1_H_MASK_SFT                             (0xff << 0)

/* AFE_VOW_VAD_CFG18 */
#define N_MIN_CH2_L_SFT                                  0
#define N_MIN_CH2_L_MASK                                 0xff
#define N_MIN_CH2_L_MASK_SFT                             (0xff << 0)

/* AFE_VOW_VAD_CFG19 */
#define N_MIN_CH2_H_SFT                                  0
#define N_MIN_CH2_H_MASK                                 0xff
#define N_MIN_CH2_H_MASK_SFT                             (0xff << 0)

/* AFE_VOW_VAD_CFG20 */
#define VOW_SN_INI_CFG_VAL_CH1_L_SFT                     0
#define VOW_SN_INI_CFG_VAL_CH1_L_MASK                    0xff
#define VOW_SN_INI_CFG_VAL_CH1_L_MASK_SFT                (0xff << 0)

/* AFE_VOW_VAD_CFG21 */
#define VOW_SN_INI_CFG_EN_CH1_SFT                        7
#define VOW_SN_INI_CFG_EN_CH1_MASK                       0x1
#define VOW_SN_INI_CFG_EN_CH1_MASK_SFT                   (0x1 << 7)
#define VOW_SN_INI_CFG_VAL_CH1_H_SFT                     0
#define VOW_SN_INI_CFG_VAL_CH1_H_MASK                    0x7f
#define VOW_SN_INI_CFG_VAL_CH1_H_MASK_SFT                (0x7f << 0)

/* AFE_VOW_VAD_CFG22 */
#define VOW_SN_INI_CFG_VAL_CH2_L_SFT                     0
#define VOW_SN_INI_CFG_VAL_CH2_L_MASK                    0xff
#define VOW_SN_INI_CFG_VAL_CH2_L_MASK_SFT                (0xff << 0)

/* AFE_VOW_VAD_CFG23 */
#define VOW_SN_INI_CFG_EN_CH2_SFT                        7
#define VOW_SN_INI_CFG_EN_CH2_MASK                       0x1
#define VOW_SN_INI_CFG_EN_CH2_MASK_SFT                   (0x1 << 7)
#define VOW_SN_INI_CFG_VAL_CH2_H_SFT                     0
#define VOW_SN_INI_CFG_VAL_CH2_H_MASK                    0x7f
#define VOW_SN_INI_CFG_VAL_CH2_H_MASK_SFT                (0x7f << 0)

/* AFE_VOW_VAD_CFG24 */
#define K_GAMMA_CH1_SFT                                  4
#define K_GAMMA_CH1_MASK                                 0xf
#define K_GAMMA_CH1_MASK_SFT                             (0xf << 4)
#define K_GAMMA_CH2_SFT                                  0
#define K_GAMMA_CH2_MASK                                 0xf
#define K_GAMMA_CH2_MASK_SFT                             (0xf << 0)

/* AFE_VOW_VAD_MON0 */
#define VOW_DOWNCNT_CH1_L_SFT                            0
#define VOW_DOWNCNT_CH1_L_MASK                           0xff
#define VOW_DOWNCNT_CH1_L_MASK_SFT                       (0xff << 0)

/* AFE_VOW_VAD_MON1 */
#define VOW_DOWNCNT_CH1_H_SFT                            0
#define VOW_DOWNCNT_CH1_H_MASK                           0xff
#define VOW_DOWNCNT_CH1_H_MASK_SFT                       (0xff << 0)

/* AFE_VOW_VAD_MON2 */
#define VOW_DOWNCNT_CH2_L_SFT                            0
#define VOW_DOWNCNT_CH2_L_MASK                           0xff
#define VOW_DOWNCNT_CH2_L_MASK_SFT                       (0xff << 0)

/* AFE_VOW_VAD_MON3 */
#define VOW_DOWNCNT_CH2_H_SFT                            0
#define VOW_DOWNCNT_CH2_H_MASK                           0xff
#define VOW_DOWNCNT_CH2_H_MASK_SFT                       (0xff << 0)

/* AFE_VOW_VAD_MON4 */
#define VOW_B_CH1_SFT                                    4
#define VOW_B_CH1_MASK                                   0x7
#define VOW_B_CH1_MASK_SFT                               (0x7 << 4)
#define VOW_A_CH1_SFT                                    1
#define VOW_A_CH1_MASK                                   0x7
#define VOW_A_CH1_MASK_SFT                               (0x7 << 1)
#define SECOND_CNT_START_CH1_SFT                         0
#define SECOND_CNT_START_CH1_MASK                        0x1
#define SECOND_CNT_START_CH1_MASK_SFT                    (0x1 << 0)

/* AFE_VOW_VAD_MON5 */
#define K_TMP_MON_CH1_SFT                                4
#define K_TMP_MON_CH1_MASK                               0xf
#define K_TMP_MON_CH1_MASK_SFT                           (0xf << 4)
#define SLT_COUNTER_MON_CH1_SFT                          0
#define SLT_COUNTER_MON_CH1_MASK                         0x7
#define SLT_COUNTER_MON_CH1_MASK_SFT                     (0x7 << 0)

/* AFE_VOW_VAD_MON6 */
#define VOW_B_CH2_SFT                                    4
#define VOW_B_CH2_MASK                                   0x7
#define VOW_B_CH2_MASK_SFT                               (0x7 << 4)
#define VOW_A_CH2_SFT                                    1
#define VOW_A_CH2_MASK                                   0x7
#define VOW_A_CH2_MASK_SFT                               (0x7 << 1)
#define SECOND_CNT_START_CH2_SFT                         0
#define SECOND_CNT_START_CH2_MASK                        0x1
#define SECOND_CNT_START_CH2_MASK_SFT                    (0x1 << 0)

/* AFE_VOW_VAD_MON7 */
#define K_TMP_MON_CH2_SFT                                4
#define K_TMP_MON_CH2_MASK                               0xf
#define K_TMP_MON_CH2_MASK_SFT                           (0xf << 4)
#define SLT_COUNTER_MON_CH2_SFT                          0
#define SLT_COUNTER_MON_CH2_MASK                         0x7
#define SLT_COUNTER_MON_CH2_MASK_SFT                     (0x7 << 0)

/* AFE_VOW_VAD_MON8 */
#define VOW_S_L_CH1_L_SFT                                0
#define VOW_S_L_CH1_L_MASK                               0xff
#define VOW_S_L_CH1_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON9 */
#define VOW_S_L_CH1_H_SFT                                0
#define VOW_S_L_CH1_H_MASK                               0xff
#define VOW_S_L_CH1_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON10 */
#define VOW_S_L_CH2_L_SFT                                0
#define VOW_S_L_CH2_L_MASK                               0xff
#define VOW_S_L_CH2_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON11 */
#define VOW_S_L_CH2_H_SFT                                0
#define VOW_S_L_CH2_H_MASK                               0xff
#define VOW_S_L_CH2_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON12 */
#define VOW_S_H_CH1_L_SFT                                0
#define VOW_S_H_CH1_L_MASK                               0xff
#define VOW_S_H_CH1_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON13 */
#define VOW_S_H_CH1_H_SFT                                0
#define VOW_S_H_CH1_H_MASK                               0xff
#define VOW_S_H_CH1_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON14 */
#define VOW_S_H_CH2_L_SFT                                0
#define VOW_S_H_CH2_L_MASK                               0xff
#define VOW_S_H_CH2_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON15 */
#define VOW_S_H_CH2_H_SFT                                0
#define VOW_S_H_CH2_H_MASK                               0xff
#define VOW_S_H_CH2_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON16 */
#define VOW_N_L_CH1_L_SFT                                0
#define VOW_N_L_CH1_L_MASK                               0xff
#define VOW_N_L_CH1_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON17 */
#define VOW_N_L_CH1_H_SFT                                0
#define VOW_N_L_CH1_H_MASK                               0xff
#define VOW_N_L_CH1_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON18 */
#define VOW_N_L_CH2_L_SFT                                0
#define VOW_N_L_CH2_L_MASK                               0xff
#define VOW_N_L_CH2_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON19 */
#define VOW_N_L_CH2_H_SFT                                0
#define VOW_N_L_CH2_H_MASK                               0xff
#define VOW_N_L_CH2_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON20 */
#define VOW_N_H_CH1_L_SFT                                0
#define VOW_N_H_CH1_L_MASK                               0xff
#define VOW_N_H_CH1_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON21 */
#define VOW_N_H_CH1_H_SFT                                0
#define VOW_N_H_CH1_H_MASK                               0xff
#define VOW_N_H_CH1_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON22 */
#define VOW_N_H_CH2_L_SFT                                0
#define VOW_N_H_CH2_L_MASK                               0xff
#define VOW_N_H_CH2_L_MASK_SFT                           (0xff << 0)

/* AFE_VOW_VAD_MON23 */
#define VOW_N_H_CH2_H_SFT                                0
#define VOW_N_H_CH2_H_MASK                               0xff
#define VOW_N_H_CH2_H_MASK_SFT                           (0xff << 0)

/* AFE_VOW_TGEN_CFG0 */
#define VOW_TGEN_FREQ_DIV_CH1_L_SFT                      0
#define VOW_TGEN_FREQ_DIV_CH1_L_MASK                     0xff
#define VOW_TGEN_FREQ_DIV_CH1_L_MASK_SFT                 (0xff << 0)

/* AFE_VOW_TGEN_CFG1 */
#define VOW_TGEN_EN_CH1_SFT                              7
#define VOW_TGEN_EN_CH1_MASK                             0x1
#define VOW_TGEN_EN_CH1_MASK_SFT                         (0x1 << 7)
#define VOW_TGEN_MUTE_SW_CH1_SFT                         6
#define VOW_TGEN_MUTE_SW_CH1_MASK                        0x1
#define VOW_TGEN_MUTE_SW_CH1_MASK_SFT                    (0x1 << 6)
#define VOW_TGEN_FREQ_DIV_CH1_H_SFT                      0
#define VOW_TGEN_FREQ_DIV_CH1_H_MASK                     0x3f
#define VOW_TGEN_FREQ_DIV_CH1_H_MASK_SFT                 (0x3f << 0)

/* AFE_VOW_TGEN_CFG2 */
#define VOW_TGEN_FREQ_DIV_CH2_L_SFT                      0
#define VOW_TGEN_FREQ_DIV_CH2_L_MASK                     0xff
#define VOW_TGEN_FREQ_DIV_CH2_L_MASK_SFT                 (0xff << 0)

/* AFE_VOW_TGEN_CFG3 */
#define VOW_TGEN_EN_CH2_SFT                              7
#define VOW_TGEN_EN_CH2_MASK                             0x1
#define VOW_TGEN_EN_CH2_MASK_SFT                         (0x1 << 7)
#define VOW_TGEN_MUTE_SW_CH2_SFT                         6
#define VOW_TGEN_MUTE_SW_CH2_MASK                        0x1
#define VOW_TGEN_MUTE_SW_CH2_MASK_SFT                    (0x1 << 6)
#define VOW_TGEN_FREQ_DIV_CH2_H_SFT                      0
#define VOW_TGEN_FREQ_DIV_CH2_H_MASK                     0x3f
#define VOW_TGEN_FREQ_DIV_CH2_H_MASK_SFT                 (0x3f << 0)

/* AFE_VOW_HPF_CFG0 */
#define RG_BASELINE_ALPHA_ORDER_CH1_SFT                  4
#define RG_BASELINE_ALPHA_ORDER_CH1_MASK                 0xf
#define RG_BASELINE_ALPHA_ORDER_CH1_MASK_SFT             (0xf << 4)
#define RG_MTKAIF_HPF_BYPASS_CH1_SFT                     2
#define RG_MTKAIF_HPF_BYPASS_CH1_MASK                    0x1
#define RG_MTKAIF_HPF_BYPASS_CH1_MASK_SFT                (0x1 << 2)
#define RG_SNRDET_HPF_BYPASS_CH1_SFT                     1
#define RG_SNRDET_HPF_BYPASS_CH1_MASK                    0x1
#define RG_SNRDET_HPF_BYPASS_CH1_MASK_SFT                (0x1 << 1)
#define RG_HPF_ON_CH1_SFT                                0
#define RG_HPF_ON_CH1_MASK                               0x1
#define RG_HPF_ON_CH1_MASK_SFT                           (0x1 << 0)

/* AFE_VOW_HPF_CFG1 */
#define VOW_HPF_DC_TEST_CH1_SFT                          4
#define VOW_HPF_DC_TEST_CH1_MASK                         0xf
#define VOW_HPF_DC_TEST_CH1_MASK_SFT                     (0xf << 4)

/* AFE_VOW_HPF_CFG2 */
#define RG_BASELINE_ALPHA_ORDER_CH2_SFT                  4
#define RG_BASELINE_ALPHA_ORDER_CH2_MASK                 0xf
#define RG_BASELINE_ALPHA_ORDER_CH2_MASK_SFT             (0xf << 4)
#define RG_MTKAIF_HPF_BYPASS_CH2_SFT                     2
#define RG_MTKAIF_HPF_BYPASS_CH2_MASK                    0x1
#define RG_MTKAIF_HPF_BYPASS_CH2_MASK_SFT                (0x1 << 2)
#define RG_SNRDET_HPF_BYPASS_CH2_SFT                     1
#define RG_SNRDET_HPF_BYPASS_CH2_MASK                    0x1
#define RG_SNRDET_HPF_BYPASS_CH2_MASK_SFT                (0x1 << 1)
#define RG_HPF_ON_CH2_SFT                                0
#define RG_HPF_ON_CH2_MASK                               0x1
#define RG_HPF_ON_CH2_MASK_SFT                           (0x1 << 0)

/* AFE_VOW_HPF_CFG3 */
#define VOW_HPF_DC_TEST_CH2_SFT                          4
#define VOW_HPF_DC_TEST_CH2_MASK                         0xf
#define VOW_HPF_DC_TEST_CH2_MASK_SFT                     (0xf << 4)

/* AUDIO_DIG_3_ANA_ID */
#define AUDIO_DIG_3_ANA_ID_SFT                           0
#define AUDIO_DIG_3_ANA_ID_MASK                          0xff
#define AUDIO_DIG_3_ANA_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_3_DIG_ID */
#define AUDIO_DIG_3_DIG_ID_SFT                           0
#define AUDIO_DIG_3_DIG_ID_MASK                          0xff
#define AUDIO_DIG_3_DIG_ID_MASK_SFT                      (0xff << 0)

/* AUDIO_DIG_3_ANA_REV */
#define AUDIO_DIG_3_ANA_MINOR_REV_SFT                    0
#define AUDIO_DIG_3_ANA_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_3_ANA_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_3_ANA_MAJOR_REV_SFT                    4
#define AUDIO_DIG_3_ANA_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_3_ANA_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_3_DIG_REV */
#define AUDIO_DIG_3_DIG_MINOR_REV_SFT                    0
#define AUDIO_DIG_3_DIG_MINOR_REV_MASK                   0xf
#define AUDIO_DIG_3_DIG_MINOR_REV_MASK_SFT               (0xf << 0)
#define AUDIO_DIG_3_DIG_MAJOR_REV_SFT                    4
#define AUDIO_DIG_3_DIG_MAJOR_REV_MASK                   0xf
#define AUDIO_DIG_3_DIG_MAJOR_REV_MASK_SFT               (0xf << 4)

/* AUDIO_DIG_3_DSN_DBI */
#define AUDIO_DIG_3_DSN_CBS_SFT                          0
#define AUDIO_DIG_3_DSN_CBS_MASK                         0x3
#define AUDIO_DIG_3_DSN_CBS_MASK_SFT                     (0x3 << 0)
#define AUDIO_DIG_3_DSN_BIX_SFT                          2
#define AUDIO_DIG_3_DSN_BIX_MASK                         0x3
#define AUDIO_DIG_3_DSN_BIX_MASK_SFT                     (0x3 << 2)

/* AUDIO_DIG_3_DSN_ESP */
#define AUDIO_DIG_3_DSN_ESP_SFT                          0
#define AUDIO_DIG_3_DSN_ESP_MASK                         0xff
#define AUDIO_DIG_3_DSN_ESP_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_3_DSN_FPI */
#define AUDIO_DIG_3_DSN_FPI_SFT                          0
#define AUDIO_DIG_3_DSN_FPI_MASK                         0xff
#define AUDIO_DIG_3_DSN_FPI_MASK_SFT                     (0xff << 0)

/* AUDIO_DIG_3_DSN_DXI */
#define AUDIO_DIG_3_DSN_DXI_SFT                          0
#define AUDIO_DIG_3_DSN_DXI_MASK                         0xff
#define AUDIO_DIG_3_DSN_DXI_MASK_SFT                     (0xff << 0)

/* AFE_NCP_CFG0 */
#define RG_NCP_DITHER_EN_SFT                             7
#define RG_NCP_DITHER_EN_MASK                            0x1
#define RG_NCP_DITHER_EN_MASK_SFT                        (0x1 << 7)
#define RG_NCP_DITHER_FIXED_CK0_ACK1_2P_SFT              4
#define RG_NCP_DITHER_FIXED_CK0_ACK1_2P_MASK             0x7
#define RG_NCP_DITHER_FIXED_CK0_ACK1_2P_MASK_SFT         (0x7 << 4)
#define RG_NCP_DITHER_FIXED_CK0_ACK2_2P_SFT              1
#define RG_NCP_DITHER_FIXED_CK0_ACK2_2P_MASK             0x7
#define RG_NCP_DITHER_FIXED_CK0_ACK2_2P_MASK_SFT         (0x7 << 1)
#define RG_NCP_ON_SFT                                    0
#define RG_NCP_ON_MASK                                   0x1
#define RG_NCP_ON_MASK_SFT                               (0x1 << 0)

/* AFE_NCP_CFG1 */
#define RG_NCP_CK1_VALID_CNT_SFT                         1
#define RG_NCP_CK1_VALID_CNT_MASK                        0x7f
#define RG_NCP_CK1_VALID_CNT_MASK_SFT                    (0x7f << 1)
#define RG_NCP_ADITH_SFT                                 0
#define RG_NCP_ADITH_MASK                                0x1
#define RG_NCP_ADITH_MASK_SFT                            (0x1 << 0)

/* AFE_NCP_CFG2 */
#define RG_Y_VAL_CFG_SFT                                 0
#define RG_Y_VAL_CFG_MASK                                0x7f
#define RG_Y_VAL_CFG_MASK_SFT                            (0x7f << 0)

/* AFE_NCP_CFG3 */
#define RG_XY_VAL_CFG_EN_SFT                             7
#define RG_XY_VAL_CFG_EN_MASK                            0x1
#define RG_XY_VAL_CFG_EN_MASK_SFT                        (0x1 << 7)
#define RG_X_VAL_CFG_SFT                                 0
#define RG_X_VAL_CFG_MASK                                0x7f
#define RG_X_VAL_CFG_MASK_SFT                            (0x7f << 0)

/* AFE_NCP_CFG4 */
#define RG_NCP_NONCLK_SET_SFT                            1
#define RG_NCP_NONCLK_SET_MASK                           0x1
#define RG_NCP_NONCLK_SET_MASK_SFT                       (0x1 << 1)
#define RG_NCP_PDDIS_EN_SFT                              0
#define RG_NCP_PDDIS_EN_MASK                             0x1
#define RG_NCP_PDDIS_EN_MASK_SFT                         (0x1 << 0)

/* AUDNCP_CLKDIV_CON0 */
#define RG_DIVCKS_CHG_SFT                                0
#define RG_DIVCKS_CHG_MASK                               0x1
#define RG_DIVCKS_CHG_MASK_SFT                           (0x1 << 0)

/* AUDNCP_CLKDIV_CON1 */
#define RG_DIVCKS_ON_SFT                                 0
#define RG_DIVCKS_ON_MASK                                0x1
#define RG_DIVCKS_ON_MASK_SFT                            (0x1 << 0)

/* AUDNCP_CLKDIV_CON2 */
#define RG_DIVCKS_PRG_L_SFT                              0
#define RG_DIVCKS_PRG_L_MASK                             0xff
#define RG_DIVCKS_PRG_L_MASK_SFT                         (0xff << 0)

/* AUDNCP_CLKDIV_CON3 */
#define RG_DIVCKS_PRG_H_SFT                              7
#define RG_DIVCKS_PRG_H_MASK                             0x1
#define RG_DIVCKS_PRG_H_MASK_SFT                         (0x1 << 7)
#define RG_DIVCKS_PWD_NCP_SFT                            0
#define RG_DIVCKS_PWD_NCP_MASK                           0x1
#define RG_DIVCKS_PWD_NCP_MASK_SFT                       (0x1 << 0)

/* AUDNCP_CLKDIV_CON4 */
#define RG_DIVCKS_PWD_NCP_ST_SEL_SFT                     0
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK                    0x3
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK_SFT                (0x3 << 0)

/* AUDENC_ANA_ID */
#define AUDENC_ANA_ID_SFT                                0
#define AUDENC_ANA_ID_MASK                               0xff
#define AUDENC_ANA_ID_MASK_SFT                           (0xff << 0)

/* AUDENC_DIG_ID */
#define AUDENC_DIG_ID_SFT                                0
#define AUDENC_DIG_ID_MASK                               0xff
#define AUDENC_DIG_ID_MASK_SFT                           (0xff << 0)

/* AUDENC_ANA_REV */
#define AUDENC_ANA_MINOR_REV_SFT                         0
#define AUDENC_ANA_MINOR_REV_MASK                        0xf
#define AUDENC_ANA_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDENC_ANA_MAJOR_REV_SFT                         4
#define AUDENC_ANA_MAJOR_REV_MASK                        0xf
#define AUDENC_ANA_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDENC_DIG_REV */
#define AUDENC_DIG_MINOR_REV_SFT                         0
#define AUDENC_DIG_MINOR_REV_MASK                        0xf
#define AUDENC_DIG_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDENC_DIG_MAJOR_REV_SFT                         4
#define AUDENC_DIG_MAJOR_REV_MASK                        0xf
#define AUDENC_DIG_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDENC_DBI */
#define AUDENC_CBS_SFT                                   0
#define AUDENC_CBS_MASK                                  0x3
#define AUDENC_CBS_MASK_SFT                              (0x3 << 0)
#define AUDENC_BIX_SFT                                   2
#define AUDENC_BIX_MASK                                  0x3
#define AUDENC_BIX_MASK_SFT                              (0x3 << 2)

/* AUDENC_ESP */
#define AUDENC_ESP_SFT                                   0
#define AUDENC_ESP_MASK                                  0xff
#define AUDENC_ESP_MASK_SFT                              (0xff << 0)

/* AUDENC_FPI */
#define AUDENC_FPI_SFT                                   0
#define AUDENC_FPI_MASK                                  0xff
#define AUDENC_FPI_MASK_SFT                              (0xff << 0)

/* AUDENC_DXI */
#define AUDENC_DXI_SFT                                   0
#define AUDENC_DXI_MASK                                  0xff
#define AUDENC_DXI_MASK_SFT                              (0xff << 0)

/* AUDENC_ANA_CON0 */
#define RG_AUDPREAMPLON_SFT                              0
#define RG_AUDPREAMPLON_MASK                             0x1
#define RG_AUDPREAMPLON_MASK_SFT                         (0x1 << 0)
#define RG_AUDPREAMPLDCCEN_SFT                           1
#define RG_AUDPREAMPLDCCEN_MASK                          0x1
#define RG_AUDPREAMPLDCCEN_MASK_SFT                      (0x1 << 1)
#define RG_AUDPREAMPLDCPRECHARGE_SFT                     2
#define RG_AUDPREAMPLDCPRECHARGE_MASK                    0x1
#define RG_AUDPREAMPLDCPRECHARGE_MASK_SFT                (0x1 << 2)
#define RG_AUDPREAMPLPGATEST_SFT                         3
#define RG_AUDPREAMPLPGATEST_MASK                        0x1
#define RG_AUDPREAMPLPGATEST_MASK_SFT                    (0x1 << 3)
#define RG_AUDPREAMPLVSCALE_SFT                          4
#define RG_AUDPREAMPLVSCALE_MASK                         0x3
#define RG_AUDPREAMPLVSCALE_MASK_SFT                     (0x3 << 4)
#define RG_AUDPREAMPLINPUTSEL_SFT                        6
#define RG_AUDPREAMPLINPUTSEL_MASK                       0x3
#define RG_AUDPREAMPLINPUTSEL_MASK_SFT                   (0x3 << 6)

/* AUDENC_ANA_CON1 */
#define RG_AUDPREAMPLGAIN_SFT                            0
#define RG_AUDPREAMPLGAIN_MASK                           0x7
#define RG_AUDPREAMPLGAIN_MASK_SFT                       (0x7 << 0)
#define RG_AUDIO_VOW_EN_SFT                              3
#define RG_AUDIO_VOW_EN_MASK                             0x1
#define RG_AUDIO_VOW_EN_MASK_SFT                         (0x1 << 3)
#define RG_AUDADCLPWRUP_SFT                              4
#define RG_AUDADCLPWRUP_MASK                             0x1
#define RG_AUDADCLPWRUP_MASK_SFT                         (0x1 << 4)
#define RG_AUDADCLINPUTSEL_SFT                           5
#define RG_AUDADCLINPUTSEL_MASK                          0x3
#define RG_AUDADCLINPUTSEL_MASK_SFT                      (0x3 << 5)
#define RG_CLKSQ_EN_VOW_SFT                              7
#define RG_CLKSQ_EN_VOW_MASK                             0x1
#define RG_CLKSQ_EN_VOW_MASK_SFT                         (0x1 << 7)

/* AUDENC_ANA_CON2 */
#define RG_AUDPREAMPRON_SFT                              0
#define RG_AUDPREAMPRON_MASK                             0x1
#define RG_AUDPREAMPRON_MASK_SFT                         (0x1 << 0)
#define RG_AUDPREAMPRDCCEN_SFT                           1
#define RG_AUDPREAMPRDCCEN_MASK                          0x1
#define RG_AUDPREAMPRDCCEN_MASK_SFT                      (0x1 << 1)
#define RG_AUDPREAMPRDCPRECHARGE_SFT                     2
#define RG_AUDPREAMPRDCPRECHARGE_MASK                    0x1
#define RG_AUDPREAMPRDCPRECHARGE_MASK_SFT                (0x1 << 2)
#define RG_AUDPREAMPRPGATEST_SFT                         3
#define RG_AUDPREAMPRPGATEST_MASK                        0x1
#define RG_AUDPREAMPRPGATEST_MASK_SFT                    (0x1 << 3)
#define RG_AUDPREAMPRVSCALE_SFT                          4
#define RG_AUDPREAMPRVSCALE_MASK                         0x3
#define RG_AUDPREAMPRVSCALE_MASK_SFT                     (0x3 << 4)
#define RG_AUDPREAMPRINPUTSEL_SFT                        6
#define RG_AUDPREAMPRINPUTSEL_MASK                       0x3
#define RG_AUDPREAMPRINPUTSEL_MASK_SFT                   (0x3 << 6)

/* AUDENC_ANA_CON3 */
#define RG_AUDPREAMPRGAIN_SFT                            0
#define RG_AUDPREAMPRGAIN_MASK                           0x7
#define RG_AUDPREAMPRGAIN_MASK_SFT                       (0x7 << 0)
#define RG_AUDADCRPWRUP_SFT                              4
#define RG_AUDADCRPWRUP_MASK                             0x1
#define RG_AUDADCRPWRUP_MASK_SFT                         (0x1 << 4)
#define RG_AUDADCRINPUTSEL_SFT                           5
#define RG_AUDADCRINPUTSEL_MASK                          0x3
#define RG_AUDADCRINPUTSEL_MASK_SFT                      (0x3 << 5)

/* AUDENC_ANA_CON4 */
#define RG_AUDULHALFBIAS_SFT                             0
#define RG_AUDULHALFBIAS_MASK                            0x1
#define RG_AUDULHALFBIAS_MASK_SFT                        (0x1 << 0)
#define RG_AUDGLBVOWLPWEN_SFT                            1
#define RG_AUDGLBVOWLPWEN_MASK                           0x1
#define RG_AUDGLBVOWLPWEN_MASK_SFT                       (0x1 << 1)
#define RG_AUDPREAMPLPEN_SFT                             2
#define RG_AUDPREAMPLPEN_MASK                            0x1
#define RG_AUDPREAMPLPEN_MASK_SFT                        (0x1 << 2)
#define RG_AUDADC1STSTAGELPEN_SFT                        3
#define RG_AUDADC1STSTAGELPEN_MASK                       0x1
#define RG_AUDADC1STSTAGELPEN_MASK_SFT                   (0x1 << 3)
#define RG_AUDADC2NDSTAGELPEN_SFT                        4
#define RG_AUDADC2NDSTAGELPEN_MASK                       0x1
#define RG_AUDADC2NDSTAGELPEN_MASK_SFT                   (0x1 << 4)
#define RG_AUDADCFLASHLPEN_SFT                           5
#define RG_AUDADCFLASHLPEN_MASK                          0x1
#define RG_AUDADCFLASHLPEN_MASK_SFT                      (0x1 << 5)
#define RG_AUDPREAMPIDDTEST_SFT                          6
#define RG_AUDPREAMPIDDTEST_MASK                         0x3
#define RG_AUDPREAMPIDDTEST_MASK_SFT                     (0x3 << 6)

/* AUDENC_ANA_CON5 */
#define RG_AUDADC1STSTAGEIDDTEST_SFT                     0
#define RG_AUDADC1STSTAGEIDDTEST_MASK                    0x3
#define RG_AUDADC1STSTAGEIDDTEST_MASK_SFT                (0x3 << 0)
#define RG_AUDADC2NDSTAGEIDDTEST_SFT                     2
#define RG_AUDADC2NDSTAGEIDDTEST_MASK                    0x3
#define RG_AUDADC2NDSTAGEIDDTEST_MASK_SFT                (0x3 << 2)
#define RG_AUDADCREFBUFIDDTEST_SFT                       4
#define RG_AUDADCREFBUFIDDTEST_MASK                      0x3
#define RG_AUDADCREFBUFIDDTEST_MASK_SFT                  (0x3 << 4)
#define RG_AUDADCFLASHIDDTEST_SFT                        6
#define RG_AUDADCFLASHIDDTEST_MASK                       0x3
#define RG_AUDADCFLASHIDDTEST_MASK_SFT                   (0x3 << 6)

/* AUDENC_ANA_CON6 */
#define RG_AUDADCDAC0P25FS_SFT                           0
#define RG_AUDADCDAC0P25FS_MASK                          0x1
#define RG_AUDADCDAC0P25FS_MASK_SFT                      (0x1 << 0)
#define RG_AUDADCCLKSEL_SFT                              1
#define RG_AUDADCCLKSEL_MASK                             0x1
#define RG_AUDADCCLKSEL_MASK_SFT                         (0x1 << 1)
#define RG_AUDADCCLKSOURCE_SFT                           2
#define RG_AUDADCCLKSOURCE_MASK                          0x3
#define RG_AUDADCCLKSOURCE_MASK_SFT                      (0x3 << 2)

/* AUDENC_ANA_CON7 */
#define RG_AUDPREAMPAAFEN_SFT                            0
#define RG_AUDPREAMPAAFEN_MASK                           0x1
#define RG_AUDPREAMPAAFEN_MASK_SFT                       (0x1 << 0)
#define RG_DCCVCMBUFLPMODSEL_SFT                         1
#define RG_DCCVCMBUFLPMODSEL_MASK                        0x1
#define RG_DCCVCMBUFLPMODSEL_MASK_SFT                    (0x1 << 1)
#define RG_DCCVCMBUFLPSWEN_SFT                           2
#define RG_DCCVCMBUFLPSWEN_MASK                          0x1
#define RG_DCCVCMBUFLPSWEN_MASK_SFT                      (0x1 << 2)
#define RG_CMSTBENH_SFT                                  3
#define RG_CMSTBENH_MASK                                 0x1
#define RG_CMSTBENH_MASK_SFT                             (0x1 << 3)
#define RG_PGABODYSW_SFT                                 4
#define RG_PGABODYSW_MASK                                0x1
#define RG_PGABODYSW_MASK_SFT                            (0x1 << 4)

/* AUDENC_ANA_CON8 */
#define RG_AUDADC1STSTAGESDENB_SFT                       0
#define RG_AUDADC1STSTAGESDENB_MASK                      0x1
#define RG_AUDADC1STSTAGESDENB_MASK_SFT                  (0x1 << 0)
#define RG_AUDADC2NDSTAGERESET_SFT                       1
#define RG_AUDADC2NDSTAGERESET_MASK                      0x1
#define RG_AUDADC2NDSTAGERESET_MASK_SFT                  (0x1 << 1)
#define RG_AUDADC3RDSTAGERESET_SFT                       2
#define RG_AUDADC3RDSTAGERESET_MASK                      0x1
#define RG_AUDADC3RDSTAGERESET_MASK_SFT                  (0x1 << 2)
#define RG_AUDADCFSRESET_SFT                             3
#define RG_AUDADCFSRESET_MASK                            0x1
#define RG_AUDADCFSRESET_MASK_SFT                        (0x1 << 3)
#define RG_AUDADCWIDECM_SFT                              4
#define RG_AUDADCWIDECM_MASK                             0x1
#define RG_AUDADCWIDECM_MASK_SFT                         (0x1 << 4)
#define RG_AUDADCNOPATEST_SFT                            5
#define RG_AUDADCNOPATEST_MASK                           0x1
#define RG_AUDADCNOPATEST_MASK_SFT                       (0x1 << 5)
#define RG_AUDADCBYPASS_SFT                              6
#define RG_AUDADCBYPASS_MASK                             0x1
#define RG_AUDADCBYPASS_MASK_SFT                         (0x1 << 6)
#define RG_AUDADCFFBYPASS_SFT                            7
#define RG_AUDADCFFBYPASS_MASK                           0x1
#define RG_AUDADCFFBYPASS_MASK_SFT                       (0x1 << 7)

/* AUDENC_ANA_CON9 */
#define RG_AUDADCDACFBCURRENT_SFT                        0
#define RG_AUDADCDACFBCURRENT_MASK                       0x1
#define RG_AUDADCDACFBCURRENT_MASK_SFT                   (0x1 << 0)
#define RG_AUDADCDACIDDTEST_SFT                          1
#define RG_AUDADCDACIDDTEST_MASK                         0x3
#define RG_AUDADCDACIDDTEST_MASK_SFT                     (0x3 << 1)
#define RG_AUDADCDACNRZ_SFT                              3
#define RG_AUDADCDACNRZ_MASK                             0x1
#define RG_AUDADCDACNRZ_MASK_SFT                         (0x1 << 3)
#define RG_AUDADCNODEM_SFT                               4
#define RG_AUDADCNODEM_MASK                              0x1
#define RG_AUDADCNODEM_MASK_SFT                          (0x1 << 4)
#define RG_AUDADCDACTEST_SFT                             5
#define RG_AUDADCDACTEST_MASK                            0x1
#define RG_AUDADCDACTEST_MASK_SFT                        (0x1 << 5)

/* AUDENC_ANA_CON10 */
#define RG_AUDRCTUNEL_SFT                                0
#define RG_AUDRCTUNEL_MASK                               0x1f
#define RG_AUDRCTUNEL_MASK_SFT                           (0x1f << 0)
#define RG_AUDRCTUNELSEL_SFT                             5
#define RG_AUDRCTUNELSEL_MASK                            0x1
#define RG_AUDRCTUNELSEL_MASK_SFT                        (0x1 << 5)

/* AUDENC_ANA_CON11 */
#define RG_AUDRCTUNER_SFT                                0
#define RG_AUDRCTUNER_MASK                               0x1f
#define RG_AUDRCTUNER_MASK_SFT                           (0x1f << 0)
#define RG_AUDRCTUNERSEL_SFT                             5
#define RG_AUDRCTUNERSEL_MASK                            0x1
#define RG_AUDRCTUNERSEL_MASK_SFT                        (0x1 << 5)

/* AUDENC_ANA_CON12 */
#define RG_CLKSQ_EN_SFT                                  0
#define RG_CLKSQ_EN_MASK                                 0x1
#define RG_CLKSQ_EN_MASK_SFT                             (0x1 << 0)
#define RG_CLKSQ_IN_SEL_TEST_SFT                         1
#define RG_CLKSQ_IN_SEL_TEST_MASK                        0x1
#define RG_CLKSQ_IN_SEL_TEST_MASK_SFT                    (0x1 << 1)
#define RG_CM_REFGENSEL_SFT                              2
#define RG_CM_REFGENSEL_MASK                             0x1
#define RG_CM_REFGENSEL_MASK_SFT                         (0x1 << 2)
#define RG_AUDSPARE_SFT                                  4
#define RG_AUDSPARE_MASK                                 0xf
#define RG_AUDSPARE_MASK_SFT                             (0xf << 4)

/* AUDENC_ANA_CON13 */
#define RG_AUDENCSPARE_SFT                               0
#define RG_AUDENCSPARE_MASK                              0x3f
#define RG_AUDENCSPARE_MASK_SFT                          (0x3f << 0)

/* AUDENC_ANA_CON14 */
#define RG_AUDENCSPARE2_SFT                              0
#define RG_AUDENCSPARE2_MASK                             0xff
#define RG_AUDENCSPARE2_MASK_SFT                         (0xff << 0)

/* AUDENC_ANA_CON15 */
#define RG_AUDDIGMICEN_SFT                               0
#define RG_AUDDIGMICEN_MASK                              0x1
#define RG_AUDDIGMICEN_MASK_SFT                          (0x1 << 0)
#define RG_AUDDIGMICBIAS_SFT                             1
#define RG_AUDDIGMICBIAS_MASK                            0x3
#define RG_AUDDIGMICBIAS_MASK_SFT                        (0x3 << 1)
#define RG_DMICHPCLKEN_SFT                               3
#define RG_DMICHPCLKEN_MASK                              0x1
#define RG_DMICHPCLKEN_MASK_SFT                          (0x1 << 3)
#define RG_AUDDIGMICPDUTY_SFT                            4
#define RG_AUDDIGMICPDUTY_MASK                           0x3
#define RG_AUDDIGMICPDUTY_MASK_SFT                       (0x3 << 4)
#define RG_AUDDIGMICNDUTY_SFT                            6
#define RG_AUDDIGMICNDUTY_MASK                           0x3
#define RG_AUDDIGMICNDUTY_MASK_SFT                       (0x3 << 6)

/* AUDENC_ANA_CON16 */
#define RG_DMICMONEN_SFT                                 0
#define RG_DMICMONEN_MASK                                0x1
#define RG_DMICMONEN_MASK_SFT                            (0x1 << 0)
#define RG_DMICMONSEL_SFT                                1
#define RG_DMICMONSEL_MASK                               0x7
#define RG_DMICMONSEL_MASK_SFT                           (0x7 << 1)
#define RG_AUDSPAREVMIC_SFT                              4
#define RG_AUDSPAREVMIC_MASK                             0xf
#define RG_AUDSPAREVMIC_MASK_SFT                         (0xf << 4)

/* AUDENC_ANA_CON17 */
#define RG_AUDPWDBMICBIAS0_SFT                           0
#define RG_AUDPWDBMICBIAS0_MASK                          0x1
#define RG_AUDPWDBMICBIAS0_MASK_SFT                      (0x1 << 0)
#define RG_AUDMICBIAS0BYPASSEN_SFT                       1
#define RG_AUDMICBIAS0BYPASSEN_MASK                      0x1
#define RG_AUDMICBIAS0BYPASSEN_MASK_SFT                  (0x1 << 1)
#define RG_AUDMICBIAS0LOWPEN_SFT                         2
#define RG_AUDMICBIAS0LOWPEN_MASK                        0x1
#define RG_AUDMICBIAS0LOWPEN_MASK_SFT                    (0x1 << 2)
#define RG_AUDMICBIAS0VREF_SFT                           4
#define RG_AUDMICBIAS0VREF_MASK                          0x7
#define RG_AUDMICBIAS0VREF_MASK_SFT                      (0x7 << 4)

/* AUDENC_ANA_CON18 */
#define RG_AUDMICBIAS0DCSW0P1EN_SFT                      0
#define RG_AUDMICBIAS0DCSW0P1EN_MASK                     0x1
#define RG_AUDMICBIAS0DCSW0P1EN_MASK_SFT                 (0x1 << 0)
#define RG_AUDMICBIAS0DCSW0P2EN_SFT                      1
#define RG_AUDMICBIAS0DCSW0P2EN_MASK                     0x1
#define RG_AUDMICBIAS0DCSW0P2EN_MASK_SFT                 (0x1 << 1)
#define RG_AUDMICBIAS0DCSW0NEN_SFT                       2
#define RG_AUDMICBIAS0DCSW0NEN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW0NEN_MASK_SFT                  (0x1 << 2)
#define RG_AUDMICBIAS0DCSW2P1EN_SFT                      4
#define RG_AUDMICBIAS0DCSW2P1EN_MASK                     0x1
#define RG_AUDMICBIAS0DCSW2P1EN_MASK_SFT                 (0x1 << 4)
#define RG_AUDMICBIAS0DCSW2P2EN_SFT                      5
#define RG_AUDMICBIAS0DCSW2P2EN_MASK                     0x1
#define RG_AUDMICBIAS0DCSW2P2EN_MASK_SFT                 (0x1 << 5)
#define RG_AUDMICBIAS0DCSW2NEN_SFT                       6
#define RG_AUDMICBIAS0DCSW2NEN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW2NEN_MASK_SFT                  (0x1 << 6)

/* AUDENC_ANA_CON19 */
#define RG_AUDPWDBMICBIAS1_SFT                           0
#define RG_AUDPWDBMICBIAS1_MASK                          0x1
#define RG_AUDPWDBMICBIAS1_MASK_SFT                      (0x1 << 0)
#define RG_AUDMICBIAS1BYPASSEN_SFT                       1
#define RG_AUDMICBIAS1BYPASSEN_MASK                      0x1
#define RG_AUDMICBIAS1BYPASSEN_MASK_SFT                  (0x1 << 1)
#define RG_AUDMICBIAS1LOWPEN_SFT                         2
#define RG_AUDMICBIAS1LOWPEN_MASK                        0x1
#define RG_AUDMICBIAS1LOWPEN_MASK_SFT                    (0x1 << 2)
#define RG_AUDMICBIAS1VREF_SFT                           4
#define RG_AUDMICBIAS1VREF_MASK                          0x7
#define RG_AUDMICBIAS1VREF_MASK_SFT                      (0x7 << 4)

/* AUDENC_ANA_CON20 */
#define RG_AUDMICBIAS1DCSW1PEN_SFT                       0
#define RG_AUDMICBIAS1DCSW1PEN_MASK                      0x1
#define RG_AUDMICBIAS1DCSW1PEN_MASK_SFT                  (0x1 << 0)
#define RG_AUDMICBIAS1DCSW1NEN_SFT                       1
#define RG_AUDMICBIAS1DCSW1NEN_MASK                      0x1
#define RG_AUDMICBIAS1DCSW1NEN_MASK_SFT                  (0x1 << 1)
#define RG_BANDGAPGEN_SFT                                5
#define RG_BANDGAPGEN_MASK                               0x1
#define RG_BANDGAPGEN_MASK_SFT                           (0x1 << 5)

/* AUDENC_ANA_CON21 */
#define RG_AUDACCDETMICBIAS0PULLLOW_SFT                  0
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK                 0x1
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT             (0x1 << 0)
#define RG_AUDACCDETMICBIAS1PULLLOW_SFT                  1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK                 0x1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT             (0x1 << 1)
#define RG_AUDACCDETVIN1PULLLOW_SFT                      2
#define RG_AUDACCDETVIN1PULLLOW_MASK                     0x1
#define RG_AUDACCDETVIN1PULLLOW_MASK_SFT                 (0x1 << 2)
#define RG_AUDACCDETVTHACAL_SFT                          4
#define RG_AUDACCDETVTHACAL_MASK                         0x1
#define RG_AUDACCDETVTHACAL_MASK_SFT                     (0x1 << 4)
#define RG_AUDACCDETVTHBCAL_SFT                          5
#define RG_AUDACCDETVTHBCAL_MASK                         0x1
#define RG_AUDACCDETVTHBCAL_MASK_SFT                     (0x1 << 5)
#define RG_AUDACCDETTVDET_SFT                            6
#define RG_AUDACCDETTVDET_MASK                           0x1
#define RG_AUDACCDETTVDET_MASK_SFT                       (0x1 << 6)
#define RG_ACCDETSEL_SFT                                 7
#define RG_ACCDETSEL_MASK                                0x1
#define RG_ACCDETSEL_MASK_SFT                            (0x1 << 7)

/* AUDENC_ANA_CON22 */
#define RG_SWBUFMODSEL_SFT                               0
#define RG_SWBUFMODSEL_MASK                              0x1
#define RG_SWBUFMODSEL_MASK_SFT                          (0x1 << 0)
#define RG_SWBUFSWEN_SFT                                 1
#define RG_SWBUFSWEN_MASK                                0x1
#define RG_SWBUFSWEN_MASK_SFT                            (0x1 << 1)
#define RG_EINT0NOHYS_SFT                                2
#define RG_EINT0NOHYS_MASK                               0x1
#define RG_EINT0NOHYS_MASK_SFT                           (0x1 << 2)
#define RG_EINT0CONFIGACCDET_SFT                         3
#define RG_EINT0CONFIGACCDET_MASK                        0x1
#define RG_EINT0CONFIGACCDET_MASK_SFT                    (0x1 << 3)
#define RG_EINT0HIRENB_SFT                               4
#define RG_EINT0HIRENB_MASK                              0x1
#define RG_EINT0HIRENB_MASK_SFT                          (0x1 << 4)
#define RG_ACCDET2AUXRESBYPASS_SFT                       5
#define RG_ACCDET2AUXRESBYPASS_MASK                      0x1
#define RG_ACCDET2AUXRESBYPASS_MASK_SFT                  (0x1 << 5)
#define RG_ACCDET2AUXSWEN_SFT                            6
#define RG_ACCDET2AUXSWEN_MASK                           0x1
#define RG_ACCDET2AUXSWEN_MASK_SFT                       (0x1 << 6)
#define RG_ACCDET2AUXBUFFERBYPASS_SFT                    7
#define RG_ACCDET2AUXBUFFERBYPASS_MASK                   0x1
#define RG_ACCDET2AUXBUFFERBYPASS_MASK_SFT               (0x1 << 7)

/* AUDENC_ANA_CON23 */
#define RG_EINT1CONFIGACCDET_SFT                         0
#define RG_EINT1CONFIGACCDET_MASK                        0x1
#define RG_EINT1CONFIGACCDET_MASK_SFT                    (0x1 << 0)
#define RG_EINT1HIRENB_SFT                               1
#define RG_EINT1HIRENB_MASK                              0x1
#define RG_EINT1HIRENB_MASK_SFT                          (0x1 << 1)
#define RG_EINT1NOHYS_SFT                                2
#define RG_EINT1NOHYS_MASK                               0x1
#define RG_EINT1NOHYS_MASK_SFT                           (0x1 << 2)
#define RG_EINTCOMPVTH_SFT                               4
#define RG_EINTCOMPVTH_MASK                              0xf
#define RG_EINTCOMPVTH_MASK_SFT                          (0xf << 4)

/* AUDENC_ANA_CON24 */
#define RG_MTEST_EN_SFT                                  0
#define RG_MTEST_EN_MASK                                 0x1
#define RG_MTEST_EN_MASK_SFT                             (0x1 << 0)
#define RG_MTEST_SEL_SFT                                 1
#define RG_MTEST_SEL_MASK                                0x1
#define RG_MTEST_SEL_MASK_SFT                            (0x1 << 1)
#define RG_MTEST_CURRENT_SFT                             2
#define RG_MTEST_CURRENT_MASK                            0x1
#define RG_MTEST_CURRENT_MASK_SFT                        (0x1 << 2)
#define RG_ANALOGFDEN_SFT                                4
#define RG_ANALOGFDEN_MASK                               0x1
#define RG_ANALOGFDEN_MASK_SFT                           (0x1 << 4)
#define RG_FDVIN1PPULLLOW_SFT                            5
#define RG_FDVIN1PPULLLOW_MASK                           0x1
#define RG_FDVIN1PPULLLOW_MASK_SFT                       (0x1 << 5)
#define RG_FDEINT0TYPE_SFT                               6
#define RG_FDEINT0TYPE_MASK                              0x1
#define RG_FDEINT0TYPE_MASK_SFT                          (0x1 << 6)
#define RG_FDEINT1TYPE_SFT                               7
#define RG_FDEINT1TYPE_MASK                              0x1
#define RG_FDEINT1TYPE_MASK_SFT                          (0x1 << 7)

/* AUDENC_ANA_CON25 */
#define RG_EINT0CMPEN_SFT                                0
#define RG_EINT0CMPEN_MASK                               0x1
#define RG_EINT0CMPEN_MASK_SFT                           (0x1 << 0)
#define RG_EINT0CMPMEN_SFT                               1
#define RG_EINT0CMPMEN_MASK                              0x1
#define RG_EINT0CMPMEN_MASK_SFT                          (0x1 << 1)
#define RG_EINT0EN_SFT                                   2
#define RG_EINT0EN_MASK                                  0x1
#define RG_EINT0EN_MASK_SFT                              (0x1 << 2)
#define RG_EINT0CEN_SFT                                  3
#define RG_EINT0CEN_MASK                                 0x1
#define RG_EINT0CEN_MASK_SFT                             (0x1 << 3)
#define RG_EINT0INVEN_SFT                                4
#define RG_EINT0INVEN_MASK                               0x1
#define RG_EINT0INVEN_MASK_SFT                           (0x1 << 4)

/* AUDENC_ANA_CON26 */
#define RG_EINT1CMPEN_SFT                                0
#define RG_EINT1CMPEN_MASK                               0x1
#define RG_EINT1CMPEN_MASK_SFT                           (0x1 << 0)
#define RG_EINT1CMPMEN_SFT                               1
#define RG_EINT1CMPMEN_MASK                              0x1
#define RG_EINT1CMPMEN_MASK_SFT                          (0x1 << 1)
#define RG_EINT1EN_SFT                                   2
#define RG_EINT1EN_MASK                                  0x1
#define RG_EINT1EN_MASK_SFT                              (0x1 << 2)
#define RG_EINT1CEN_SFT                                  3
#define RG_EINT1CEN_MASK                                 0x1
#define RG_EINT1CEN_MASK_SFT                             (0x1 << 3)
#define RG_EINT1INVEN_SFT                                4
#define RG_EINT1INVEN_MASK                               0x1
#define RG_EINT1INVEN_MASK_SFT                           (0x1 << 4)

/* AUDENC_ANA_CON27 */
#define RG_EINT0CTURBO_SFT                               0
#define RG_EINT0CTURBO_MASK                              0x1f
#define RG_EINT0CTURBO_MASK_SFT                          (0x1f << 0)

/* AUDENC_ANA_CON28 */
#define RG_EINT1CTURBO_SFT                               0
#define RG_EINT1CTURBO_MASK                              0x1f
#define RG_EINT1CTURBO_MASK_SFT                          (0x1f << 0)

/* AUDENC_ANA_CON29 */
#define RG_EINT0CSEL_SFT                                 0
#define RG_EINT0CSEL_MASK                                0x3
#define RG_EINT0CSEL_MASK_SFT                            (0x3 << 0)
#define RG_MVTHSEL_SFT                                   2
#define RG_MVTHSEL_MASK                                  0x1f
#define RG_MVTHSEL_MASK_SFT                              (0x1f << 2)
#define RG_MVTH2EN_SFT                                   7
#define RG_MVTH2EN_MASK                                  0x1
#define RG_MVTH2EN_MASK_SFT                              (0x1 << 7)

/* AUDENC_ANA_CON30 */
#define RG_MVTH2SEL_SFT                                  0
#define RG_MVTH2SEL_MASK                                 0xf
#define RG_MVTH2SEL_MASK_SFT                             (0xf << 0)

/* AUDENC_ANA_CON31 */
#define RG_ACCDETSPARE_L_SFT                             0
#define RG_ACCDETSPARE_L_MASK                            0xff
#define RG_ACCDETSPARE_L_MASK_SFT                        (0xff << 0)

/* AUDENC_ANA_CON32 */
#define RG_ACCDETSPARE_H_SFT                             0
#define RG_ACCDETSPARE_H_MASK                            0xff
#define RG_ACCDETSPARE_H_MASK_SFT                        (0xff << 0)

/* AUDENC_ANA_CON33 */
#define RGS_AUDRCTUNELREAD_SFT                           0
#define RGS_AUDRCTUNELREAD_MASK                          0x1f
#define RGS_AUDRCTUNELREAD_MASK_SFT                      (0x1f << 0)

/* AUDENC_ANA_CON34 */
#define RGS_AUDRCTUNERREAD_SFT                           0
#define RGS_AUDRCTUNERREAD_MASK                          0x1f
#define RGS_AUDRCTUNERREAD_MASK_SFT                      (0x1f << 0)

/* VOWPLL_ANA_CON0 */
#define RG_PLL_EN_SFT                                    0
#define RG_PLL_EN_MASK                                   0x1
#define RG_PLL_EN_MASK_SFT                               (0x1 << 0)
#define RG_PLLBS_RST_SFT                                 1
#define RG_PLLBS_RST_MASK                                0x1
#define RG_PLLBS_RST_MASK_SFT                            (0x1 << 1)
#define RG_PLL_DCKO_SEL_SFT                              2
#define RG_PLL_DCKO_SEL_MASK                             0x3
#define RG_PLL_DCKO_SEL_MASK_SFT                         (0x3 << 2)

/* VOWPLL_ANA_CON1 */
#define RG_PLL_DIV1_SFT                                  0
#define RG_PLL_DIV1_MASK                                 0x3f
#define RG_PLL_DIV1_MASK_SFT                             (0x3f << 0)

/* VOWPLL_ANA_CON2 */
#define RG_PLL_RLATCH_EN_SFT                             0
#define RG_PLL_RLATCH_EN_MASK                            0x1
#define RG_PLL_RLATCH_EN_MASK_SFT                        (0x1 << 0)
#define RG_PLL_PDIV1_EN_SFT                              1
#define RG_PLL_PDIV1_EN_MASK                             0x1
#define RG_PLL_PDIV1_EN_MASK_SFT                         (0x1 << 1)
#define RG_PLL_PDIV1_SFT                                 4
#define RG_PLL_PDIV1_MASK                                0xf
#define RG_PLL_PDIV1_MASK_SFT                            (0xf << 4)

/* VOWPLL_ANA_CON3 */
#define RG_PLL_BC_SFT                                    0
#define RG_PLL_BC_MASK                                   0x3
#define RG_PLL_BC_MASK_SFT                               (0x3 << 0)
#define RG_PLL_BP_SFT                                    2
#define RG_PLL_BP_MASK                                   0x3
#define RG_PLL_BP_MASK_SFT                               (0x3 << 2)
#define RG_PLL_BR_SFT                                    4
#define RG_PLL_BR_MASK                                   0x3
#define RG_PLL_BR_MASK_SFT                               (0x3 << 4)
#define RG_CKO_SEL_SFT                                   6
#define RG_CKO_SEL_MASK                                  0x3
#define RG_CKO_SEL_MASK_SFT                              (0x3 << 6)

/* VOWPLL_ANA_CON4 */
#define RG_PLL_IBSEL_SFT                                 0
#define RG_PLL_IBSEL_MASK                                0x3
#define RG_PLL_IBSEL_MASK_SFT                            (0x3 << 0)
#define RG_PLL_CKT_SEL_SFT                               2
#define RG_PLL_CKT_SEL_MASK                              0x3
#define RG_PLL_CKT_SEL_MASK_SFT                          (0x3 << 2)
#define RG_PLL_VCT_EN_SFT                                4
#define RG_PLL_VCT_EN_MASK                               0x1
#define RG_PLL_VCT_EN_MASK_SFT                           (0x1 << 4)
#define RG_PLL_CKT_EN_SFT                                5
#define RG_PLL_CKT_EN_MASK                               0x1
#define RG_PLL_CKT_EN_MASK_SFT                           (0x1 << 5)
#define RG_PLL_HPM_EN_SFT                                6
#define RG_PLL_HPM_EN_MASK                               0x1
#define RG_PLL_HPM_EN_MASK_SFT                           (0x1 << 6)
#define RG_PLL_DCHP_EN_SFT                               7
#define RG_PLL_DCHP_EN_MASK                              0x1
#define RG_PLL_DCHP_EN_MASK_SFT                          (0x1 << 7)

/* VOWPLL_ANA_CON5 */
#define RG_PLL_CDIV_SFT                                  0
#define RG_PLL_CDIV_MASK                                 0x7
#define RG_PLL_CDIV_MASK_SFT                             (0x7 << 0)
#define RG_VCOBAND_SFT                                   4
#define RG_VCOBAND_MASK                                  0x7
#define RG_VCOBAND_MASK_SFT                              (0x7 << 4)

/* VOWPLL_ANA_CON6 */
#define RG_CKDRV_EN_SFT                                  0
#define RG_CKDRV_EN_MASK                                 0x1
#define RG_CKDRV_EN_MASK_SFT                             (0x1 << 0)
#define RG_PLL_DCHP_AEN_SFT                              1
#define RG_PLL_DCHP_AEN_MASK                             0x1
#define RG_PLL_DCHP_AEN_MASK_SFT                         (0x1 << 1)

/* VOWPLL_ANA_CON7 */
#define RG_PLL_RSVA_SFT                                  0
#define RG_PLL_RSVA_MASK                                 0xff
#define RG_PLL_RSVA_MASK_SFT                             (0xff << 0)

/* VOWPLL_ANA_CON8 */
#define RG_PLL_RSVB_SFT                                  0
#define RG_PLL_RSVB_MASK                                 0xff
#define RG_PLL_RSVB_MASK_SFT                             (0xff << 0)

/* AUDDEC_ANA_ID */
#define AUDDEC_ANA_ID_SFT                                0
#define AUDDEC_ANA_ID_MASK                               0xff
#define AUDDEC_ANA_ID_MASK_SFT                           (0xff << 0)

/* AUDDEC_DIG_ID */
#define AUDDEC_DIG_ID_SFT                                0
#define AUDDEC_DIG_ID_MASK                               0xff
#define AUDDEC_DIG_ID_MASK_SFT                           (0xff << 0)

/* AUDDEC_ANA_REV */
#define AUDDEC_ANA_MINOR_REV_SFT                         0
#define AUDDEC_ANA_MINOR_REV_MASK                        0xf
#define AUDDEC_ANA_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDDEC_ANA_MAJOR_REV_SFT                         4
#define AUDDEC_ANA_MAJOR_REV_MASK                        0xf
#define AUDDEC_ANA_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDDEC_DIG_REV */
#define AUDDEC_DIG_MINOR_REV_SFT                         0
#define AUDDEC_DIG_MINOR_REV_MASK                        0xf
#define AUDDEC_DIG_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDDEC_DIG_MAJOR_REV_SFT                         4
#define AUDDEC_DIG_MAJOR_REV_MASK                        0xf
#define AUDDEC_DIG_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDDEC_DBI */
#define AUDDEC_CBS_SFT                                   0
#define AUDDEC_CBS_MASK                                  0x3
#define AUDDEC_CBS_MASK_SFT                              (0x3 << 0)
#define AUDDEC_BIX_SFT                                   2
#define AUDDEC_BIX_MASK                                  0x3
#define AUDDEC_BIX_MASK_SFT                              (0x3 << 2)

/* AUDDEC_ESP */
#define AUDDEC_ESP_SFT                                   0
#define AUDDEC_ESP_MASK                                  0xff
#define AUDDEC_ESP_MASK_SFT                              (0xff << 0)

/* AUDDEC_FPI */
#define AUDDEC_FPI_SFT                                   0
#define AUDDEC_FPI_MASK                                  0xff
#define AUDDEC_FPI_MASK_SFT                              (0xff << 0)

/* AUDDEC_DXI */
#define AUDDEC_DXI_SFT                                   0
#define AUDDEC_DXI_MASK                                  0xff
#define AUDDEC_DXI_MASK_SFT                              (0xff << 0)

/* AUDDEC_ANA_CON0 */
#define RG_AUDDACLPWRUP_VAUDP15_SFT                      0
#define RG_AUDDACLPWRUP_VAUDP15_MASK                     0x1
#define RG_AUDDACLPWRUP_VAUDP15_MASK_SFT                 (0x1 << 0)
#define RG_AUDDACRPWRUP_VAUDP15_SFT                      1
#define RG_AUDDACRPWRUP_VAUDP15_MASK                     0x1
#define RG_AUDDACRPWRUP_VAUDP15_MASK_SFT                 (0x1 << 1)
#define RG_AUD_DAC_PWR_UP_VA28_SFT                       2
#define RG_AUD_DAC_PWR_UP_VA28_MASK                      0x1
#define RG_AUD_DAC_PWR_UP_VA28_MASK_SFT                  (0x1 << 2)
#define RG_AUD_DAC_PWL_UP_VA28_SFT                       3
#define RG_AUD_DAC_PWL_UP_VA28_MASK                      0x1
#define RG_AUD_DAC_PWL_UP_VA28_MASK_SFT                  (0x1 << 3)
#define RG_AUDHPLPWRUP_VAUDP15_SFT                       4
#define RG_AUDHPLPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDHPLPWRUP_VAUDP15_MASK_SFT                  (0x1 << 4)
#define RG_AUDHPRPWRUP_VAUDP15_SFT                       5
#define RG_AUDHPRPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDHPRPWRUP_VAUDP15_MASK_SFT                  (0x1 << 5)
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_SFT                 6
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK                0x1
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK_SFT            (0x1 << 6)
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_SFT                 7
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK                0x1
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK_SFT            (0x1 << 7)

/* AUDDEC_ANA_CON1 */
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_SFT                 0
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK                0x3
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK_SFT            (0x3 << 0)
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_SFT                 2
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK                0x3
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK_SFT            (0x3 << 2)
#define RG_AUDHPLSCDISABLE_VAUDP15_SFT                   4
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK                  0x1
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK_SFT              (0x1 << 4)
#define RG_AUDHPRSCDISABLE_VAUDP15_SFT                   5
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK                  0x1
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK_SFT              (0x1 << 5)
#define RG_AUDHPLBSCCURRENT_VAUDP15_SFT                  6
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK                 0x1
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK_SFT             (0x1 << 6)
#define RG_AUDHPRBSCCURRENT_VAUDP15_SFT                  7
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK                 0x1
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK_SFT             (0x1 << 7)

/* AUDDEC_ANA_CON2 */
#define RG_AUDHPLOUTPWRUP_VAUDP15_SFT                    0
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK                   0x1
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK_SFT               (0x1 << 0)
#define RG_AUDHPROUTPWRUP_VAUDP15_SFT                    1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK                   0x1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK_SFT               (0x1 << 1)
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_SFT                 2
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK                0x1
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK_SFT            (0x1 << 2)
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_SFT                 3
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK                0x1
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK_SFT            (0x1 << 3)
#define RG_HPLAUXFBRSW_EN_VAUDP15_SFT                    4
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK                   0x1
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK_SFT               (0x1 << 4)
#define RG_HPRAUXFBRSW_EN_VAUDP15_SFT                    5
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK                   0x1
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK_SFT               (0x1 << 5)
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_SFT                6
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK               0x1
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK_SFT           (0x1 << 6)
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_SFT                7
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK               0x1
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK_SFT           (0x1 << 7)

/* AUDDEC_ANA_CON3 */
#define RG_HPLOUTSTGCTRL_VAUDP15_SFT                     0
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK                    0x7
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK_SFT                (0x7 << 0)
#define RG_HPROUTSTGCTRL_VAUDP15_SFT                     4
#define RG_HPROUTSTGCTRL_VAUDP15_MASK                    0x7
#define RG_HPROUTSTGCTRL_VAUDP15_MASK_SFT                (0x7 << 4)

/* AUDDEC_ANA_CON4 */
#define RG_HPLOUTPUTSTBENH_VAUDP15_SFT                   0
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK                  0x7
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK_SFT              (0x7 << 0)
#define RG_HPROUTPUTSTBENH_VAUDP15_SFT                   4
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK                  0x7
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK_SFT              (0x7 << 4)
#define RG_AUDHPSTARTUP_VAUDP15_SFT                      7
#define RG_AUDHPSTARTUP_VAUDP15_MASK                     0x1
#define RG_AUDHPSTARTUP_VAUDP15_MASK_SFT                 (0x1 << 7)

/* AUDDEC_ANA_CON5 */
#define RG_AUDREFN_DERES_EN_VAUDP15_SFT                  0
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK                 0x1
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK_SFT             (0x1 << 0)
#define RG_HPPSHORT2VCM_VAUDP15_SFT                      1
#define RG_HPPSHORT2VCM_VAUDP15_MASK                     0x1
#define RG_HPPSHORT2VCM_VAUDP15_MASK_SFT                 (0x1 << 1)
#define RG_HPINPUTSTBENH_VAUDP15_SFT                     2
#define RG_HPINPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_HPINPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 2)
#define RG_HPINPUTRESET0_VAUDP15_SFT                     3
#define RG_HPINPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HPINPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 3)
#define RG_HPOUTPUTRESET0_VAUDP15_SFT                    4
#define RG_HPOUTPUTRESET0_VAUDP15_MASK                   0x1
#define RG_HPOUTPUTRESET0_VAUDP15_MASK_SFT               (0x1 << 4)

/* AUDDEC_ANA_CON6 */
#define RG_AUDHPLTRIM_VAUDP15_SFT                        0
#define RG_AUDHPLTRIM_VAUDP15_MASK                       0xf
#define RG_AUDHPLTRIM_VAUDP15_MASK_SFT                   (0xf << 0)
#define RG_AUDHPRTRIM_VAUDP15_SFT                        4
#define RG_AUDHPRTRIM_VAUDP15_MASK                       0xf
#define RG_AUDHPRTRIM_VAUDP15_MASK_SFT                   (0xf << 4)

/* AUDDEC_ANA_CON7 */
#define RG_AUDHPLFINETRIM_VAUDP15_SFT                    0
#define RG_AUDHPLFINETRIM_VAUDP15_MASK                   0x3
#define RG_AUDHPLFINETRIM_VAUDP15_MASK_SFT               (0x3 << 0)
#define RG_AUDHPRFINETRIM_VAUDP15_SFT                    2
#define RG_AUDHPRFINETRIM_VAUDP15_MASK                   0x3
#define RG_AUDHPRFINETRIM_VAUDP15_MASK_SFT               (0x3 << 2)
#define RG_AUDHPTRIM_EN_VAUDP15_SFT                      4
#define RG_AUDHPTRIM_EN_VAUDP15_MASK                     0x1
#define RG_AUDHPTRIM_EN_VAUDP15_MASK_SFT                 (0x1 << 4)

/* AUDDEC_ANA_CON8 */
#define RG_ABIDEC_RSVD0_VAUDP28_SFT                      0
#define RG_ABIDEC_RSVD0_VAUDP28_MASK                     0xff
#define RG_ABIDEC_RSVD0_VAUDP28_MASK_SFT                 (0xff << 0)

/* AUDDEC_ANA_CON9 */
#define RG_AUDHPDECMGAINADJ_VAUDP15_SFT                  0
#define RG_AUDHPDECMGAINADJ_VAUDP15_MASK                 0x7
#define RG_AUDHPDECMGAINADJ_VAUDP15_MASK_SFT             (0x7 << 0)
#define RG_AUDHPDEDMGAINADJ_VAUDP15_SFT                  4
#define RG_AUDHPDEDMGAINADJ_VAUDP15_MASK                 0x7
#define RG_AUDHPDEDMGAINADJ_VAUDP15_MASK_SFT             (0x7 << 4)

/* AUDDEC_ANA_CON10 */
#define RG_AUDHSPWRUP_VAUDP15_SFT                        0
#define RG_AUDHSPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDHSPWRUP_VAUDP15_MASK_SFT                   (0x1 << 0)
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_SFT                  1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 1)
#define RG_AUDHSMUXINPUTSEL_VAUDP15_SFT                  2
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 2)
#define RG_AUDHSSCDISABLE_VAUDP15_SFT                    4
#define RG_AUDHSSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDHSSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 4)
#define RG_AUDHSBSCCURRENT_VAUDP15_SFT                   5
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 5)
#define RG_AUDHSSTARTUP_VAUDP15_SFT                      6
#define RG_AUDHSSTARTUP_VAUDP15_MASK                     0x1
#define RG_AUDHSSTARTUP_VAUDP15_MASK_SFT                 (0x1 << 6)
#define RG_HSOUTPUTSTBENH_VAUDP15_SFT                    7
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK                   0x1
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK_SFT               (0x1 << 7)

/* AUDDEC_ANA_CON11 */
#define RG_HSINPUTSTBENH_VAUDP15_SFT                     0
#define RG_HSINPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_HSINPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 0)
#define RG_HSINPUTRESET0_VAUDP15_SFT                     1
#define RG_HSINPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HSINPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 1)
#define RG_HSOUTPUTRESET0_VAUDP15_SFT                    2
#define RG_HSOUTPUTRESET0_VAUDP15_MASK                   0x1
#define RG_HSOUTPUTRESET0_VAUDP15_MASK_SFT               (0x1 << 2)
#define RG_HSOUT_SHORTVCM_VAUDP15_SFT                    3
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK                   0x1
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK_SFT               (0x1 << 3)

/* AUDDEC_ANA_CON12 */
#define RG_AUDLOLPWRUP_VAUDP15_SFT                       0
#define RG_AUDLOLPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDLOLPWRUP_VAUDP15_MASK_SFT                  (0x1 << 0)
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_SFT                 1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK                0x1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK_SFT            (0x1 << 1)
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_SFT                 2
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK                0x3
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK_SFT            (0x3 << 2)
#define RG_AUDLOLSCDISABLE_VAUDP15_SFT                   4
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK                  0x1
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK_SFT              (0x1 << 4)
#define RG_AUDLOLBSCCURRENT_VAUDP15_SFT                  5
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK                 0x1
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK_SFT             (0x1 << 5)
#define RG_AUDLOSTARTUP_VAUDP15_SFT                      6
#define RG_AUDLOSTARTUP_VAUDP15_MASK                     0x1
#define RG_AUDLOSTARTUP_VAUDP15_MASK_SFT                 (0x1 << 6)
#define RG_LOINPUTSTBENH_VAUDP15_SFT                     7
#define RG_LOINPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_LOINPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 7)

/* AUDDEC_ANA_CON13 */
#define RG_LOOUTPUTSTBENH_VAUDP15_SFT                    0
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK                   0x1
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK_SFT               (0x1 << 0)
#define RG_LOINPUTRESET0_VAUDP15_SFT                     1
#define RG_LOINPUTRESET0_VAUDP15_MASK                    0x1
#define RG_LOINPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 1)
#define RG_LOOUTPUTRESET0_VAUDP15_SFT                    2
#define RG_LOOUTPUTRESET0_VAUDP15_MASK                   0x1
#define RG_LOOUTPUTRESET0_VAUDP15_MASK_SFT               (0x1 << 2)
#define RG_LOOUT_SHORTVCM_VAUDP15_SFT                    3
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK                   0x1
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK_SFT               (0x1 << 3)

/* AUDDEC_ANA_CON14 */
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_SFT            0
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK           0xf
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK_SFT       (0xf << 0)
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_SFT                4
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK               0x3
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK_SFT           (0x3 << 4)
#define RG_AUDTRIMBUF_EN_VAUDP15_SFT                     6
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK                    0x1
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK_SFT                (0x1 << 6)

/* AUDDEC_ANA_CON15 */
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_SFT           0
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK          0x3
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK_SFT      (0x3 << 0)
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_SFT          2
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK         0x3
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK_SFT     (0x3 << 2)
#define RG_AUDHPSPKDET_EN_VAUDP15_SFT                    4
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK                   0x1
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK_SFT               (0x1 << 4)

/* AUDDEC_ANA_CON16 */
#define RG_ABIDEC_RSVD0_VA28_SFT                         0
#define RG_ABIDEC_RSVD0_VA28_MASK                        0xff
#define RG_ABIDEC_RSVD0_VA28_MASK_SFT                    (0xff << 0)

/* AUDDEC_ANA_CON17 */
#define RG_ABIDEC_RSVD0_VAUDP15_SFT                      0
#define RG_ABIDEC_RSVD0_VAUDP15_MASK                     0xff
#define RG_ABIDEC_RSVD0_VAUDP15_MASK_SFT                 (0xff << 0)

/* AUDDEC_ANA_CON18 */
#define RG_ABIDEC_RSVD1_VAUDP15_SFT                      0
#define RG_ABIDEC_RSVD1_VAUDP15_MASK                     0xff
#define RG_ABIDEC_RSVD1_VAUDP15_MASK_SFT                 (0xff << 0)

/* AUDDEC_ANA_CON19 */
#define RG_ABIDEC_RSVD2_VAUDP15_SFT                      0
#define RG_ABIDEC_RSVD2_VAUDP15_MASK                     0xff
#define RG_ABIDEC_RSVD2_VAUDP15_MASK_SFT                 (0xff << 0)

/* AUDDEC_ANA_CON20 */
#define RG_AUDZCDMUXSEL_VAUDP15_SFT                      0
#define RG_AUDZCDMUXSEL_VAUDP15_MASK                     0x7
#define RG_AUDZCDMUXSEL_VAUDP15_MASK_SFT                 (0x7 << 0)
#define RG_AUDZCDCLKSEL_VAUDP15_SFT                      3
#define RG_AUDZCDCLKSEL_VAUDP15_MASK                     0x1
#define RG_AUDZCDCLKSEL_VAUDP15_MASK_SFT                 (0x1 << 3)

/* AUDDEC_ANA_CON21 */
#define RG_AUDBIASADJ_0_HP_VAUDP15_SFT                   0
#define RG_AUDBIASADJ_0_HP_VAUDP15_MASK                  0x7
#define RG_AUDBIASADJ_0_HP_VAUDP15_MASK_SFT              (0x7 << 0)
#define RG_AUDBIASADJ_0_HS_VAUDP15_SFT                   4
#define RG_AUDBIASADJ_0_HS_VAUDP15_MASK                  0x7
#define RG_AUDBIASADJ_0_HS_VAUDP15_MASK_SFT              (0x7 << 4)

/* AUDDEC_ANA_CON22 */
#define RG_AUDBIASADJ_0_LO_VAUDP15_SFT                   0
#define RG_AUDBIASADJ_0_LO_VAUDP15_MASK                  0x7
#define RG_AUDBIASADJ_0_LO_VAUDP15_MASK_SFT              (0x7 << 0)
#define RG_AUDIBIASPWRDN_VAUDP15_SFT                     4
#define RG_AUDIBIASPWRDN_VAUDP15_MASK                    0x1
#define RG_AUDIBIASPWRDN_VAUDP15_MASK_SFT                (0x1 << 4)

/* AUDDEC_ANA_CON23 */
#define RG_AUDBIASADJ_1_VAUDP15_SFT                      0
#define RG_AUDBIASADJ_1_VAUDP15_MASK                     0xff
#define RG_AUDBIASADJ_1_VAUDP15_MASK_SFT                 (0xff << 0)

/* AUDDEC_ANA_CON24 */
#define RG_RSTB_DECODER_VA28_SFT                         0
#define RG_RSTB_DECODER_VA28_MASK                        0x1
#define RG_RSTB_DECODER_VA28_MASK_SFT                    (0x1 << 0)
#define RG_SEL_DECODER_96K_VA28_SFT                      1
#define RG_SEL_DECODER_96K_VA28_MASK                     0x1
#define RG_SEL_DECODER_96K_VA28_MASK_SFT                 (0x1 << 1)
#define RG_SEL_DELAY_VCORE_SFT                           2
#define RG_SEL_DELAY_VCORE_MASK                          0x1
#define RG_SEL_DELAY_VCORE_MASK_SFT                      (0x1 << 2)
#define RG_AUDGLB_PWRDN_VA28_SFT                         4
#define RG_AUDGLB_PWRDN_VA28_MASK                        0x1
#define RG_AUDGLB_PWRDN_VA28_MASK_SFT                    (0x1 << 4)
#define RG_RSTB_ENCODER_VA28_SFT                         5
#define RG_RSTB_ENCODER_VA28_MASK                        0x1
#define RG_RSTB_ENCODER_VA28_MASK_SFT                    (0x1 << 5)
#define RG_SEL_ENCODER_96K_VA28_SFT                      6
#define RG_SEL_ENCODER_96K_VA28_MASK                     0x1
#define RG_SEL_ENCODER_96K_VA28_MASK_SFT                 (0x1 << 6)

/* AUDDEC_ANA_CON25 */
#define RG_HCLDO_EN_VA18_SFT                             0
#define RG_HCLDO_EN_VA18_MASK                            0x1
#define RG_HCLDO_EN_VA18_MASK_SFT                        (0x1 << 0)
#define RG_HCLDO_PDDIS_EN_VA18_SFT                       1
#define RG_HCLDO_PDDIS_EN_VA18_MASK                      0x1
#define RG_HCLDO_PDDIS_EN_VA18_MASK_SFT                  (0x1 << 1)
#define RG_HCLDO_REMOTE_SENSE_VA18_SFT                   2
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK                  0x1
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK_SFT              (0x1 << 2)
#define RG_LCLDO_EN_VA18_SFT                             4
#define RG_LCLDO_EN_VA18_MASK                            0x1
#define RG_LCLDO_EN_VA18_MASK_SFT                        (0x1 << 4)
#define RG_LCLDO_PDDIS_EN_VA18_SFT                       5
#define RG_LCLDO_PDDIS_EN_VA18_MASK                      0x1
#define RG_LCLDO_PDDIS_EN_VA18_MASK_SFT                  (0x1 << 5)
#define RG_LCLDO_REMOTE_SENSE_VA18_SFT                   6
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK                  0x1
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK_SFT              (0x1 << 6)

/* AUDDEC_ANA_CON26 */
#define RG_LCLDO_ENC_EN_VA28_SFT                         0
#define RG_LCLDO_ENC_EN_VA28_MASK                        0x1
#define RG_LCLDO_ENC_EN_VA28_MASK_SFT                    (0x1 << 0)
#define RG_LCLDO_ENC_PDDIS_EN_VA28_SFT                   1
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK                  0x1
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK_SFT              (0x1 << 1)
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_SFT               2
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK              0x1
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK_SFT          (0x1 << 2)
#define RG_VA33REFGEN_EN_VA18_SFT                        4
#define RG_VA33REFGEN_EN_VA18_MASK                       0x1
#define RG_VA33REFGEN_EN_VA18_MASK_SFT                   (0x1 << 4)
#define RG_VA28REFGEN_EN_VA28_SFT                        5
#define RG_VA28REFGEN_EN_VA28_MASK                       0x1
#define RG_VA28REFGEN_EN_VA28_MASK_SFT                   (0x1 << 5)
#define RG_HCLDO_VOSEL_VA18_SFT                          6
#define RG_HCLDO_VOSEL_VA18_MASK                         0x1
#define RG_HCLDO_VOSEL_VA18_MASK_SFT                     (0x1 << 6)
#define RG_LCLDO_VOSEL_VA18_SFT                          7
#define RG_LCLDO_VOSEL_VA18_MASK                         0x1
#define RG_LCLDO_VOSEL_VA18_MASK_SFT                     (0x1 << 7)

/* AUDDEC_ANA_CON27 */
#define RG_NVREG_EN_VAUDP15_SFT                          0
#define RG_NVREG_EN_VAUDP15_MASK                         0x1
#define RG_NVREG_EN_VAUDP15_MASK_SFT                     (0x1 << 0)
#define RG_NVREG_PULL0V_VAUDP15_SFT                      1
#define RG_NVREG_PULL0V_VAUDP15_MASK                     0x1
#define RG_NVREG_PULL0V_VAUDP15_MASK_SFT                 (0x1 << 1)
#define RG_AUDPMU_RSD0_VAUDP15_SFT                       4
#define RG_AUDPMU_RSD0_VAUDP15_MASK                      0xf
#define RG_AUDPMU_RSD0_VAUDP15_MASK_SFT                  (0xf << 4)

/* AUDDEC_ANA_CON28 */
#define RG_AUDPMU_RSD0_VA18_SFT                          0
#define RG_AUDPMU_RSD0_VA18_MASK                         0xf
#define RG_AUDPMU_RSD0_VA18_MASK_SFT                     (0xf << 0)
#define RG_AUDPMU_RSD0_VA28_SFT                          4
#define RG_AUDPMU_RSD0_VA28_MASK                         0xf
#define RG_AUDPMU_RSD0_VA28_MASK_SFT                     (0xf << 4)

/* AUDZCD_ANA_ID */
#define AUDZCD_ANA_ID_SFT                                0
#define AUDZCD_ANA_ID_MASK                               0xff
#define AUDZCD_ANA_ID_MASK_SFT                           (0xff << 0)

/* AUDZCD_DIG_ID */
#define AUDZCD_DIG_ID_SFT                                0
#define AUDZCD_DIG_ID_MASK                               0xff
#define AUDZCD_DIG_ID_MASK_SFT                           (0xff << 0)

/* AUDZCD_ANA_REV */
#define AUDZCD_ANA_MINOR_REV_SFT                         0
#define AUDZCD_ANA_MINOR_REV_MASK                        0xf
#define AUDZCD_ANA_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDZCD_ANA_MAJOR_REV_SFT                         4
#define AUDZCD_ANA_MAJOR_REV_MASK                        0xf
#define AUDZCD_ANA_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDZCD_DIG_REV */
#define AUDZCD_DIG_MINOR_REV_SFT                         0
#define AUDZCD_DIG_MINOR_REV_MASK                        0xf
#define AUDZCD_DIG_MINOR_REV_MASK_SFT                    (0xf << 0)
#define AUDZCD_DIG_MAJOR_REV_SFT                         4
#define AUDZCD_DIG_MAJOR_REV_MASK                        0xf
#define AUDZCD_DIG_MAJOR_REV_MASK_SFT                    (0xf << 4)

/* AUDZCD_DSN_DBI */
#define AUDZCD_DSN_CBS_SFT                               0
#define AUDZCD_DSN_CBS_MASK                              0x3
#define AUDZCD_DSN_CBS_MASK_SFT                          (0x3 << 0)
#define AUDZCD_DSN_BIX_SFT                               2
#define AUDZCD_DSN_BIX_MASK                              0x3
#define AUDZCD_DSN_BIX_MASK_SFT                          (0x3 << 2)

/* AUDZCD_DSN_ESP */
#define AUDZCD_DSN_ESP_SFT                               0
#define AUDZCD_DSN_ESP_MASK                              0xff
#define AUDZCD_DSN_ESP_MASK_SFT                          (0xff << 0)

/* AUDZCD_DSN_FPI */
#define AUDZCD_DSN_FPI_SFT                               0
#define AUDZCD_DSN_FPI_MASK                              0xff
#define AUDZCD_DSN_FPI_MASK_SFT                          (0xff << 0)

/* AUDZCD_DSN_DXI */
#define AUDZCD_DSN_DXI_SFT                               0
#define AUDZCD_DSN_DXI_MASK                              0xff
#define AUDZCD_DSN_DXI_MASK_SFT                          (0xff << 0)

/* ZCD_CON0 */
#define RG_AUDZCDENABLE_SFT                              0
#define RG_AUDZCDENABLE_MASK                             0x1
#define RG_AUDZCDENABLE_MASK_SFT                         (0x1 << 0)
#define RG_AUDZCDGAINSTEPTIME_SFT                        1
#define RG_AUDZCDGAINSTEPTIME_MASK                       0x7
#define RG_AUDZCDGAINSTEPTIME_MASK_SFT                   (0x7 << 1)
#define RG_AUDZCDGAINSTEPSIZE_SFT                        4
#define RG_AUDZCDGAINSTEPSIZE_MASK                       0x3
#define RG_AUDZCDGAINSTEPSIZE_MASK_SFT                   (0x3 << 4)
#define RG_AUDZCDTIMEOUTMODESEL_SFT                      6
#define RG_AUDZCDTIMEOUTMODESEL_MASK                     0x1
#define RG_AUDZCDTIMEOUTMODESEL_MASK_SFT                 (0x1 << 6)

/* ZCD_CON1 */
#define RG_AUDLOLGAIN_SFT                                0
#define RG_AUDLOLGAIN_MASK                               0x1f
#define RG_AUDLOLGAIN_MASK_SFT                           (0x1f << 0)

/* ZCD_CON2 */
#define RG_AUDLORGAIN_SFT                                0
#define RG_AUDLORGAIN_MASK                               0x1f
#define RG_AUDLORGAIN_MASK_SFT                           (0x1f << 0)

/* ZCD_CON3 */
#define RG_AUDHPLGAIN_SFT                                0
#define RG_AUDHPLGAIN_MASK                               0x1f
#define RG_AUDHPLGAIN_MASK_SFT                           (0x1f << 0)

/* ZCD_CON4 */
#define RG_AUDHPRGAIN_SFT                                0
#define RG_AUDHPRGAIN_MASK                               0x1f
#define RG_AUDHPRGAIN_MASK_SFT                           (0x1f << 0)

/* ZCD_CON5 */
#define RG_AUDHSGAIN_SFT                                 0
#define RG_AUDHSGAIN_MASK                                0x1f
#define RG_AUDHSGAIN_MASK_SFT                            (0x1f << 0)

/* ZCD_CON6 */
#define RG_AUDIVLGAIN_SFT                                0
#define RG_AUDIVLGAIN_MASK                               0x7
#define RG_AUDIVLGAIN_MASK_SFT                           (0x7 << 0)
#define RG_AUDIVRGAIN_SFT                                4
#define RG_AUDIVRGAIN_MASK                               0x7
#define RG_AUDIVRGAIN_MASK_SFT                           (0x7 << 4)

/* ZCD_CON7 */
#define RG_AUDINTGAIN1_SFT                               0
#define RG_AUDINTGAIN1_MASK                              0x3f
#define RG_AUDINTGAIN1_MASK_SFT                          (0x3f << 0)

/* ZCD_CON8 */
#define RG_AUDINTGAIN2_SFT                               0
#define RG_AUDINTGAIN2_MASK                              0x3f
#define RG_AUDINTGAIN2_MASK_SFT                          (0x3f << 0)


#define AUD_TOP_ANA_ID_ADDR                           \
	MT6369_AUD_TOP_ANA_ID
#define RG_ACCDET_CK_PDN_ADDR                         \
	MT6369_AUD_TOP_CKPDN_CON0
#define RG_ACCDET_RST_ADDR                            \
	MT6369_AUD_TOP_RST_CON0
#define RG_INT_EN_ACCDET_ADDR                         \
	MT6369_AUD_TOP_INT_CON0
#define RG_INT_MASK_ACCDET_ADDR                       \
	MT6369_AUD_TOP_INT_MASK_CON0
#define RG_INT_STATUS_ACCDET_ADDR                     \
	MT6369_AUD_TOP_INT_STATUS0
#define RG_NCP_PDDIS_EN_ADDR                          \
	MT6369_AFE_NCP_CFG4
#define RG_AUDPREAMPLON_ADDR                          \
	MT6369_AUDENC_ANA_CON0
#define RG_AUDHPROUTPWRUP_VAUDP15_ADDR                \
	MT6369_AUDDEC_ANA_CON2
#define RG_HPLOUTPUTSTBENH_VAUDP15_ADDR               \
	MT6369_AUDDEC_ANA_CON4
#define RG_HPROUTPUTSTBENH_VAUDP15_ADDR               \
	MT6369_AUDDEC_ANA_CON4
#define RG_CLKSQ_EN_ADDR                              \
	MT6369_AUDENC_ANA_CON12
#define RG_AUDMICBIAS0LOWPEN_ADDR                     \
	MT6369_AUDENC_ANA_CON17
#define RG_AUDPWDBMICBIAS0_ADDR                       \
	MT6369_AUDENC_ANA_CON17
#define RG_AUDMICBIAS1LOWPEN_ADDR                     \
	MT6369_AUDENC_ANA_CON19
#define RG_AUDPWDBMICBIAS1_ADDR                       \
	MT6369_AUDENC_ANA_CON19
#define RG_AUDMICBIAS1VREF_ADDR                       \
	MT6369_AUDENC_ANA_CON19
#define RG_AUDMICBIAS1DCSW1PEN_ADDR                   \
	MT6369_AUDENC_ANA_CON20
#define RG_AUDACCDETMICBIAS0PULLLOW_ADDR              \
	MT6369_AUDENC_ANA_CON21
#define RG_AUDACCDETMICBIAS1PULLLOW_ADDR              \
	MT6369_AUDENC_ANA_CON21
#define RG_ACCDET2AUXSWEN_ADDR                        \
	MT6369_AUDENC_ANA_CON22
#define RG_EINT0NOHYS_ADDR                            \
	MT6369_AUDENC_ANA_CON22
#define RG_EINT0HIRENB_ADDR                           \
	MT6369_AUDENC_ANA_CON22
#define RG_EINT0CONFIGACCDET_ADDR                     \
	MT6369_AUDENC_ANA_CON22
#define RG_EINT1CONFIGACCDET_ADDR                     \
	MT6369_AUDENC_ANA_CON23
#define RG_EINTCOMPVTH_ADDR                           \
	MT6369_AUDENC_ANA_CON23
#define RG_MTEST_EN_ADDR                              \
	MT6369_AUDENC_ANA_CON24
#define RG_MTEST_SEL_ADDR                             \
	MT6369_AUDENC_ANA_CON24
#define RG_ANALOGFDEN_ADDR                            \
	MT6369_AUDENC_ANA_CON24
#define RG_EINT0EN_ADDR                               \
	MT6369_AUDENC_ANA_CON25
#define RG_EINT0CMPEN_ADDR                            \
	MT6369_AUDENC_ANA_CON25
#define RG_EINT1EN_ADDR                               \
	MT6369_AUDENC_ANA_CON26
#define RG_EINT0CTURBO_ADDR                           \
	MT6369_AUDENC_ANA_CON27
#define RG_EINT1CTURBO_ADDR                           \
	MT6369_AUDENC_ANA_CON28
#define RG_ACCDETSPARE_L_ADDR                         \
	MT6369_AUDENC_ANA_CON31
#define RG_ACCDETSPARE_H_ADDR                         \
	MT6369_AUDENC_ANA_CON32

#define AUXADC_RQST_CH5_ADDR                          \
	MT6369_AUXADC_RQST0
#define AUXADC_RQST_CH5_SFT                           5
#define AUXADC_RQST_CH5_MASK                          0x1
#define AUXADC_RQST_CH5_MASK_SFT                      (0x1 << 5)
#define AUXADC_ACCDET_AUTO_SPL_ADDR                   \
	MT6369_AUXADC_ACCDET1
#define AUXADC_ACCDET_AUTO_SPL_SFT                    0
#define AUXADC_ACCDET_AUTO_SPL_MASK                   0x1
#define AUXADC_ACCDET_AUTO_SPL_MASK_SFT               (0x1 << 0)
#define RG_LDO_VUSB_HW0_OP_EN_ADDR                    \
	MT6369_LDO_VUSB_OP_EN2
#define RG_LDO_VUSB_HW0_OP_EN_SFT                     0
#define RG_LDO_VUSB_HW0_OP_EN_MASK                    0x1
#define RG_LDO_VUSB_HW0_OP_EN_MASK_SFT                (0x1 << 0)

#define ACCDET_ANA_ID_ADDR                            \
	MT6369_ACCDET_ANA_ID
#define ACCDET_ANA_ID_SFT                             0
#define ACCDET_ANA_ID_MASK                            0xFF
#define ACCDET_ANA_ID_MASK_SFT                        (0xFF << 0)
#define ACCDET_DIG_ID_ADDR                            \
	MT6369_ACCDET_DIG_ID
#define ACCDET_DIG_ID_SFT                             0
#define ACCDET_DIG_ID_MASK                            0xFF
#define ACCDET_DIG_ID_MASK_SFT                        (0xFF << 0)
#define ACCDET_ANA_MINOR_REV_ADDR                     \
	MT6369_ACCDET_ANA_REV
#define ACCDET_ANA_MINOR_REV_SFT                      0
#define ACCDET_ANA_MINOR_REV_MASK                     0xF
#define ACCDET_ANA_MINOR_REV_MASK_SFT                 (0xF << 0)
#define ACCDET_ANA_MAJOR_REV_ADDR                     \
	MT6369_ACCDET_ANA_REV
#define ACCDET_ANA_MAJOR_REV_SFT                      4
#define ACCDET_ANA_MAJOR_REV_MASK                     0xF
#define ACCDET_ANA_MAJOR_REV_MASK_SFT                 (0xF << 4)
#define ACCDET_DIG_MINOR_REV_ADDR                     \
	MT6369_ACCDET_DIG_REV
#define ACCDET_DIG_MINOR_REV_SFT                      0
#define ACCDET_DIG_MINOR_REV_MASK                     0xF
#define ACCDET_DIG_MINOR_REV_MASK_SFT                 (0xF << 0)
#define ACCDET_DIG_MAJOR_REV_ADDR                     \
	MT6369_ACCDET_DIG_REV
#define ACCDET_DIG_MAJOR_REV_SFT                      4
#define ACCDET_DIG_MAJOR_REV_MASK                     0xF
#define ACCDET_DIG_MAJOR_REV_MASK_SFT                 (0xF << 4)
#define ACCDET_DSN_CBS_ADDR                           \
	MT6369_ACCDET_DSN_DBI
#define ACCDET_DSN_CBS_SFT                            0
#define ACCDET_DSN_CBS_MASK                           0x3
#define ACCDET_DSN_CBS_MASK_SFT                       (0x3 << 0)
#define ACCDET_DSN_BIX_ADDR                           \
	MT6369_ACCDET_DSN_DBI
#define ACCDET_DSN_BIX_SFT                            2
#define ACCDET_DSN_BIX_MASK                           0x3
#define ACCDET_DSN_BIX_MASK_SFT                       (0x3 << 2)
#define ACCDET_DSN_ESP_ADDR                           \
	MT6369_ACCDET_DSN_ESP
#define ACCDET_DSN_ESP_SFT                            0
#define ACCDET_DSN_ESP_MASK                           0xFF
#define ACCDET_DSN_ESP_MASK_SFT                       (0xFF << 0)
#define ACCDET_DSN_FPI_ADDR                           \
	MT6369_ACCDET_DSN_FPI
#define ACCDET_DSN_FPI_SFT                            0
#define ACCDET_DSN_FPI_MASK                           0xFF
#define ACCDET_DSN_FPI_MASK_SFT                       (0xFF << 0)
#define ACCDET_DSN_DXI_ADDR                           \
	MT6369_ACCDET_DSN_DXI
#define ACCDET_DSN_DXI_SFT                            0
#define ACCDET_DSN_DXI_MASK                           0xFF
#define ACCDET_DSN_DXI_MASK_SFT                       (0xFF << 0)
#define ACCDET_AUXADC_SEL_ADDR                        \
	MT6369_ACCDET_CON0
#define ACCDET_AUXADC_SEL_SFT                         0
#define ACCDET_AUXADC_SEL_MASK                        0x1
#define ACCDET_AUXADC_SEL_MASK_SFT                    (0x1 << 0)
#define ACCDET_AUXADC_SW_ADDR                         \
	MT6369_ACCDET_CON0
#define ACCDET_AUXADC_SW_SFT                          1
#define ACCDET_AUXADC_SW_MASK                         0x1
#define ACCDET_AUXADC_SW_MASK_SFT                     (0x1 << 1)
#define ACCDET_TEST_AUXADC_ADDR                       \
	MT6369_ACCDET_CON0
#define ACCDET_TEST_AUXADC_SFT                        2
#define ACCDET_TEST_AUXADC_MASK                       0x1
#define ACCDET_TEST_AUXADC_MASK_SFT                   (0x1 << 2)
#define ACCDET_AUXADC_ANASWCTRL_SEL_ADDR              \
	MT6369_ACCDET_CON1
#define ACCDET_AUXADC_ANASWCTRL_SEL_SFT               0
#define ACCDET_AUXADC_ANASWCTRL_SEL_MASK              0x1
#define ACCDET_AUXADC_ANASWCTRL_SEL_MASK_SFT          (0x1 << 0)
#define AUDACCDETAUXADCSWCTRL_SEL_ADDR                \
	MT6369_ACCDET_CON1
#define AUDACCDETAUXADCSWCTRL_SEL_SFT                 1
#define AUDACCDETAUXADCSWCTRL_SEL_MASK                0x1
#define AUDACCDETAUXADCSWCTRL_SEL_MASK_SFT            (0x1 << 1)
#define AUDACCDETAUXADCSWCTRL_SW_ADDR                 \
	MT6369_ACCDET_CON1
#define AUDACCDETAUXADCSWCTRL_SW_SFT                  2
#define AUDACCDETAUXADCSWCTRL_SW_MASK                 0x1
#define AUDACCDETAUXADCSWCTRL_SW_MASK_SFT             (0x1 << 2)
#define ACCDET_TEST_ANA_ADDR                          \
	MT6369_ACCDET_CON1
#define ACCDET_TEST_ANA_SFT                           3
#define ACCDET_TEST_ANA_MASK                          0x1
#define ACCDET_TEST_ANA_MASK_SFT                      (0x1 << 3)
#define RG_AUDACCDETRSV_ADDR                          \
	MT6369_ACCDET_CON1
#define RG_AUDACCDETRSV_SFT                           4
#define RG_AUDACCDETRSV_MASK                          0x3
#define RG_AUDACCDETRSV_MASK_SFT                      (0x3 << 4)
#define ACCDET_SW_EN_ADDR                             \
	MT6369_ACCDET_CON2
#define ACCDET_SW_EN_SFT                              0
#define ACCDET_SW_EN_MASK                             0x1
#define ACCDET_SW_EN_MASK_SFT                         (0x1 << 0)
#define ACCDET_SEQ_INIT_ADDR                          \
	MT6369_ACCDET_CON2
#define ACCDET_SEQ_INIT_SFT                           1
#define ACCDET_SEQ_INIT_MASK                          0x1
#define ACCDET_SEQ_INIT_MASK_SFT                      (0x1 << 1)
#define ACCDET_EINT0_SW_EN_ADDR                       \
	MT6369_ACCDET_CON2
#define ACCDET_EINT0_SW_EN_SFT                        2
#define ACCDET_EINT0_SW_EN_MASK                       0x1
#define ACCDET_EINT0_SW_EN_MASK_SFT                   (0x1 << 2)
#define ACCDET_EINT0_SEQ_INIT_ADDR                    \
	MT6369_ACCDET_CON2
#define ACCDET_EINT0_SEQ_INIT_SFT                     3
#define ACCDET_EINT0_SEQ_INIT_MASK                    0x1
#define ACCDET_EINT0_SEQ_INIT_MASK_SFT                (0x1 << 3)
#define ACCDET_EINT1_SW_EN_ADDR                       \
	MT6369_ACCDET_CON2
#define ACCDET_EINT1_SW_EN_SFT                        4
#define ACCDET_EINT1_SW_EN_MASK                       0x1
#define ACCDET_EINT1_SW_EN_MASK_SFT                   (0x1 << 4)
#define ACCDET_EINT1_SEQ_INIT_ADDR                    \
	MT6369_ACCDET_CON2
#define ACCDET_EINT1_SEQ_INIT_SFT                     5
#define ACCDET_EINT1_SEQ_INIT_MASK                    0x1
#define ACCDET_EINT1_SEQ_INIT_MASK_SFT                (0x1 << 5)
#define ACCDET_EINT0_INVERTER_SW_EN_ADDR              \
	MT6369_ACCDET_CON2
#define ACCDET_EINT0_INVERTER_SW_EN_SFT               6
#define ACCDET_EINT0_INVERTER_SW_EN_MASK              0x1
#define ACCDET_EINT0_INVERTER_SW_EN_MASK_SFT          (0x1 << 6)
#define ACCDET_EINT0_INVERTER_SEQ_INIT_ADDR           \
	MT6369_ACCDET_CON2
#define ACCDET_EINT0_INVERTER_SEQ_INIT_SFT            7
#define ACCDET_EINT0_INVERTER_SEQ_INIT_MASK           0x1
#define ACCDET_EINT0_INVERTER_SEQ_INIT_MASK_SFT       (0x1 << 7)
#define ACCDET_EINT1_INVERTER_SW_EN_ADDR              \
	MT6369_ACCDET_CON3
#define ACCDET_EINT1_INVERTER_SW_EN_SFT               0
#define ACCDET_EINT1_INVERTER_SW_EN_MASK              0x1
#define ACCDET_EINT1_INVERTER_SW_EN_MASK_SFT          (0x1 << 0)
#define ACCDET_EINT1_INVERTER_SEQ_INIT_ADDR           \
	MT6369_ACCDET_CON3
#define ACCDET_EINT1_INVERTER_SEQ_INIT_SFT            1
#define ACCDET_EINT1_INVERTER_SEQ_INIT_MASK           0x1
#define ACCDET_EINT1_INVERTER_SEQ_INIT_MASK_SFT       (0x1 << 1)
#define ACCDET_EINT0_M_SW_EN_ADDR                     \
	MT6369_ACCDET_CON3
#define ACCDET_EINT0_M_SW_EN_SFT                      2
#define ACCDET_EINT0_M_SW_EN_MASK                     0x1
#define ACCDET_EINT0_M_SW_EN_MASK_SFT                 (0x1 << 2)
#define ACCDET_EINT1_M_SW_EN_ADDR                     \
	MT6369_ACCDET_CON3
#define ACCDET_EINT1_M_SW_EN_SFT                      3
#define ACCDET_EINT1_M_SW_EN_MASK                     0x1
#define ACCDET_EINT1_M_SW_EN_MASK_SFT                 (0x1 << 3)
#define ACCDET_EINT_M_DETECT_EN_ADDR                  \
	MT6369_ACCDET_CON3
#define ACCDET_EINT_M_DETECT_EN_SFT                   4
#define ACCDET_EINT_M_DETECT_EN_MASK                  0x1
#define ACCDET_EINT_M_DETECT_EN_MASK_SFT              (0x1 << 4)
#define ACCDET_CMP_PWM_EN_ADDR                        \
	MT6369_ACCDET_CON4
#define ACCDET_CMP_PWM_EN_SFT                         0
#define ACCDET_CMP_PWM_EN_MASK                        0x1
#define ACCDET_CMP_PWM_EN_MASK_SFT                    (0x1 << 0)
#define ACCDET_VTH_PWM_EN_ADDR                        \
	MT6369_ACCDET_CON4
#define ACCDET_VTH_PWM_EN_SFT                         1
#define ACCDET_VTH_PWM_EN_MASK                        0x1
#define ACCDET_VTH_PWM_EN_MASK_SFT                    (0x1 << 1)
#define ACCDET_MBIAS_PWM_EN_ADDR                      \
	MT6369_ACCDET_CON4
#define ACCDET_MBIAS_PWM_EN_SFT                       2
#define ACCDET_MBIAS_PWM_EN_MASK                      0x1
#define ACCDET_MBIAS_PWM_EN_MASK_SFT                  (0x1 << 2)
#define ACCDET_EINT_EN_PWM_EN_ADDR                    \
	MT6369_ACCDET_CON4
#define ACCDET_EINT_EN_PWM_EN_SFT                     3
#define ACCDET_EINT_EN_PWM_EN_MASK                    0x1
#define ACCDET_EINT_EN_PWM_EN_MASK_SFT                (0x1 << 3)
#define ACCDET_EINT_CMPEN_PWM_EN_ADDR                 \
	MT6369_ACCDET_CON4
#define ACCDET_EINT_CMPEN_PWM_EN_SFT                  4
#define ACCDET_EINT_CMPEN_PWM_EN_MASK                 0x1
#define ACCDET_EINT_CMPEN_PWM_EN_MASK_SFT             (0x1 << 4)
#define ACCDET_EINT_CMPMEN_PWM_EN_ADDR                \
	MT6369_ACCDET_CON4
#define ACCDET_EINT_CMPMEN_PWM_EN_SFT                 5
#define ACCDET_EINT_CMPMEN_PWM_EN_MASK                0x1
#define ACCDET_EINT_CMPMEN_PWM_EN_MASK_SFT            (0x1 << 5)
#define ACCDET_EINT_CTURBO_PWM_EN_ADDR                \
	MT6369_ACCDET_CON4
#define ACCDET_EINT_CTURBO_PWM_EN_SFT                 6
#define ACCDET_EINT_CTURBO_PWM_EN_MASK                0x1
#define ACCDET_EINT_CTURBO_PWM_EN_MASK_SFT            (0x1 << 6)
#define ACCDET_CMP_PWM_IDLE_ADDR                      \
	MT6369_ACCDET_CON5
#define ACCDET_CMP_PWM_IDLE_SFT                       0
#define ACCDET_CMP_PWM_IDLE_MASK                      0x1
#define ACCDET_CMP_PWM_IDLE_MASK_SFT                  (0x1 << 0)
#define ACCDET_VTH_PWM_IDLE_ADDR                      \
	MT6369_ACCDET_CON5
#define ACCDET_VTH_PWM_IDLE_SFT                       1
#define ACCDET_VTH_PWM_IDLE_MASK                      0x1
#define ACCDET_VTH_PWM_IDLE_MASK_SFT                  (0x1 << 1)
#define ACCDET_MBIAS_PWM_IDLE_ADDR                    \
	MT6369_ACCDET_CON5
#define ACCDET_MBIAS_PWM_IDLE_SFT                     2
#define ACCDET_MBIAS_PWM_IDLE_MASK                    0x1
#define ACCDET_MBIAS_PWM_IDLE_MASK_SFT                (0x1 << 2)
#define ACCDET_EINT0_CMPEN_PWM_IDLE_ADDR              \
	MT6369_ACCDET_CON5
#define ACCDET_EINT0_CMPEN_PWM_IDLE_SFT               3
#define ACCDET_EINT0_CMPEN_PWM_IDLE_MASK              0x1
#define ACCDET_EINT0_CMPEN_PWM_IDLE_MASK_SFT          (0x1 << 3)
#define ACCDET_EINT1_CMPEN_PWM_IDLE_ADDR              \
	MT6369_ACCDET_CON5
#define ACCDET_EINT1_CMPEN_PWM_IDLE_SFT               4
#define ACCDET_EINT1_CMPEN_PWM_IDLE_MASK              0x1
#define ACCDET_EINT1_CMPEN_PWM_IDLE_MASK_SFT          (0x1 << 4)
#define ACCDET_PWM_EN_SW_ADDR                         \
	MT6369_ACCDET_CON5
#define ACCDET_PWM_EN_SW_SFT                          5
#define ACCDET_PWM_EN_SW_MASK                         0x1
#define ACCDET_PWM_EN_SW_MASK_SFT                     (0x1 << 5)
#define ACCDET_PWM_EN_SEL_ADDR                        \
	MT6369_ACCDET_CON5
#define ACCDET_PWM_EN_SEL_SFT                         6
#define ACCDET_PWM_EN_SEL_MASK                        0x3
#define ACCDET_PWM_EN_SEL_MASK_SFT                    (0x3 << 6)
#define ACCDET_PWM_WIDTH_L_ADDR                       \
	MT6369_ACCDET_CON6
#define ACCDET_PWM_WIDTH_L_SFT                        0
#define ACCDET_PWM_WIDTH_L_MASK                       0xFF
#define ACCDET_PWM_WIDTH_L_MASK_SFT                   (0xFF << 0)
#define ACCDET_PWM_WIDTH_H_ADDR                       \
	MT6369_ACCDET_CON7
#define ACCDET_PWM_WIDTH_H_SFT                        0
#define ACCDET_PWM_WIDTH_H_MASK                       0xFF
#define ACCDET_PWM_WIDTH_H_MASK_SFT                   (0xFF << 0)
#define ACCDET_PWM_THRESH_L_ADDR                      \
	MT6369_ACCDET_CON8
#define ACCDET_PWM_THRESH_L_SFT                       0
#define ACCDET_PWM_THRESH_L_MASK                      0xFF
#define ACCDET_PWM_THRESH_L_MASK_SFT                  (0xFF << 0)
#define ACCDET_PWM_THRESH_H_ADDR                      \
	MT6369_ACCDET_CON9
#define ACCDET_PWM_THRESH_H_SFT                       0
#define ACCDET_PWM_THRESH_H_MASK                      0xFF
#define ACCDET_PWM_THRESH_H_MASK_SFT                  (0xFF << 0)
#define ACCDET_RISE_DELAY_L_ADDR                      \
	MT6369_ACCDET_CON10
#define ACCDET_RISE_DELAY_L_SFT                       0
#define ACCDET_RISE_DELAY_L_MASK                      0xFF
#define ACCDET_RISE_DELAY_L_MASK_SFT                  (0xFF << 0)
#define ACCDET_RISE_DELAY_H_ADDR                      \
	MT6369_ACCDET_CON11
#define ACCDET_RISE_DELAY_H_SFT                       0
#define ACCDET_RISE_DELAY_H_MASK                      0x7F
#define ACCDET_RISE_DELAY_H_MASK_SFT                  (0x7F << 0)
#define ACCDET_FALL_DELAY_ADDR                        \
	MT6369_ACCDET_CON11
#define ACCDET_FALL_DELAY_SFT                         7
#define ACCDET_FALL_DELAY_MASK                        0x1
#define ACCDET_FALL_DELAY_MASK_SFT                    (0x1 << 7)
#define ACCDET_EINT_CMPMEN_PWM_THRESH_ADDR            \
	MT6369_ACCDET_CON12
#define ACCDET_EINT_CMPMEN_PWM_THRESH_SFT             0
#define ACCDET_EINT_CMPMEN_PWM_THRESH_MASK            0x7
#define ACCDET_EINT_CMPMEN_PWM_THRESH_MASK_SFT        (0x7 << 0)
#define ACCDET_EINT_CMPMEN_PWM_WIDTH_ADDR             \
	MT6369_ACCDET_CON12
#define ACCDET_EINT_CMPMEN_PWM_WIDTH_SFT              4
#define ACCDET_EINT_CMPMEN_PWM_WIDTH_MASK             0x7
#define ACCDET_EINT_CMPMEN_PWM_WIDTH_MASK_SFT         (0x7 << 4)
#define ACCDET_EINT_EN_PWM_THRESH_ADDR                \
	MT6369_ACCDET_CON13
#define ACCDET_EINT_EN_PWM_THRESH_SFT                 0
#define ACCDET_EINT_EN_PWM_THRESH_MASK                0x7
#define ACCDET_EINT_EN_PWM_THRESH_MASK_SFT            (0x7 << 0)
#define ACCDET_EINT_EN_PWM_WIDTH_ADDR                 \
	MT6369_ACCDET_CON13
#define ACCDET_EINT_EN_PWM_WIDTH_SFT                  4
#define ACCDET_EINT_EN_PWM_WIDTH_MASK                 0x3
#define ACCDET_EINT_EN_PWM_WIDTH_MASK_SFT             (0x3 << 4)
#define ACCDET_EINT_CMPEN_PWM_THRESH_ADDR             \
	MT6369_ACCDET_CON14
#define ACCDET_EINT_CMPEN_PWM_THRESH_SFT              0
#define ACCDET_EINT_CMPEN_PWM_THRESH_MASK             0x7
#define ACCDET_EINT_CMPEN_PWM_THRESH_MASK_SFT         (0x7 << 0)
#define ACCDET_EINT_CMPEN_PWM_WIDTH_ADDR              \
	MT6369_ACCDET_CON14
#define ACCDET_EINT_CMPEN_PWM_WIDTH_SFT               4
#define ACCDET_EINT_CMPEN_PWM_WIDTH_MASK              0x3
#define ACCDET_EINT_CMPEN_PWM_WIDTH_MASK_SFT          (0x3 << 4)
#define ACCDET_DEBOUNCE0_L_ADDR                       \
	MT6369_ACCDET_CON15
#define ACCDET_DEBOUNCE0_L_SFT                        0
#define ACCDET_DEBOUNCE0_L_MASK                       0xFF
#define ACCDET_DEBOUNCE0_L_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE0_H_ADDR                       \
	MT6369_ACCDET_CON16
#define ACCDET_DEBOUNCE0_H_SFT                        0
#define ACCDET_DEBOUNCE0_H_MASK                       0xFF
#define ACCDET_DEBOUNCE0_H_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE1_L_ADDR                       \
	MT6369_ACCDET_CON17
#define ACCDET_DEBOUNCE1_L_SFT                        0
#define ACCDET_DEBOUNCE1_L_MASK                       0xFF
#define ACCDET_DEBOUNCE1_L_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE1_H_ADDR                       \
	MT6369_ACCDET_CON18
#define ACCDET_DEBOUNCE1_H_SFT                        0
#define ACCDET_DEBOUNCE1_H_MASK                       0xFF
#define ACCDET_DEBOUNCE1_H_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE2_L_ADDR                       \
	MT6369_ACCDET_CON19
#define ACCDET_DEBOUNCE2_L_SFT                        0
#define ACCDET_DEBOUNCE2_L_MASK                       0xFF
#define ACCDET_DEBOUNCE2_L_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE2_H_ADDR                       \
	MT6369_ACCDET_CON20
#define ACCDET_DEBOUNCE2_H_SFT                        0
#define ACCDET_DEBOUNCE2_H_MASK                       0xFF
#define ACCDET_DEBOUNCE2_H_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE3_L_ADDR                       \
	MT6369_ACCDET_CON21
#define ACCDET_DEBOUNCE3_L_SFT                        0
#define ACCDET_DEBOUNCE3_L_MASK                       0xFF
#define ACCDET_DEBOUNCE3_L_MASK_SFT                   (0xFF << 0)
#define ACCDET_DEBOUNCE3_H_ADDR                       \
	MT6369_ACCDET_CON22
#define ACCDET_DEBOUNCE3_H_SFT                        0
#define ACCDET_DEBOUNCE3_H_MASK                       0xFF
#define ACCDET_DEBOUNCE3_H_MASK_SFT                   (0xFF << 0)
#define ACCDET_CONNECT_AUXADC_TIME_DIG_L_ADDR         \
	MT6369_ACCDET_CON23
#define ACCDET_CONNECT_AUXADC_TIME_DIG_L_SFT          0
#define ACCDET_CONNECT_AUXADC_TIME_DIG_L_MASK         0xFF
#define ACCDET_CONNECT_AUXADC_TIME_DIG_L_MASK_SFT     (0xFF << 0)
#define ACCDET_CONNECT_AUXADC_TIME_DIG_H_ADDR         \
	MT6369_ACCDET_CON24
#define ACCDET_CONNECT_AUXADC_TIME_DIG_H_SFT          0
#define ACCDET_CONNECT_AUXADC_TIME_DIG_H_MASK         0xFF
#define ACCDET_CONNECT_AUXADC_TIME_DIG_H_MASK_SFT     (0xFF << 0)
#define ACCDET_CONNECT_AUXADC_TIME_ANA_L_ADDR         \
	MT6369_ACCDET_CON25
#define ACCDET_CONNECT_AUXADC_TIME_ANA_L_SFT          0
#define ACCDET_CONNECT_AUXADC_TIME_ANA_L_MASK         0xFF
#define ACCDET_CONNECT_AUXADC_TIME_ANA_L_MASK_SFT     (0xFF << 0)
#define ACCDET_CONNECT_AUXADC_TIME_ANA_H_ADDR         \
	MT6369_ACCDET_CON26
#define ACCDET_CONNECT_AUXADC_TIME_ANA_H_SFT          0
#define ACCDET_CONNECT_AUXADC_TIME_ANA_H_MASK         0xFF
#define ACCDET_CONNECT_AUXADC_TIME_ANA_H_MASK_SFT     (0xFF << 0)
#define ACCDET_EINT_DEBOUNCE0_ADDR                    \
	MT6369_ACCDET_CON27
#define ACCDET_EINT_DEBOUNCE0_SFT                     0
#define ACCDET_EINT_DEBOUNCE0_MASK                    0xF
#define ACCDET_EINT_DEBOUNCE0_MASK_SFT                (0xF << 0)
#define ACCDET_EINT_DEBOUNCE1_ADDR                    \
	MT6369_ACCDET_CON27
#define ACCDET_EINT_DEBOUNCE1_SFT                     4
#define ACCDET_EINT_DEBOUNCE1_MASK                    0xF
#define ACCDET_EINT_DEBOUNCE1_MASK_SFT                (0xF << 4)
#define ACCDET_EINT_DEBOUNCE2_ADDR                    \
	MT6369_ACCDET_CON28
#define ACCDET_EINT_DEBOUNCE2_SFT                     0
#define ACCDET_EINT_DEBOUNCE2_MASK                    0xF
#define ACCDET_EINT_DEBOUNCE2_MASK_SFT                (0xF << 0)
#define ACCDET_EINT_DEBOUNCE3_ADDR                    \
	MT6369_ACCDET_CON28
#define ACCDET_EINT_DEBOUNCE3_SFT                     4
#define ACCDET_EINT_DEBOUNCE3_MASK                    0xF
#define ACCDET_EINT_DEBOUNCE3_MASK_SFT                (0xF << 4)
#define ACCDET_EINT_INVERTER_DEBOUNCE_ADDR            \
	MT6369_ACCDET_CON29
#define ACCDET_EINT_INVERTER_DEBOUNCE_SFT             0
#define ACCDET_EINT_INVERTER_DEBOUNCE_MASK            0xF
#define ACCDET_EINT_INVERTER_DEBOUNCE_MASK_SFT        (0xF << 0)
#define ACCDET_IVAL_CUR_IN_ADDR                       \
	MT6369_ACCDET_CON30
#define ACCDET_IVAL_CUR_IN_SFT                        0
#define ACCDET_IVAL_CUR_IN_MASK                       0x3
#define ACCDET_IVAL_CUR_IN_MASK_SFT                   (0x3 << 0)
#define ACCDET_IVAL_SAM_IN_ADDR                       \
	MT6369_ACCDET_CON30
#define ACCDET_IVAL_SAM_IN_SFT                        2
#define ACCDET_IVAL_SAM_IN_MASK                       0x3
#define ACCDET_IVAL_SAM_IN_MASK_SFT                   (0x3 << 2)
#define ACCDET_IVAL_MEM_IN_ADDR                       \
	MT6369_ACCDET_CON30
#define ACCDET_IVAL_MEM_IN_SFT                        4
#define ACCDET_IVAL_MEM_IN_MASK                       0x3
#define ACCDET_IVAL_MEM_IN_MASK_SFT                   (0x3 << 4)
#define ACCDET_EINT_IVAL_CUR_IN_ADDR                  \
	MT6369_ACCDET_CON30
#define ACCDET_EINT_IVAL_CUR_IN_SFT                   6
#define ACCDET_EINT_IVAL_CUR_IN_MASK                  0x3
#define ACCDET_EINT_IVAL_CUR_IN_MASK_SFT              (0x3 << 6)
#define ACCDET_EINT_IVAL_SAM_IN_ADDR                  \
	MT6369_ACCDET_CON31
#define ACCDET_EINT_IVAL_SAM_IN_SFT                   0
#define ACCDET_EINT_IVAL_SAM_IN_MASK                  0x3
#define ACCDET_EINT_IVAL_SAM_IN_MASK_SFT              (0x3 << 0)
#define ACCDET_EINT_IVAL_MEM_IN_ADDR                  \
	MT6369_ACCDET_CON31
#define ACCDET_EINT_IVAL_MEM_IN_SFT                   2
#define ACCDET_EINT_IVAL_MEM_IN_MASK                  0x3
#define ACCDET_EINT_IVAL_MEM_IN_MASK_SFT              (0x3 << 2)
#define ACCDET_IVAL_SEL_ADDR                          \
	MT6369_ACCDET_CON31
#define ACCDET_IVAL_SEL_SFT                           4
#define ACCDET_IVAL_SEL_MASK                          0x1
#define ACCDET_IVAL_SEL_MASK_SFT                      (0x1 << 4)
#define ACCDET_EINT_IVAL_SEL_ADDR                     \
	MT6369_ACCDET_CON31
#define ACCDET_EINT_IVAL_SEL_SFT                      5
#define ACCDET_EINT_IVAL_SEL_MASK                     0x1
#define ACCDET_EINT_IVAL_SEL_MASK_SFT                 (0x1 << 5)
#define ACCDET_EINT_INVERTER_IVAL_CUR_IN_ADDR         \
	MT6369_ACCDET_CON32
#define ACCDET_EINT_INVERTER_IVAL_CUR_IN_SFT          0
#define ACCDET_EINT_INVERTER_IVAL_CUR_IN_MASK         0x1
#define ACCDET_EINT_INVERTER_IVAL_CUR_IN_MASK_SFT     (0x1 << 0)
#define ACCDET_EINT_INVERTER_IVAL_SAM_IN_ADDR         \
	MT6369_ACCDET_CON32
#define ACCDET_EINT_INVERTER_IVAL_SAM_IN_SFT          1
#define ACCDET_EINT_INVERTER_IVAL_SAM_IN_MASK         0x1
#define ACCDET_EINT_INVERTER_IVAL_SAM_IN_MASK_SFT     (0x1 << 1)
#define ACCDET_EINT_INVERTER_IVAL_MEM_IN_ADDR         \
	MT6369_ACCDET_CON32
#define ACCDET_EINT_INVERTER_IVAL_MEM_IN_SFT          2
#define ACCDET_EINT_INVERTER_IVAL_MEM_IN_MASK         0x1
#define ACCDET_EINT_INVERTER_IVAL_MEM_IN_MASK_SFT     (0x1 << 2)
#define ACCDET_EINT_INVERTER_IVAL_SEL_ADDR            \
	MT6369_ACCDET_CON32
#define ACCDET_EINT_INVERTER_IVAL_SEL_SFT             3
#define ACCDET_EINT_INVERTER_IVAL_SEL_MASK            0x1
#define ACCDET_EINT_INVERTER_IVAL_SEL_MASK_SFT        (0x1 << 3)
#define ACCDET_IRQ_ADDR                               \
	MT6369_ACCDET_CON33
#define ACCDET_IRQ_SFT                                0
#define ACCDET_IRQ_MASK                               0x1
#define ACCDET_IRQ_MASK_SFT                           (0x1 << 0)
#define ACCDET_EINT0_IRQ_ADDR                         \
	MT6369_ACCDET_CON33
#define ACCDET_EINT0_IRQ_SFT                          2
#define ACCDET_EINT0_IRQ_MASK                         0x1
#define ACCDET_EINT0_IRQ_MASK_SFT                     (0x1 << 2)
#define ACCDET_EINT1_IRQ_ADDR                         \
	MT6369_ACCDET_CON33
#define ACCDET_EINT1_IRQ_SFT                          3
#define ACCDET_EINT1_IRQ_MASK                         0x1
#define ACCDET_EINT1_IRQ_MASK_SFT                     (0x1 << 3)
#define ACCDET_EINT_IN_INVERSE_ADDR                   \
	MT6369_ACCDET_CON33
#define ACCDET_EINT_IN_INVERSE_SFT                    4
#define ACCDET_EINT_IN_INVERSE_MASK                   0x1
#define ACCDET_EINT_IN_INVERSE_MASK_SFT               (0x1 << 4)
#define ACCDET_IRQ_CLR_ADDR                           \
	MT6369_ACCDET_CON34
#define ACCDET_IRQ_CLR_SFT                            0
#define ACCDET_IRQ_CLR_MASK                           0x1
#define ACCDET_IRQ_CLR_MASK_SFT                       (0x1 << 0)
#define ACCDET_EINT0_IRQ_CLR_ADDR                     \
	MT6369_ACCDET_CON34
#define ACCDET_EINT0_IRQ_CLR_SFT                      2
#define ACCDET_EINT0_IRQ_CLR_MASK                     0x1
#define ACCDET_EINT0_IRQ_CLR_MASK_SFT                 (0x1 << 2)
#define ACCDET_EINT1_IRQ_CLR_ADDR                     \
	MT6369_ACCDET_CON34
#define ACCDET_EINT1_IRQ_CLR_SFT                      3
#define ACCDET_EINT1_IRQ_CLR_MASK                     0x1
#define ACCDET_EINT1_IRQ_CLR_MASK_SFT                 (0x1 << 3)
#define ACCDET_EINT_M_PLUG_IN_NUM_ADDR                \
	MT6369_ACCDET_CON34
#define ACCDET_EINT_M_PLUG_IN_NUM_SFT                 4
#define ACCDET_EINT_M_PLUG_IN_NUM_MASK                0x7
#define ACCDET_EINT_M_PLUG_IN_NUM_MASK_SFT            (0x7 << 4)
#define ACCDET_DA_STABLE_ADDR                         \
	MT6369_ACCDET_CON35
#define ACCDET_DA_STABLE_SFT                          0
#define ACCDET_DA_STABLE_MASK                         0x1
#define ACCDET_DA_STABLE_MASK_SFT                     (0x1 << 0)
#define ACCDET_EINT0_EN_STABLE_ADDR                   \
	MT6369_ACCDET_CON35
#define ACCDET_EINT0_EN_STABLE_SFT                    1
#define ACCDET_EINT0_EN_STABLE_MASK                   0x1
#define ACCDET_EINT0_EN_STABLE_MASK_SFT               (0x1 << 1)
#define ACCDET_EINT0_CMPEN_STABLE_ADDR                \
	MT6369_ACCDET_CON35
#define ACCDET_EINT0_CMPEN_STABLE_SFT                 2
#define ACCDET_EINT0_CMPEN_STABLE_MASK                0x1
#define ACCDET_EINT0_CMPEN_STABLE_MASK_SFT            (0x1 << 2)
#define ACCDET_EINT0_CMPMEN_STABLE_ADDR               \
	MT6369_ACCDET_CON35
#define ACCDET_EINT0_CMPMEN_STABLE_SFT                3
#define ACCDET_EINT0_CMPMEN_STABLE_MASK               0x1
#define ACCDET_EINT0_CMPMEN_STABLE_MASK_SFT           (0x1 << 3)
#define ACCDET_EINT0_CTURBO_STABLE_ADDR               \
	MT6369_ACCDET_CON35
#define ACCDET_EINT0_CTURBO_STABLE_SFT                4
#define ACCDET_EINT0_CTURBO_STABLE_MASK               0x1
#define ACCDET_EINT0_CTURBO_STABLE_MASK_SFT           (0x1 << 4)
#define ACCDET_EINT0_CEN_STABLE_ADDR                  \
	MT6369_ACCDET_CON35
#define ACCDET_EINT0_CEN_STABLE_SFT                   5
#define ACCDET_EINT0_CEN_STABLE_MASK                  0x1
#define ACCDET_EINT0_CEN_STABLE_MASK_SFT              (0x1 << 5)
#define ACCDET_EINT1_EN_STABLE_ADDR                   \
	MT6369_ACCDET_CON35
#define ACCDET_EINT1_EN_STABLE_SFT                    6
#define ACCDET_EINT1_EN_STABLE_MASK                   0x1
#define ACCDET_EINT1_EN_STABLE_MASK_SFT               (0x1 << 6)
#define ACCDET_EINT1_CMPEN_STABLE_ADDR                \
	MT6369_ACCDET_CON35
#define ACCDET_EINT1_CMPEN_STABLE_SFT                 7
#define ACCDET_EINT1_CMPEN_STABLE_MASK                0x1
#define ACCDET_EINT1_CMPEN_STABLE_MASK_SFT            (0x1 << 7)
#define ACCDET_EINT1_CMPMEN_STABLE_ADDR               \
	MT6369_ACCDET_CON36
#define ACCDET_EINT1_CMPMEN_STABLE_SFT                0
#define ACCDET_EINT1_CMPMEN_STABLE_MASK               0x1
#define ACCDET_EINT1_CMPMEN_STABLE_MASK_SFT           (0x1 << 0)
#define ACCDET_EINT1_CTURBO_STABLE_ADDR               \
	MT6369_ACCDET_CON36
#define ACCDET_EINT1_CTURBO_STABLE_SFT                1
#define ACCDET_EINT1_CTURBO_STABLE_MASK               0x1
#define ACCDET_EINT1_CTURBO_STABLE_MASK_SFT           (0x1 << 1)
#define ACCDET_EINT1_CEN_STABLE_ADDR                  \
	MT6369_ACCDET_CON36
#define ACCDET_EINT1_CEN_STABLE_SFT                   2
#define ACCDET_EINT1_CEN_STABLE_MASK                  0x1
#define ACCDET_EINT1_CEN_STABLE_MASK_SFT              (0x1 << 2)
#define ACCDET_HWMODE_EN_ADDR                         \
	MT6369_ACCDET_CON37
#define ACCDET_HWMODE_EN_SFT                          0
#define ACCDET_HWMODE_EN_MASK                         0x1
#define ACCDET_HWMODE_EN_MASK_SFT                     (0x1 << 0)
#define ACCDET_HWMODE_SEL_ADDR                        \
	MT6369_ACCDET_CON37
#define ACCDET_HWMODE_SEL_SFT                         1
#define ACCDET_HWMODE_SEL_MASK                        0x3
#define ACCDET_HWMODE_SEL_MASK_SFT                    (0x3 << 1)
#define ACCDET_PLUG_OUT_DETECT_ADDR                   \
	MT6369_ACCDET_CON37
#define ACCDET_PLUG_OUT_DETECT_SFT                    3
#define ACCDET_PLUG_OUT_DETECT_MASK                   0x1
#define ACCDET_PLUG_OUT_DETECT_MASK_SFT               (0x1 << 3)
#define ACCDET_EINT0_REVERSE_ADDR                     \
	MT6369_ACCDET_CON37
#define ACCDET_EINT0_REVERSE_SFT                      4
#define ACCDET_EINT0_REVERSE_MASK                     0x1
#define ACCDET_EINT0_REVERSE_MASK_SFT                 (0x1 << 4)
#define ACCDET_EINT1_REVERSE_ADDR                     \
	MT6369_ACCDET_CON37
#define ACCDET_EINT1_REVERSE_SFT                      5
#define ACCDET_EINT1_REVERSE_MASK                     0x1
#define ACCDET_EINT1_REVERSE_MASK_SFT                 (0x1 << 5)
#define ACCDET_EINT_HWMODE_EN_ADDR                    \
	MT6369_ACCDET_CON38
#define ACCDET_EINT_HWMODE_EN_SFT                     0
#define ACCDET_EINT_HWMODE_EN_MASK                    0x1
#define ACCDET_EINT_HWMODE_EN_MASK_SFT                (0x1 << 0)
#define ACCDET_EINT_PLUG_OUT_BYPASS_DEB_ADDR          \
	MT6369_ACCDET_CON38
#define ACCDET_EINT_PLUG_OUT_BYPASS_DEB_SFT           1
#define ACCDET_EINT_PLUG_OUT_BYPASS_DEB_MASK          0x1
#define ACCDET_EINT_PLUG_OUT_BYPASS_DEB_MASK_SFT      (0x1 << 1)
#define ACCDET_EINT_M_PLUG_IN_EN_ADDR                 \
	MT6369_ACCDET_CON38
#define ACCDET_EINT_M_PLUG_IN_EN_SFT                  2
#define ACCDET_EINT_M_PLUG_IN_EN_MASK                 0x1
#define ACCDET_EINT_M_PLUG_IN_EN_MASK_SFT             (0x1 << 2)
#define ACCDET_EINT_M_HWMODE_EN_ADDR                  \
	MT6369_ACCDET_CON38
#define ACCDET_EINT_M_HWMODE_EN_SFT                   3
#define ACCDET_EINT_M_HWMODE_EN_MASK                  0x1
#define ACCDET_EINT_M_HWMODE_EN_MASK_SFT              (0x1 << 3)
#define ACCDET_TEST_CMPEN_ADDR                        \
	MT6369_ACCDET_CON39
#define ACCDET_TEST_CMPEN_SFT                         0
#define ACCDET_TEST_CMPEN_MASK                        0x1
#define ACCDET_TEST_CMPEN_MASK_SFT                    (0x1 << 0)
#define ACCDET_TEST_VTHEN_ADDR                        \
	MT6369_ACCDET_CON39
#define ACCDET_TEST_VTHEN_SFT                         1
#define ACCDET_TEST_VTHEN_MASK                        0x1
#define ACCDET_TEST_VTHEN_MASK_SFT                    (0x1 << 1)
#define ACCDET_TEST_MBIASEN_ADDR                      \
	MT6369_ACCDET_CON39
#define ACCDET_TEST_MBIASEN_SFT                       2
#define ACCDET_TEST_MBIASEN_MASK                      0x1
#define ACCDET_TEST_MBIASEN_MASK_SFT                  (0x1 << 2)
#define ACCDET_EINT_TEST_EN_ADDR                      \
	MT6369_ACCDET_CON39
#define ACCDET_EINT_TEST_EN_SFT                       3
#define ACCDET_EINT_TEST_EN_MASK                      0x1
#define ACCDET_EINT_TEST_EN_MASK_SFT                  (0x1 << 3)
#define ACCDET_EINT_TEST_INVEN_ADDR                   \
	MT6369_ACCDET_CON39
#define ACCDET_EINT_TEST_INVEN_SFT                    4
#define ACCDET_EINT_TEST_INVEN_MASK                   0x1
#define ACCDET_EINT_TEST_INVEN_MASK_SFT               (0x1 << 4)
#define ACCDET_EINT_TEST_CMPEN_ADDR                   \
	MT6369_ACCDET_CON39
#define ACCDET_EINT_TEST_CMPEN_SFT                    5
#define ACCDET_EINT_TEST_CMPEN_MASK                   0x1
#define ACCDET_EINT_TEST_CMPEN_MASK_SFT               (0x1 << 5)
#define ACCDET_EINT_TEST_CMPMEN_ADDR                  \
	MT6369_ACCDET_CON39
#define ACCDET_EINT_TEST_CMPMEN_SFT                   6
#define ACCDET_EINT_TEST_CMPMEN_MASK                  0x1
#define ACCDET_EINT_TEST_CMPMEN_MASK_SFT              (0x1 << 6)
#define ACCDET_EINT_TEST_CTURBO_ADDR                  \
	MT6369_ACCDET_CON39
#define ACCDET_EINT_TEST_CTURBO_SFT                   7
#define ACCDET_EINT_TEST_CTURBO_MASK                  0x1
#define ACCDET_EINT_TEST_CTURBO_MASK_SFT              (0x1 << 7)
#define ACCDET_EINT_TEST_CEN_ADDR                     \
	MT6369_ACCDET_CON40
#define ACCDET_EINT_TEST_CEN_SFT                      0
#define ACCDET_EINT_TEST_CEN_MASK                     0x1
#define ACCDET_EINT_TEST_CEN_MASK_SFT                 (0x1 << 0)
#define ACCDET_TEST_B_ADDR                            \
	MT6369_ACCDET_CON40
#define ACCDET_TEST_B_SFT                             1
#define ACCDET_TEST_B_MASK                            0x1
#define ACCDET_TEST_B_MASK_SFT                        (0x1 << 1)
#define ACCDET_TEST_A_ADDR                            \
	MT6369_ACCDET_CON40
#define ACCDET_TEST_A_SFT                             2
#define ACCDET_TEST_A_MASK                            0x1
#define ACCDET_TEST_A_MASK_SFT                        (0x1 << 2)
#define ACCDET_EINT_TEST_CMPOUT_ADDR                  \
	MT6369_ACCDET_CON40
#define ACCDET_EINT_TEST_CMPOUT_SFT                   3
#define ACCDET_EINT_TEST_CMPOUT_MASK                  0x1
#define ACCDET_EINT_TEST_CMPOUT_MASK_SFT              (0x1 << 3)
#define ACCDET_EINT_TEST_CMPMOUT_ADDR                 \
	MT6369_ACCDET_CON40
#define ACCDET_EINT_TEST_CMPMOUT_SFT                  4
#define ACCDET_EINT_TEST_CMPMOUT_MASK                 0x1
#define ACCDET_EINT_TEST_CMPMOUT_MASK_SFT             (0x1 << 4)
#define ACCDET_EINT_TEST_INVOUT_ADDR                  \
	MT6369_ACCDET_CON40
#define ACCDET_EINT_TEST_INVOUT_SFT                   5
#define ACCDET_EINT_TEST_INVOUT_MASK                  0x1
#define ACCDET_EINT_TEST_INVOUT_MASK_SFT              (0x1 << 5)
#define ACCDET_CMPEN_SEL_ADDR                         \
	MT6369_ACCDET_CON41
#define ACCDET_CMPEN_SEL_SFT                          0
#define ACCDET_CMPEN_SEL_MASK                         0x1
#define ACCDET_CMPEN_SEL_MASK_SFT                     (0x1 << 0)
#define ACCDET_VTHEN_SEL_ADDR                         \
	MT6369_ACCDET_CON41
#define ACCDET_VTHEN_SEL_SFT                          1
#define ACCDET_VTHEN_SEL_MASK                         0x1
#define ACCDET_VTHEN_SEL_MASK_SFT                     (0x1 << 1)
#define ACCDET_MBIASEN_SEL_ADDR                       \
	MT6369_ACCDET_CON41
#define ACCDET_MBIASEN_SEL_SFT                        2
#define ACCDET_MBIASEN_SEL_MASK                       0x1
#define ACCDET_MBIASEN_SEL_MASK_SFT                   (0x1 << 2)
#define ACCDET_EINT_EN_SEL_ADDR                       \
	MT6369_ACCDET_CON41
#define ACCDET_EINT_EN_SEL_SFT                        3
#define ACCDET_EINT_EN_SEL_MASK                       0x1
#define ACCDET_EINT_EN_SEL_MASK_SFT                   (0x1 << 3)
#define ACCDET_EINT_INVEN_SEL_ADDR                    \
	MT6369_ACCDET_CON41
#define ACCDET_EINT_INVEN_SEL_SFT                     4
#define ACCDET_EINT_INVEN_SEL_MASK                    0x1
#define ACCDET_EINT_INVEN_SEL_MASK_SFT                (0x1 << 4)
#define ACCDET_EINT_CMPEN_SEL_ADDR                    \
	MT6369_ACCDET_CON41
#define ACCDET_EINT_CMPEN_SEL_SFT                     5
#define ACCDET_EINT_CMPEN_SEL_MASK                    0x1
#define ACCDET_EINT_CMPEN_SEL_MASK_SFT                (0x1 << 5)
#define ACCDET_EINT_CMPMEN_SEL_ADDR                   \
	MT6369_ACCDET_CON41
#define ACCDET_EINT_CMPMEN_SEL_SFT                    6
#define ACCDET_EINT_CMPMEN_SEL_MASK                   0x1
#define ACCDET_EINT_CMPMEN_SEL_MASK_SFT               (0x1 << 6)
#define ACCDET_EINT_CTURBO_SEL_ADDR                   \
	MT6369_ACCDET_CON41
#define ACCDET_EINT_CTURBO_SEL_SFT                    7
#define ACCDET_EINT_CTURBO_SEL_MASK                   0x1
#define ACCDET_EINT_CTURBO_SEL_MASK_SFT               (0x1 << 7)
#define ACCDET_B_SEL_ADDR                             \
	MT6369_ACCDET_CON42
#define ACCDET_B_SEL_SFT                              1
#define ACCDET_B_SEL_MASK                             0x1
#define ACCDET_B_SEL_MASK_SFT                         (0x1 << 1)
#define ACCDET_A_SEL_ADDR                             \
	MT6369_ACCDET_CON42
#define ACCDET_A_SEL_SFT                              2
#define ACCDET_A_SEL_MASK                             0x1
#define ACCDET_A_SEL_MASK_SFT                         (0x1 << 2)
#define ACCDET_EINT_CMPOUT_SEL_ADDR                   \
	MT6369_ACCDET_CON42
#define ACCDET_EINT_CMPOUT_SEL_SFT                    3
#define ACCDET_EINT_CMPOUT_SEL_MASK                   0x1
#define ACCDET_EINT_CMPOUT_SEL_MASK_SFT               (0x1 << 3)
#define ACCDET_EINT_CMPMOUT_SEL_ADDR                  \
	MT6369_ACCDET_CON42
#define ACCDET_EINT_CMPMOUT_SEL_SFT                   4
#define ACCDET_EINT_CMPMOUT_SEL_MASK                  0x1
#define ACCDET_EINT_CMPMOUT_SEL_MASK_SFT              (0x1 << 4)
#define ACCDET_EINT_INVOUT_SEL_ADDR                   \
	MT6369_ACCDET_CON42
#define ACCDET_EINT_INVOUT_SEL_SFT                    5
#define ACCDET_EINT_INVOUT_SEL_MASK                   0x1
#define ACCDET_EINT_INVOUT_SEL_MASK_SFT               (0x1 << 5)
#define ACCDET_CMPEN_SW_ADDR                          \
	MT6369_ACCDET_CON43
#define ACCDET_CMPEN_SW_SFT                           0
#define ACCDET_CMPEN_SW_MASK                          0x1
#define ACCDET_CMPEN_SW_MASK_SFT                      (0x1 << 0)
#define ACCDET_VTHEN_SW_ADDR                          \
	MT6369_ACCDET_CON43
#define ACCDET_VTHEN_SW_SFT                           1
#define ACCDET_VTHEN_SW_MASK                          0x1
#define ACCDET_VTHEN_SW_MASK_SFT                      (0x1 << 1)
#define ACCDET_MBIASEN_SW_ADDR                        \
	MT6369_ACCDET_CON43
#define ACCDET_MBIASEN_SW_SFT                         2
#define ACCDET_MBIASEN_SW_MASK                        0x1
#define ACCDET_MBIASEN_SW_MASK_SFT                    (0x1 << 2)
#define ACCDET_EINT0_EN_SW_ADDR                       \
	MT6369_ACCDET_CON43
#define ACCDET_EINT0_EN_SW_SFT                        3
#define ACCDET_EINT0_EN_SW_MASK                       0x1
#define ACCDET_EINT0_EN_SW_MASK_SFT                   (0x1 << 3)
#define ACCDET_EINT0_INVEN_SW_ADDR                    \
	MT6369_ACCDET_CON43
#define ACCDET_EINT0_INVEN_SW_SFT                     4
#define ACCDET_EINT0_INVEN_SW_MASK                    0x1
#define ACCDET_EINT0_INVEN_SW_MASK_SFT                (0x1 << 4)
#define ACCDET_EINT0_CMPEN_SW_ADDR                    \
	MT6369_ACCDET_CON43
#define ACCDET_EINT0_CMPEN_SW_SFT                     5
#define ACCDET_EINT0_CMPEN_SW_MASK                    0x1
#define ACCDET_EINT0_CMPEN_SW_MASK_SFT                (0x1 << 5)
#define ACCDET_EINT0_CMPMEN_SW_ADDR                   \
	MT6369_ACCDET_CON43
#define ACCDET_EINT0_CMPMEN_SW_SFT                    6
#define ACCDET_EINT0_CMPMEN_SW_MASK                   0x1
#define ACCDET_EINT0_CMPMEN_SW_MASK_SFT               (0x1 << 6)
#define ACCDET_EINT0_CTURBO_SW_ADDR                   \
	MT6369_ACCDET_CON43
#define ACCDET_EINT0_CTURBO_SW_SFT                    7
#define ACCDET_EINT0_CTURBO_SW_MASK                   0x1
#define ACCDET_EINT0_CTURBO_SW_MASK_SFT               (0x1 << 7)
#define ACCDET_EINT1_EN_SW_ADDR                       \
	MT6369_ACCDET_CON44
#define ACCDET_EINT1_EN_SW_SFT                        0
#define ACCDET_EINT1_EN_SW_MASK                       0x1
#define ACCDET_EINT1_EN_SW_MASK_SFT                   (0x1 << 0)
#define ACCDET_EINT1_INVEN_SW_ADDR                    \
	MT6369_ACCDET_CON44
#define ACCDET_EINT1_INVEN_SW_SFT                     1
#define ACCDET_EINT1_INVEN_SW_MASK                    0x1
#define ACCDET_EINT1_INVEN_SW_MASK_SFT                (0x1 << 1)
#define ACCDET_EINT1_CMPEN_SW_ADDR                    \
	MT6369_ACCDET_CON44
#define ACCDET_EINT1_CMPEN_SW_SFT                     2
#define ACCDET_EINT1_CMPEN_SW_MASK                    0x1
#define ACCDET_EINT1_CMPEN_SW_MASK_SFT                (0x1 << 2)
#define ACCDET_EINT1_CMPMEN_SW_ADDR                   \
	MT6369_ACCDET_CON44
#define ACCDET_EINT1_CMPMEN_SW_SFT                    3
#define ACCDET_EINT1_CMPMEN_SW_MASK                   0x1
#define ACCDET_EINT1_CMPMEN_SW_MASK_SFT               (0x1 << 3)
#define ACCDET_EINT1_CTURBO_SW_ADDR                   \
	MT6369_ACCDET_CON44
#define ACCDET_EINT1_CTURBO_SW_SFT                    4
#define ACCDET_EINT1_CTURBO_SW_MASK                   0x1
#define ACCDET_EINT1_CTURBO_SW_MASK_SFT               (0x1 << 4)
#define ACCDET_B_SW_ADDR                              \
	MT6369_ACCDET_CON45
#define ACCDET_B_SW_SFT                               0
#define ACCDET_B_SW_MASK                              0x1
#define ACCDET_B_SW_MASK_SFT                          (0x1 << 0)
#define ACCDET_A_SW_ADDR                              \
	MT6369_ACCDET_CON45
#define ACCDET_A_SW_SFT                               1
#define ACCDET_A_SW_MASK                              0x1
#define ACCDET_A_SW_MASK_SFT                          (0x1 << 1)
#define ACCDET_EINT0_CMPOUT_SW_ADDR                   \
	MT6369_ACCDET_CON45
#define ACCDET_EINT0_CMPOUT_SW_SFT                    2
#define ACCDET_EINT0_CMPOUT_SW_MASK                   0x1
#define ACCDET_EINT0_CMPOUT_SW_MASK_SFT               (0x1 << 2)
#define ACCDET_EINT0_CMPMOUT_SW_ADDR                  \
	MT6369_ACCDET_CON45
#define ACCDET_EINT0_CMPMOUT_SW_SFT                   3
#define ACCDET_EINT0_CMPMOUT_SW_MASK                  0x1
#define ACCDET_EINT0_CMPMOUT_SW_MASK_SFT              (0x1 << 3)
#define ACCDET_EINT0_INVOUT_SW_ADDR                   \
	MT6369_ACCDET_CON45
#define ACCDET_EINT0_INVOUT_SW_SFT                    4
#define ACCDET_EINT0_INVOUT_SW_MASK                   0x1
#define ACCDET_EINT0_INVOUT_SW_MASK_SFT               (0x1 << 4)
#define ACCDET_EINT1_CMPOUT_SW_ADDR                   \
	MT6369_ACCDET_CON45
#define ACCDET_EINT1_CMPOUT_SW_SFT                    5
#define ACCDET_EINT1_CMPOUT_SW_MASK                   0x1
#define ACCDET_EINT1_CMPOUT_SW_MASK_SFT               (0x1 << 5)
#define ACCDET_EINT1_CMPMOUT_SW_ADDR                  \
	MT6369_ACCDET_CON45
#define ACCDET_EINT1_CMPMOUT_SW_SFT                   6
#define ACCDET_EINT1_CMPMOUT_SW_MASK                  0x1
#define ACCDET_EINT1_CMPMOUT_SW_MASK_SFT              (0x1 << 6)
#define ACCDET_EINT1_INVOUT_SW_ADDR                   \
	MT6369_ACCDET_CON45
#define ACCDET_EINT1_INVOUT_SW_SFT                    7
#define ACCDET_EINT1_INVOUT_SW_MASK                   0x1
#define ACCDET_EINT1_INVOUT_SW_MASK_SFT               (0x1 << 7)
#define AD_AUDACCDETCMPOB_ADDR                        \
	MT6369_ACCDET_CON46
#define AD_AUDACCDETCMPOB_SFT                         0
#define AD_AUDACCDETCMPOB_MASK                        0x1
#define AD_AUDACCDETCMPOB_MASK_SFT                    (0x1 << 0)
#define AD_AUDACCDETCMPOA_ADDR                        \
	MT6369_ACCDET_CON46
#define AD_AUDACCDETCMPOA_SFT                         1
#define AD_AUDACCDETCMPOA_MASK                        0x1
#define AD_AUDACCDETCMPOA_MASK_SFT                    (0x1 << 1)
#define ACCDET_CUR_IN_ADDR                            \
	MT6369_ACCDET_CON46
#define ACCDET_CUR_IN_SFT                             2
#define ACCDET_CUR_IN_MASK                            0x3
#define ACCDET_CUR_IN_MASK_SFT                        (0x3 << 2)
#define ACCDET_SAM_IN_ADDR                            \
	MT6369_ACCDET_CON46
#define ACCDET_SAM_IN_SFT                             4
#define ACCDET_SAM_IN_MASK                            0x3
#define ACCDET_SAM_IN_MASK_SFT                        (0x3 << 4)
#define ACCDET_MEM_IN_ADDR                            \
	MT6369_ACCDET_CON46
#define ACCDET_MEM_IN_SFT                             6
#define ACCDET_MEM_IN_MASK                            0x3
#define ACCDET_MEM_IN_MASK_SFT                        (0x3 << 6)
#define ACCDET_STATE_ADDR                             \
	MT6369_ACCDET_CON47
#define ACCDET_STATE_SFT                              0
#define ACCDET_STATE_MASK                             0x7
#define ACCDET_STATE_MASK_SFT                         (0x7 << 0)
#define DA_AUDACCDETMBIASCLK_ADDR                     \
	MT6369_ACCDET_CON47
#define DA_AUDACCDETMBIASCLK_SFT                      4
#define DA_AUDACCDETMBIASCLK_MASK                     0x1
#define DA_AUDACCDETMBIASCLK_MASK_SFT                 (0x1 << 4)
#define DA_AUDACCDETVTHCLK_ADDR                       \
	MT6369_ACCDET_CON47
#define DA_AUDACCDETVTHCLK_SFT                        5
#define DA_AUDACCDETVTHCLK_MASK                       0x1
#define DA_AUDACCDETVTHCLK_MASK_SFT                   (0x1 << 5)
#define DA_AUDACCDETCMPCLK_ADDR                       \
	MT6369_ACCDET_CON47
#define DA_AUDACCDETCMPCLK_SFT                        6
#define DA_AUDACCDETCMPCLK_MASK                       0x1
#define DA_AUDACCDETCMPCLK_MASK_SFT                   (0x1 << 6)
#define DA_AUDACCDETAUXADCSWCTRL_ADDR                 \
	MT6369_ACCDET_CON47
#define DA_AUDACCDETAUXADCSWCTRL_SFT                  7
#define DA_AUDACCDETAUXADCSWCTRL_MASK                 0x1
#define DA_AUDACCDETAUXADCSWCTRL_MASK_SFT             (0x1 << 7)
#define AD_EINT0CMPMOUT_ADDR                          \
	MT6369_ACCDET_CON48
#define AD_EINT0CMPMOUT_SFT                           0
#define AD_EINT0CMPMOUT_MASK                          0x1
#define AD_EINT0CMPMOUT_MASK_SFT                      (0x1 << 0)
#define AD_EINT0CMPOUT_ADDR                           \
	MT6369_ACCDET_CON48
#define AD_EINT0CMPOUT_SFT                            1
#define AD_EINT0CMPOUT_MASK                           0x1
#define AD_EINT0CMPOUT_MASK_SFT                       (0x1 << 1)
#define ACCDET_EINT0_CUR_IN_ADDR                      \
	MT6369_ACCDET_CON48
#define ACCDET_EINT0_CUR_IN_SFT                       2
#define ACCDET_EINT0_CUR_IN_MASK                      0x3
#define ACCDET_EINT0_CUR_IN_MASK_SFT                  (0x3 << 2)
#define ACCDET_EINT0_SAM_IN_ADDR                      \
	MT6369_ACCDET_CON48
#define ACCDET_EINT0_SAM_IN_SFT                       4
#define ACCDET_EINT0_SAM_IN_MASK                      0x3
#define ACCDET_EINT0_SAM_IN_MASK_SFT                  (0x3 << 4)
#define ACCDET_EINT0_MEM_IN_ADDR                      \
	MT6369_ACCDET_CON48
#define ACCDET_EINT0_MEM_IN_SFT                       6
#define ACCDET_EINT0_MEM_IN_MASK                      0x3
#define ACCDET_EINT0_MEM_IN_MASK_SFT                  (0x3 << 6)
#define ACCDET_EINT0_STATE_ADDR                       \
	MT6369_ACCDET_CON49
#define ACCDET_EINT0_STATE_SFT                        0
#define ACCDET_EINT0_STATE_MASK                       0x7
#define ACCDET_EINT0_STATE_MASK_SFT                   (0x7 << 0)
#define DA_EINT0CMPEN_ADDR                            \
	MT6369_ACCDET_CON49
#define DA_EINT0CMPEN_SFT                             5
#define DA_EINT0CMPEN_MASK                            0x1
#define DA_EINT0CMPEN_MASK_SFT                        (0x1 << 5)
#define DA_EINT0CMPMEN_ADDR                           \
	MT6369_ACCDET_CON49
#define DA_EINT0CMPMEN_SFT                            6
#define DA_EINT0CMPMEN_MASK                           0x1
#define DA_EINT0CMPMEN_MASK_SFT                       (0x1 << 6)
#define DA_EINT0CTURBO_ADDR                           \
	MT6369_ACCDET_CON49
#define DA_EINT0CTURBO_SFT                            7
#define DA_EINT0CTURBO_MASK                           0x1
#define DA_EINT0CTURBO_MASK_SFT                       (0x1 << 7)
#define AD_EINT1CMPMOUT_ADDR                          \
	MT6369_ACCDET_CON50
#define AD_EINT1CMPMOUT_SFT                           0
#define AD_EINT1CMPMOUT_MASK                          0x1
#define AD_EINT1CMPMOUT_MASK_SFT                      (0x1 << 0)
#define AD_EINT1CMPOUT_ADDR                           \
	MT6369_ACCDET_CON50
#define AD_EINT1CMPOUT_SFT                            1
#define AD_EINT1CMPOUT_MASK                           0x1
#define AD_EINT1CMPOUT_MASK_SFT                       (0x1 << 1)
#define ACCDET_EINT1_CUR_IN_ADDR                      \
	MT6369_ACCDET_CON50
#define ACCDET_EINT1_CUR_IN_SFT                       2
#define ACCDET_EINT1_CUR_IN_MASK                      0x3
#define ACCDET_EINT1_CUR_IN_MASK_SFT                  (0x3 << 2)
#define ACCDET_EINT1_SAM_IN_ADDR                      \
	MT6369_ACCDET_CON50
#define ACCDET_EINT1_SAM_IN_SFT                       4
#define ACCDET_EINT1_SAM_IN_MASK                      0x3
#define ACCDET_EINT1_SAM_IN_MASK_SFT                  (0x3 << 4)
#define ACCDET_EINT1_MEM_IN_ADDR                      \
	MT6369_ACCDET_CON50
#define ACCDET_EINT1_MEM_IN_SFT                       6
#define ACCDET_EINT1_MEM_IN_MASK                      0x3
#define ACCDET_EINT1_MEM_IN_MASK_SFT                  (0x3 << 6)
#define ACCDET_EINT1_STATE_ADDR                       \
	MT6369_ACCDET_CON51
#define ACCDET_EINT1_STATE_SFT                        0
#define ACCDET_EINT1_STATE_MASK                       0x7
#define ACCDET_EINT1_STATE_MASK_SFT                   (0x7 << 0)
#define DA_EINT1CMPEN_ADDR                            \
	MT6369_ACCDET_CON51
#define DA_EINT1CMPEN_SFT                             5
#define DA_EINT1CMPEN_MASK                            0x1
#define DA_EINT1CMPEN_MASK_SFT                        (0x1 << 5)
#define DA_EINT1CMPMEN_ADDR                           \
	MT6369_ACCDET_CON51
#define DA_EINT1CMPMEN_SFT                            6
#define DA_EINT1CMPMEN_MASK                           0x1
#define DA_EINT1CMPMEN_MASK_SFT                       (0x1 << 6)
#define DA_EINT1CTURBO_ADDR                           \
	MT6369_ACCDET_CON51
#define DA_EINT1CTURBO_SFT                            7
#define DA_EINT1CTURBO_MASK                           0x1
#define DA_EINT1CTURBO_MASK_SFT                       (0x1 << 7)
#define AD_EINT0INVOUT_ADDR                           \
	MT6369_ACCDET_CON52
#define AD_EINT0INVOUT_SFT                            0
#define AD_EINT0INVOUT_MASK                           0x1
#define AD_EINT0INVOUT_MASK_SFT                       (0x1 << 0)
#define ACCDET_EINT0_INVERTER_CUR_IN_ADDR             \
	MT6369_ACCDET_CON52
#define ACCDET_EINT0_INVERTER_CUR_IN_SFT              1
#define ACCDET_EINT0_INVERTER_CUR_IN_MASK             0x1
#define ACCDET_EINT0_INVERTER_CUR_IN_MASK_SFT         (0x1 << 1)
#define ACCDET_EINT0_INVERTER_SAM_IN_ADDR             \
	MT6369_ACCDET_CON52
#define ACCDET_EINT0_INVERTER_SAM_IN_SFT              2
#define ACCDET_EINT0_INVERTER_SAM_IN_MASK             0x1
#define ACCDET_EINT0_INVERTER_SAM_IN_MASK_SFT         (0x1 << 2)
#define ACCDET_EINT0_INVERTER_MEM_IN_ADDR             \
	MT6369_ACCDET_CON52
#define ACCDET_EINT0_INVERTER_MEM_IN_SFT              3
#define ACCDET_EINT0_INVERTER_MEM_IN_MASK             0x1
#define ACCDET_EINT0_INVERTER_MEM_IN_MASK_SFT         (0x1 << 3)
#define ACCDET_EINT0_INVERTER_STATE_ADDR              \
	MT6369_ACCDET_CON53
#define ACCDET_EINT0_INVERTER_STATE_SFT               0
#define ACCDET_EINT0_INVERTER_STATE_MASK              0x7
#define ACCDET_EINT0_INVERTER_STATE_MASK_SFT          (0x7 << 0)
#define DA_EINT0EN_ADDR                               \
	MT6369_ACCDET_CON53
#define DA_EINT0EN_SFT                                4
#define DA_EINT0EN_MASK                               0x1
#define DA_EINT0EN_MASK_SFT                           (0x1 << 4)
#define DA_EINT0INVEN_ADDR                            \
	MT6369_ACCDET_CON53
#define DA_EINT0INVEN_SFT                             5
#define DA_EINT0INVEN_MASK                            0x1
#define DA_EINT0INVEN_MASK_SFT                        (0x1 << 5)
#define DA_EINT0CEN_ADDR                              \
	MT6369_ACCDET_CON53
#define DA_EINT0CEN_SFT                               6
#define DA_EINT0CEN_MASK                              0x1
#define DA_EINT0CEN_MASK_SFT                          (0x1 << 6)
#define AD_EINT1INVOUT_ADDR                           \
	MT6369_ACCDET_CON54
#define AD_EINT1INVOUT_SFT                            0
#define AD_EINT1INVOUT_MASK                           0x1
#define AD_EINT1INVOUT_MASK_SFT                       (0x1 << 0)
#define ACCDET_EINT1_INVERTER_CUR_IN_ADDR             \
	MT6369_ACCDET_CON54
#define ACCDET_EINT1_INVERTER_CUR_IN_SFT              1
#define ACCDET_EINT1_INVERTER_CUR_IN_MASK             0x1
#define ACCDET_EINT1_INVERTER_CUR_IN_MASK_SFT         (0x1 << 1)
#define ACCDET_EINT1_INVERTER_SAM_IN_ADDR             \
	MT6369_ACCDET_CON54
#define ACCDET_EINT1_INVERTER_SAM_IN_SFT              2
#define ACCDET_EINT1_INVERTER_SAM_IN_MASK             0x1
#define ACCDET_EINT1_INVERTER_SAM_IN_MASK_SFT         (0x1 << 2)
#define ACCDET_EINT1_INVERTER_MEM_IN_ADDR             \
	MT6369_ACCDET_CON54
#define ACCDET_EINT1_INVERTER_MEM_IN_SFT              3
#define ACCDET_EINT1_INVERTER_MEM_IN_MASK             0x1
#define ACCDET_EINT1_INVERTER_MEM_IN_MASK_SFT         (0x1 << 3)
#define ACCDET_EINT1_INVERTER_STATE_ADDR              \
	MT6369_ACCDET_CON55
#define ACCDET_EINT1_INVERTER_STATE_SFT               0
#define ACCDET_EINT1_INVERTER_STATE_MASK              0x7
#define ACCDET_EINT1_INVERTER_STATE_MASK_SFT          (0x7 << 0)
#define DA_EINT1EN_ADDR                               \
	MT6369_ACCDET_CON55
#define DA_EINT1EN_SFT                                4
#define DA_EINT1EN_MASK                               0x1
#define DA_EINT1EN_MASK_SFT                           (0x1 << 4)
#define DA_EINT1INVEN_ADDR                            \
	MT6369_ACCDET_CON55
#define DA_EINT1INVEN_SFT                             5
#define DA_EINT1INVEN_MASK                            0x1
#define DA_EINT1INVEN_MASK_SFT                        (0x1 << 5)
#define DA_EINT1CEN_ADDR                              \
	MT6369_ACCDET_CON55
#define DA_EINT1CEN_SFT                               6
#define DA_EINT1CEN_MASK                              0x1
#define DA_EINT1CEN_MASK_SFT                          (0x1 << 6)
#define ACCDET_EN_ADDR                                \
	MT6369_ACCDET_CON56
#define ACCDET_EN_SFT                                 0
#define ACCDET_EN_MASK                                0x1
#define ACCDET_EN_MASK_SFT                            (0x1 << 0)
#define ACCDET_EINT0_EN_ADDR                          \
	MT6369_ACCDET_CON56
#define ACCDET_EINT0_EN_SFT                           1
#define ACCDET_EINT0_EN_MASK                          0x1
#define ACCDET_EINT0_EN_MASK_SFT                      (0x1 << 1)
#define ACCDET_EINT1_EN_ADDR                          \
	MT6369_ACCDET_CON56
#define ACCDET_EINT1_EN_SFT                           2
#define ACCDET_EINT1_EN_MASK                          0x1
#define ACCDET_EINT1_EN_MASK_SFT                      (0x1 << 2)
#define ACCDET_EINT0_M_EN_ADDR                        \
	MT6369_ACCDET_CON56
#define ACCDET_EINT0_M_EN_SFT                         3
#define ACCDET_EINT0_M_EN_MASK                        0x1
#define ACCDET_EINT0_M_EN_MASK_SFT                    (0x1 << 3)
#define ACCDET_EINT0_DETECT_MOISTURE_ADDR             \
	MT6369_ACCDET_CON56
#define ACCDET_EINT0_DETECT_MOISTURE_SFT              4
#define ACCDET_EINT0_DETECT_MOISTURE_MASK             0x1
#define ACCDET_EINT0_DETECT_MOISTURE_MASK_SFT         (0x1 << 4)
#define ACCDET_EINT0_PLUG_IN_ADDR                     \
	MT6369_ACCDET_CON56
#define ACCDET_EINT0_PLUG_IN_SFT                      5
#define ACCDET_EINT0_PLUG_IN_MASK                     0x1
#define ACCDET_EINT0_PLUG_IN_MASK_SFT                 (0x1 << 5)
#define ACCDET_EINT0_M_PLUG_IN_ADDR                   \
	MT6369_ACCDET_CON56
#define ACCDET_EINT0_M_PLUG_IN_SFT                    6
#define ACCDET_EINT0_M_PLUG_IN_MASK                   0x1
#define ACCDET_EINT0_M_PLUG_IN_MASK_SFT               (0x1 << 6)
#define ACCDET_EINT1_M_EN_ADDR                        \
	MT6369_ACCDET_CON56
#define ACCDET_EINT1_M_EN_SFT                         7
#define ACCDET_EINT1_M_EN_MASK                        0x1
#define ACCDET_EINT1_M_EN_MASK_SFT                    (0x1 << 7)
#define ACCDET_EINT1_DETECT_MOISTURE_ADDR             \
	MT6369_ACCDET_CON57
#define ACCDET_EINT1_DETECT_MOISTURE_SFT              0
#define ACCDET_EINT1_DETECT_MOISTURE_MASK             0x1
#define ACCDET_EINT1_DETECT_MOISTURE_MASK_SFT         (0x1 << 0)
#define ACCDET_EINT1_PLUG_IN_ADDR                     \
	MT6369_ACCDET_CON57
#define ACCDET_EINT1_PLUG_IN_SFT                      1
#define ACCDET_EINT1_PLUG_IN_MASK                     0x1
#define ACCDET_EINT1_PLUG_IN_MASK_SFT                 (0x1 << 1)
#define ACCDET_EINT1_M_PLUG_IN_ADDR                   \
	MT6369_ACCDET_CON57
#define ACCDET_EINT1_M_PLUG_IN_SFT                    2
#define ACCDET_EINT1_M_PLUG_IN_MASK                   0x1
#define ACCDET_EINT1_M_PLUG_IN_MASK_SFT               (0x1 << 2)
#define ACCDET_CUR_DEB_L_ADDR                         \
	MT6369_ACCDET_CON58
#define ACCDET_CUR_DEB_L_SFT                          0
#define ACCDET_CUR_DEB_L_MASK                         0xFF
#define ACCDET_CUR_DEB_L_MASK_SFT                     (0xFF << 0)
#define ACCDET_CUR_DEB_H_ADDR                         \
	MT6369_ACCDET_CON59
#define ACCDET_CUR_DEB_H_SFT                          0
#define ACCDET_CUR_DEB_H_MASK                         0xFF
#define ACCDET_CUR_DEB_H_MASK_SFT                     (0xFF << 0)
#define ACCDET_EINT0_CUR_DEB_L_ADDR                   \
	MT6369_ACCDET_CON60
#define ACCDET_EINT0_CUR_DEB_L_SFT                    0
#define ACCDET_EINT0_CUR_DEB_L_MASK                   0xFF
#define ACCDET_EINT0_CUR_DEB_L_MASK_SFT               (0xFF << 0)
#define ACCDET_EINT0_CUR_DEB_H_ADDR                   \
	MT6369_ACCDET_CON61
#define ACCDET_EINT0_CUR_DEB_H_SFT                    0
#define ACCDET_EINT0_CUR_DEB_H_MASK                   0x7F
#define ACCDET_EINT0_CUR_DEB_H_MASK_SFT               (0x7F << 0)
#define ACCDET_EINT1_CUR_DEB_L_ADDR                   \
	MT6369_ACCDET_CON62
#define ACCDET_EINT1_CUR_DEB_L_SFT                    0
#define ACCDET_EINT1_CUR_DEB_L_MASK                   0xFF
#define ACCDET_EINT1_CUR_DEB_L_MASK_SFT               (0xFF << 0)
#define ACCDET_EINT1_CUR_DEB_H_ADDR                   \
	MT6369_ACCDET_CON63
#define ACCDET_EINT1_CUR_DEB_H_SFT                    0
#define ACCDET_EINT1_CUR_DEB_H_MASK                   0x7F
#define ACCDET_EINT1_CUR_DEB_H_MASK_SFT               (0x7F << 0)
#define ACCDET_EINT0_INVERTER_CUR_DEB_L_ADDR          \
	MT6369_ACCDET_CON64
#define ACCDET_EINT0_INVERTER_CUR_DEB_L_SFT           0
#define ACCDET_EINT0_INVERTER_CUR_DEB_L_MASK          0xFF
#define ACCDET_EINT0_INVERTER_CUR_DEB_L_MASK_SFT      (0xFF << 0)
#define ACCDET_EINT0_INVERTER_CUR_DEB_H_ADDR          \
	MT6369_ACCDET_CON65
#define ACCDET_EINT0_INVERTER_CUR_DEB_H_SFT           0
#define ACCDET_EINT0_INVERTER_CUR_DEB_H_MASK          0x7F
#define ACCDET_EINT0_INVERTER_CUR_DEB_H_MASK_SFT      (0x7F << 0)
#define ACCDET_EINT1_INVERTER_CUR_DEB_L_ADDR          \
	MT6369_ACCDET_CON66
#define ACCDET_EINT1_INVERTER_CUR_DEB_L_SFT           0
#define ACCDET_EINT1_INVERTER_CUR_DEB_L_MASK          0xFF
#define ACCDET_EINT1_INVERTER_CUR_DEB_L_MASK_SFT      (0xFF << 0)
#define ACCDET_EINT1_INVERTER_CUR_DEB_H_ADDR          \
	MT6369_ACCDET_CON67
#define ACCDET_EINT1_INVERTER_CUR_DEB_H_SFT           0
#define ACCDET_EINT1_INVERTER_CUR_DEB_H_MASK          0x7F
#define ACCDET_EINT1_INVERTER_CUR_DEB_H_MASK_SFT      (0x7F << 0)
#define AD_AUDACCDETCMPOB_MON_ADDR                    \
	MT6369_ACCDET_CON68
#define AD_AUDACCDETCMPOB_MON_SFT                     0
#define AD_AUDACCDETCMPOB_MON_MASK                    0x1
#define AD_AUDACCDETCMPOB_MON_MASK_SFT                (0x1 << 0)
#define AD_AUDACCDETCMPOA_MON_ADDR                    \
	MT6369_ACCDET_CON68
#define AD_AUDACCDETCMPOA_MON_SFT                     1
#define AD_AUDACCDETCMPOA_MON_MASK                    0x1
#define AD_AUDACCDETCMPOA_MON_MASK_SFT                (0x1 << 1)
#define AD_EINT0CMPMOUT_MON_ADDR                      \
	MT6369_ACCDET_CON68
#define AD_EINT0CMPMOUT_MON_SFT                       2
#define AD_EINT0CMPMOUT_MON_MASK                      0x1
#define AD_EINT0CMPMOUT_MON_MASK_SFT                  (0x1 << 2)
#define AD_EINT0CMPOUT_MON_ADDR                       \
	MT6369_ACCDET_CON68
#define AD_EINT0CMPOUT_MON_SFT                        3
#define AD_EINT0CMPOUT_MON_MASK                       0x1
#define AD_EINT0CMPOUT_MON_MASK_SFT                   (0x1 << 3)
#define AD_EINT0INVOUT_MON_ADDR                       \
	MT6369_ACCDET_CON68
#define AD_EINT0INVOUT_MON_SFT                        4
#define AD_EINT0INVOUT_MON_MASK                       0x1
#define AD_EINT0INVOUT_MON_MASK_SFT                   (0x1 << 4)
#define AD_EINT1CMPMOUT_MON_ADDR                      \
	MT6369_ACCDET_CON68
#define AD_EINT1CMPMOUT_MON_SFT                       5
#define AD_EINT1CMPMOUT_MON_MASK                      0x1
#define AD_EINT1CMPMOUT_MON_MASK_SFT                  (0x1 << 5)
#define AD_EINT1CMPOUT_MON_ADDR                       \
	MT6369_ACCDET_CON68
#define AD_EINT1CMPOUT_MON_SFT                        6
#define AD_EINT1CMPOUT_MON_MASK                       0x1
#define AD_EINT1CMPOUT_MON_MASK_SFT                   (0x1 << 6)
#define AD_EINT1INVOUT_MON_ADDR                       \
	MT6369_ACCDET_CON68
#define AD_EINT1INVOUT_MON_SFT                        7
#define AD_EINT1INVOUT_MON_MASK                       0x1
#define AD_EINT1INVOUT_MON_MASK_SFT                   (0x1 << 7)
#define DA_AUDACCDETCMPCLK_MON_ADDR                   \
	MT6369_ACCDET_CON69
#define DA_AUDACCDETCMPCLK_MON_SFT                    0
#define DA_AUDACCDETCMPCLK_MON_MASK                   0x1
#define DA_AUDACCDETCMPCLK_MON_MASK_SFT               (0x1 << 0)
#define DA_AUDACCDETVTHCLK_MON_ADDR                   \
	MT6369_ACCDET_CON69
#define DA_AUDACCDETVTHCLK_MON_SFT                    1
#define DA_AUDACCDETVTHCLK_MON_MASK                   0x1
#define DA_AUDACCDETVTHCLK_MON_MASK_SFT               (0x1 << 1)
#define DA_AUDACCDETMBIASCLK_MON_ADDR                 \
	MT6369_ACCDET_CON69
#define DA_AUDACCDETMBIASCLK_MON_SFT                  2
#define DA_AUDACCDETMBIASCLK_MON_MASK                 0x1
#define DA_AUDACCDETMBIASCLK_MON_MASK_SFT             (0x1 << 2)
#define DA_AUDACCDETAUXADCSWCTRL_MON_ADDR             \
	MT6369_ACCDET_CON69
#define DA_AUDACCDETAUXADCSWCTRL_MON_SFT              3
#define DA_AUDACCDETAUXADCSWCTRL_MON_MASK             0x1
#define DA_AUDACCDETAUXADCSWCTRL_MON_MASK_SFT         (0x1 << 3)
#define DA_EINT0CTURBO_MON_ADDR                       \
	MT6369_ACCDET_CON70
#define DA_EINT0CTURBO_MON_SFT                        0
#define DA_EINT0CTURBO_MON_MASK                       0x1
#define DA_EINT0CTURBO_MON_MASK_SFT                   (0x1 << 0)
#define DA_EINT0CMPMEN_MON_ADDR                       \
	MT6369_ACCDET_CON70
#define DA_EINT0CMPMEN_MON_SFT                        1
#define DA_EINT0CMPMEN_MON_MASK                       0x1
#define DA_EINT0CMPMEN_MON_MASK_SFT                   (0x1 << 1)
#define DA_EINT0CMPEN_MON_ADDR                        \
	MT6369_ACCDET_CON70
#define DA_EINT0CMPEN_MON_SFT                         2
#define DA_EINT0CMPEN_MON_MASK                        0x1
#define DA_EINT0CMPEN_MON_MASK_SFT                    (0x1 << 2)
#define DA_EINT0INVEN_MON_ADDR                        \
	MT6369_ACCDET_CON70
#define DA_EINT0INVEN_MON_SFT                         3
#define DA_EINT0INVEN_MON_MASK                        0x1
#define DA_EINT0INVEN_MON_MASK_SFT                    (0x1 << 3)
#define DA_EINT0CEN_MON_ADDR                          \
	MT6369_ACCDET_CON70
#define DA_EINT0CEN_MON_SFT                           4
#define DA_EINT0CEN_MON_MASK                          0x1
#define DA_EINT0CEN_MON_MASK_SFT                      (0x1 << 4)
#define DA_EINT0EN_MON_ADDR                           \
	MT6369_ACCDET_CON70
#define DA_EINT0EN_MON_SFT                            5
#define DA_EINT0EN_MON_MASK                           0x1
#define DA_EINT0EN_MON_MASK_SFT                       (0x1 << 5)
#define DA_EINT1CTURBO_MON_ADDR                       \
	MT6369_ACCDET_CON71
#define DA_EINT1CTURBO_MON_SFT                        0
#define DA_EINT1CTURBO_MON_MASK                       0x1
#define DA_EINT1CTURBO_MON_MASK_SFT                   (0x1 << 0)
#define DA_EINT1CMPMEN_MON_ADDR                       \
	MT6369_ACCDET_CON71
#define DA_EINT1CMPMEN_MON_SFT                        1
#define DA_EINT1CMPMEN_MON_MASK                       0x1
#define DA_EINT1CMPMEN_MON_MASK_SFT                   (0x1 << 1)
#define DA_EINT1CMPEN_MON_ADDR                        \
	MT6369_ACCDET_CON71
#define DA_EINT1CMPEN_MON_SFT                         2
#define DA_EINT1CMPEN_MON_MASK                        0x1
#define DA_EINT1CMPEN_MON_MASK_SFT                    (0x1 << 2)
#define DA_EINT1INVEN_MON_ADDR                        \
	MT6369_ACCDET_CON71
#define DA_EINT1INVEN_MON_SFT                         3
#define DA_EINT1INVEN_MON_MASK                        0x1
#define DA_EINT1INVEN_MON_MASK_SFT                    (0x1 << 3)
#define DA_EINT1CEN_MON_ADDR                          \
	MT6369_ACCDET_CON71
#define DA_EINT1CEN_MON_SFT                           4
#define DA_EINT1CEN_MON_MASK                          0x1
#define DA_EINT1CEN_MON_MASK_SFT                      (0x1 << 4)
#define DA_EINT1EN_MON_ADDR                           \
	MT6369_ACCDET_CON71
#define DA_EINT1EN_MON_SFT                            5
#define DA_EINT1EN_MON_MASK                           0x1
#define DA_EINT1EN_MON_MASK_SFT                       (0x1 << 5)
#define ACCDET_EINT0_M_PLUG_IN_COUNT_ADDR             \
	MT6369_ACCDET_CON72
#define ACCDET_EINT0_M_PLUG_IN_COUNT_SFT              0
#define ACCDET_EINT0_M_PLUG_IN_COUNT_MASK             0x7
#define ACCDET_EINT0_M_PLUG_IN_COUNT_MASK_SFT         (0x7 << 0)
#define ACCDET_EINT1_M_PLUG_IN_COUNT_ADDR             \
	MT6369_ACCDET_CON72
#define ACCDET_EINT1_M_PLUG_IN_COUNT_SFT              4
#define ACCDET_EINT1_M_PLUG_IN_COUNT_MASK             0x7
#define ACCDET_EINT1_M_PLUG_IN_COUNT_MASK_SFT         (0x7 << 4)
#define ACCDET_MON_FLAG_EN_ADDR                       \
	MT6369_ACCDET_CON73
#define ACCDET_MON_FLAG_EN_SFT                        0
#define ACCDET_MON_FLAG_EN_MASK                       0x1
#define ACCDET_MON_FLAG_EN_MASK_SFT                   (0x1 << 0)
#define ACCDET_MON_FLAG_SEL_ADDR                      \
	MT6369_ACCDET_CON73
#define ACCDET_MON_FLAG_SEL_SFT                       4
#define ACCDET_MON_FLAG_SEL_MASK                      0xF
#define ACCDET_MON_FLAG_SEL_MASK_SFT                  (0xF << 4)


/* AUDENC_ANA_CON21 */
#define RG_ACCDET_MODE_MODE1            (0x0007)
#define RG_ACCDET_MODE_MODE2            (0x0087)
#define RG_ACCDET_MODE_MODE6            (0x0087)

#define ACCDET_CALI_MASK0               (0xFF)
#define ACCDET_CALI_MASK1               (0xFF << 8)
#define ACCDET_CALI_MASK2               (0xFF)
#define ACCDET_CALI_MASK3               (0xFF << 8)
#define ACCDET_CALI_MASK4               (0xFF)

#define ACCDET_EINT_IRQ_B2_B3           (0x03 << ACCDET_EINT0_IRQ_SFT)

/* ACCDET_CON46: RO, accdet FSM state, etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET      (ACCDET_MEM_IN_SFT)
#define ACCDET_STATE_AB_MASK            (0x03)
#define ACCDET_STATE_AB_00              (0x00)
#define ACCDET_STATE_AB_01              (0x01)
#define ACCDET_STATE_AB_10              (0x02)
#define ACCDET_STATE_AB_11              (0x03)

/* ACCDET_CON35 */
#define ACCDET_EINT0_STABLE_VAL ((ACCDET_DA_STABLE_MASK_SFT) | \
				(ACCDET_EINT0_EN_STABLE_MASK_SFT) | \
				(ACCDET_EINT0_CMPEN_STABLE_MASK_SFT) | \
				(ACCDET_EINT0_CEN_STABLE_MASK_SFT))

#define ACCDET_EINT1_STABLE_VAL ((ACCDET_DA_STABLE_MASK_SFT) | \
				(ACCDET_EINT1_EN_STABLE_MASK_SFT) | \
				(ACCDET_EINT1_CMPEN_STABLE_MASK_SFT) | \
				(ACCDET_EINT1_CEN_STABLE_MASK_SFT))


enum {
	MT6369_MTKAIF_PROTOCOL_1 = 0,
	MT6369_MTKAIF_PROTOCOL_2,
	MT6369_MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MT6369_AIF_1 = 0,       /* dl: hp, rcv, hp+lo */
	MT6369_AIF_2,           /* dl: lo only */
	MT6369_AIF_VOW,
	MT6369_AIF_NUM,
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
	AUDIO_ANALOG_VOLUME_MICAMP3,
	AUDIO_ANALOG_VOLUME_TYPE_MAX
};

enum {
	MUX_MIC_TYPE_0, /* ain0, micbias 0 */
	MUX_MIC_TYPE_1, /* ain1, micbias 1 */
	MUX_MIC_TYPE_2, /* ain2/3, micbias 2 */
	MUX_PGA_L,
	MUX_PGA_R,
	MUX_PGA_3,
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

enum {
	HP_GAIN_CTL_ZCD = 0,
	HP_GAIN_CTL_NLE,
	HP_GAIN_CTL_NUM,
};

/* Supply widget subseq */
enum {
	/* common */
	SUPPLY_SEQ_CLK_BUF,
	SUPPLY_SEQ_AUD_GLB,
	SUPPLY_SEQ_DL_GPIO,
	SUPPLY_SEQ_UL_GPIO,
	SUPPLY_SEQ_HP_PULL_DOWN,
	SUPPLY_SEQ_CLKSQ,
	SUPPLY_SEQ_ADC_CLKGEN,
	SUPPLY_SEQ_TOP_CK,
	SUPPLY_SEQ_TOP_CK_LAST,
	SUPPLY_SEQ_DCC_CLK,
	SUPPLY_SEQ_MIC_BIAS,
	SUPPLY_SEQ_DMIC,
	SUPPLY_SEQ_AUD_TOP,
	SUPPLY_SEQ_AUD_TOP_LAST,
	SUPPLY_SEQ_DL_SDM_FIFO_CLK,
	SUPPLY_SEQ_DL_SDM,
	SUPPLY_SEQ_DL_NCP,
	SUPPLY_SEQ_AFE,
	/* playback */
	SUPPLY_SEQ_DL_SRC,
	SUPPLY_SEQ_DL_ESD_RESIST,
	SUPPLY_SEQ_HP_DAMPING_OFF_RESET_CMFB,
	SUPPLY_SEQ_HP_MUTE,
	SUPPLY_SEQ_DL_LDO_VA33REFGEN,
	SUPPLY_SEQ_DL_LCLDO_REMOTE_SENSE,
	SUPPLY_SEQ_DL_HCLDO_REMOTE_SENSE,
	SUPPLY_SEQ_DL_LCLDO,
	SUPPLY_SEQ_DL_HCLDO,
	SUPPLY_SEQ_DL_NV,
	SUPPLY_SEQ_HP_ANA_TRIM,
	SUPPLY_SEQ_DL_IBIST,
	/* capture */
	SUPPLY_SEQ_UL_PGA,
	SUPPLY_SEQ_UL_ADC,
	SUPPLY_SEQ_UL_MTKAIF,
	SUPPLY_SEQ_UL_SRC_DMIC,
	SUPPLY_SEQ_UL_SRC,
	/* vow */
	SUPPLY_SEQ_VOW_AUD_LPW,
	SUPPLY_SEQ_AUD_VOW,
	SUPPLY_SEQ_VOW_CLK,
	SUPPLY_SEQ_VOW_LDO,
	SUPPLY_SEQ_VOW_PLL,
	SUPPLY_SEQ_AUD_GLB_VOW,
	SUPPLY_SEQ_VOW_DIG_CFG,
	SUPPLY_SEQ_VOW_PERIODIC_CFG,
};

enum {
	CH_L = 0,
	CH_R,
	NUM_CH,
};

enum {
	DRBIAS_4UA = 0,
	DRBIAS_5UA,
	DRBIAS_6UA,
	DRBIAS_7UA,
	DRBIAS_8UA,
	DRBIAS_9UA,
	DRBIAS_10UA,
	DRBIAS_11UA,
};

enum {
	IBIAS_4UA = 0,
	IBIAS_5UA,
	IBIAS_6UA,
	IBIAS_7UA,
};

enum {
	IBIAS_ZCD_3UA = 0,
	IBIAS_ZCD_4UA,
	IBIAS_ZCD_5UA,
	IBIAS_ZCD_6UA,
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
	DL_GAIN_8DB = 0,
	DL_GAIN_0DB = 8,
	DL_GAIN_N_1DB = 9,
	DL_GAIN_N_10DB = 18,
	DL_GAIN_N_40DB = 0x1f,
};

enum {
	MIC_TYPE_MUX_IDLE = 0,
	MIC_TYPE_MUX_ACC,
	MIC_TYPE_MUX_DMIC,
	MIC_TYPE_MUX_DCC,
	MIC_TYPE_MUX_DCC_ECM_DIFF,
	MIC_TYPE_MUX_DCC_ECM_SINGLE,
};

enum {
	MIC_INDEX_IDLE = 0,
	MIC_INDEX_MAIN,
	MIC_INDEX_REF,
	MIC_INDEX_THIRD,
	MIC_INDEX_HEADSET,
};

enum {
	LO_MUX_OPEN = 0,
	LO_MUX_L_DAC,
	LO_MUX_3RD_DAC,
	LO_MUX_TEST_MODE,
	LO_MUX_MASK = 0x3,
};

enum {
	HP_MUX_OPEN = 0,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
	HP_MUX_HP_DUALSPK,
	HP_MUX_MASK = 0x7,
};

enum {
	RCV_MUX_OPEN = 0,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
	RCV_MUX_MASK = 0x3,
};

enum {
	PGA_L_MUX_NONE = 0,
	PGA_L_MUX_AIN0,
	PGA_L_MUX_AIN1,
	PGA_L_MUX_AIN2,
};

enum {
	PGA_R_MUX_NONE = 0,
	PGA_R_MUX_AIN0,
	PGA_R_MUX_AIN1,
	PGA_R_MUX_AIN2,
};

enum {
	UL_SRC_MUX_AMIC = 0,
	UL_SRC_MUX_DMIC,
};

enum {
	MISO_MUX_UL1_CH1 = 0,
	MISO_MUX_UL1_CH2,
	MISO_MUX_UL2_CH1,
	MISO_MUX_UL2_CH2,
};

enum {
	VOW_AMIC_MUX_ADC_L = 0,
	VOW_AMIC_MUX_ADC_R,
	VOW_AMIC_MUX_ADC_T,
};

enum {
	DMIC_MUX_DMIC_DATA0 = 0,
	DMIC_MUX_DMIC_DATA1_L,
	DMIC_MUX_DMIC_DATA1_L_1,
	DMIC_MUX_DMIC_DATA1_R,
};

enum {
	ADC_MUX_IDLE = 0,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
};

enum {
	PGA_3_MUX_NONE = 0,
	PGA_3_MUX_AIN3,
	PGA_3_MUX_AIN2,
};

enum {
	VOW_MTKIF_TX_SET_STEREO = 0,
	VOW_MTKIF_TX_SET_MONO,
};

enum {
	TRIM_BUF_MUX_OPEN = 0,
	TRIM_BUF_MUX_HPL,
	TRIM_BUF_MUX_HPR,
	TRIM_BUF_MUX_HSP,
	TRIM_BUF_MUX_HSN,
	TRIM_BUF_MUX_LOLP,
	TRIM_BUF_MUX_LOLN,
	TRIM_BUF_MUX_AU_REFN,
	TRIM_BUF_MUX_AVSS32,
	TRIM_BUF_MUX_UNUSED,
};

enum {
	TRIM_BUF_GAIN_0DB = 0,
	TRIM_BUF_GAIN_6DB,
	TRIM_BUF_GAIN_12DB,
	TRIM_BUF_GAIN_18DB,
};

enum {
	TRIM_STEP0 = 0,
	TRIM_STEP1,
	TRIM_STEP2,
	TRIM_STEP3,
	TRIM_STEP_NUM,
};

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

struct dc_trim_data {
	bool calibrated;
	int mic_vinp_mv;
};

struct hp_trim_data {
	unsigned int hp_trim_l;
	unsigned int hp_trim_r;
	unsigned int hp_fine_trim_l;
	unsigned int hp_fine_trim_r;
};

struct mt6369_vow_periodic_on_off_data {
	unsigned long long pga_on;
	unsigned long long precg_on;
	unsigned long long adc_on;
	unsigned long long micbias0_on;
	unsigned long long micbias1_on;
	unsigned long long dcxo_on;
	unsigned long long audglb_on;
	unsigned long long vow_on;
	unsigned long long pga_off;
	unsigned long long precg_off;
	unsigned long long adc_off;
	unsigned long long micbias0_off;
	unsigned long long micbias1_off;
	unsigned long long dcxo_off;
	unsigned long long audglb_off;
	unsigned long long vow_off;
};

struct mt6369_codec_ops {
	int (*enable_dc_compensation)(bool enable);
	int (*set_lch_dc_compensation)(int value);
	int (*set_rch_dc_compensation)(int value);
	int (*adda_dl_gain_control)(bool mute);
	int (*set_adda_predistortion)(int hp_impedance);
};

struct mt6369_priv {
	struct device *dev;
	struct regmap *regmap;
	unsigned int dl_rate[MT6369_AIF_NUM];
	unsigned int ul_rate[MT6369_AIF_NUM];
	int ana_gain[AUDIO_ANALOG_VOLUME_TYPE_MAX];
	unsigned int mux_select[MUX_NUM];
	int dev_counter[DEVICE_NUM];
	int hp_gain_ctl;
	int hp_hifi_mode;
	bool apply_n12db_gain;
	int hp_plugged;
	int mtkaif_protocol;
	int dmic_one_wire_mode;

	/* dc trim */
	struct dc_trim_data dc_trim;
	struct hp_trim_data hp_trim_3_pole;
	struct hp_trim_data hp_trim_4_pole;
	struct iio_channel *hpofs_cal_auxadc;

	/* headphone impedence */
	struct nvmem_device *hp_efuse;
	int hp_impedance;
	int hp_current_calibrate_val;
	struct mt6369_codec_ops ops;

	/* debugfs */
	struct dentry *debugfs;

	/* vow control */
	int vow_enable;
	int reg_afe_vow_vad_cfg0;
	int reg_afe_vow_vad_cfg1;
	int reg_afe_vow_vad_cfg2;
	int reg_afe_vow_vad_cfg3;
	int reg_afe_vow_vad_cfg4;
	int reg_afe_vow_vad_cfg5;
	int reg_afe_vow_periodic;
	unsigned int vow_channel;
	struct mt6369_vow_periodic_on_off_data vow_periodic_param;
	/* vow dmic low power mode, 1: enable, 0: disable */
	int vow_dmic_lp;
	int vow_single_mic_select;
	int hwcid0;

	/* regulator */
	struct regulator *reg_vant18;
};

#define MT_SOC_ENUM_EXT_ID(xname, xenum, xhandler_get, xhandler_put, id) \
	{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .device = id,\
				.info = snd_soc_info_enum_double, \
					.get = xhandler_get, .put = xhandler_put, \
							.private_value = (unsigned long)&xenum }

/* dl bias */
#define DRBIAS_MASK 0x7
#define DRBIAS_HP_SFT (RG_AUDBIASADJ_0_HP_VAUDP15_SFT)
#define DRBIAS_HP_MASK_SFT (DRBIAS_MASK << DRBIAS_HP_SFT)
#define DRBIAS_HS_SFT (RG_AUDBIASADJ_0_HS_VAUDP15_SFT)
#define DRBIAS_HS_MASK_SFT (DRBIAS_MASK << DRBIAS_HS_SFT)
#define DRBIAS_LO_SFT (RG_AUDBIASADJ_0_LO_VAUDP15_SFT)
#define DRBIAS_LO_MASK_SFT (DRBIAS_MASK << DRBIAS_LO_SFT)
#define IBIAS_MASK 0x3
#define IBIAS_HP_SFT (RG_AUDBIASADJ_1_VAUDP15_SFT + 0)
#define IBIAS_HP_MASK_SFT (IBIAS_MASK << IBIAS_HP_SFT)
#define IBIAS_HS_SFT (RG_AUDBIASADJ_1_VAUDP15_SFT + 2)
#define IBIAS_HS_MASK_SFT (IBIAS_MASK << IBIAS_HS_SFT)
#define IBIAS_LO_SFT (RG_AUDBIASADJ_1_VAUDP15_SFT + 4)
#define IBIAS_LO_MASK_SFT (IBIAS_MASK << IBIAS_LO_SFT)
#define IBIAS_ZCD_SFT (RG_AUDBIASADJ_1_VAUDP15_SFT + 6)
#define IBIAS_ZCD_MASK_SFT (IBIAS_MASK << IBIAS_ZCD_SFT)

/* dl pga gain */
#define DL_GAIN_REG_MASK 0xff

/* mic type */

#define IS_DCC_BASE(x) (x == MIC_TYPE_MUX_DCC || \
			x == MIC_TYPE_MUX_DCC_ECM_DIFF || \
			x == MIC_TYPE_MUX_DCC_ECM_SINGLE)

#define IS_AMIC_BASE(x) (x == MIC_TYPE_MUX_ACC || IS_DCC_BASE(x))

/* VOW MTKIF TX setting */
#define VOW_MCLK 13000
#define VOW_MTKIF_TX_MONO_CLK 650
#define VOW_MTKIF_TX_STEREO_CLK 1083

/* reg idx for -40dB */
#define PGA_MINUS_40_DB_REG_VAL 0x1f
#define HP_PGA_MINUS_40_DB_REG_VAL 0x3f

/* dc trim */
#define TRIM_TIMES 7
#define TRIM_DISCARD_NUM 1
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

/* headphone impedance detection */
#define PARALLEL_OHM 0

/* codec name */
#define CODEC_MT6369_NAME "mtk-codec-mt6369"
#define DEVICE_MT6369_NAME "mt6369-sound"

int mt6369_set_codec_ops(struct snd_soc_component *cmpnt,
			 struct mt6369_codec_ops *ops);
int mt6369_set_mtkaif_protocol(struct snd_soc_component *cmpnt,
			       int mtkaif_protocol);
void mt6369_mtkaif_calibration_enable(struct snd_soc_component *cmpnt);
void mt6369_mtkaif_calibration_disable(struct snd_soc_component *cmpnt);
void mt6369_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
		int phase_1, int phase_2, int phase_3);

#endif/* end _MT6369_H_ */
