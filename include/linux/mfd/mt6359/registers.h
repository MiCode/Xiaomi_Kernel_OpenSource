/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MFD_MT6359_REGISTERS_H__
#define __MFD_MT6359_REGISTERS_H__

/* PMIC Registers */
#define MT6359_TOP0_ID                       0x0
#define MT6359_TOP0_REV0                     0x2
#define MT6359_TOP0_DSN_DBI                  0x4
#define MT6359_TOP0_DSN_DXI                  0x6
#define MT6359_HWCID                         0x8
#define MT6359_SWCID                         0xa
#define MT6359_PONSTS                        0xc
#define MT6359_POFFSTS                       0xe
#define MT6359_PSTSCTL                       0x10
#define MT6359_PG_DEB_STS0                   0x12
#define MT6359_PG_DEB_STS1                   0x14
#define MT6359_PG_SDN_STS0                   0x16
#define MT6359_PG_SDN_STS1                   0x18
#define MT6359_OC_SDN_STS0                   0x1a
#define MT6359_OC_SDN_STS1                   0x1c
#define MT6359_THERMALSTATUS                 0x1e
#define MT6359_TOP_CON                       0x20
#define MT6359_TEST_OUT                      0x22
#define MT6359_TEST_CON0                     0x24
#define MT6359_TEST_CON1                     0x26
#define MT6359_TESTMODE_SW                   0x28
#define MT6359_TOPSTATUS                     0x2a
#define MT6359_TDSEL_CON                     0x2c
#define MT6359_RDSEL_CON                     0x2e
#define MT6359_SMT_CON0                      0x30
#define MT6359_SMT_CON1                      0x32
#define MT6359_TOP_RSV0                      0x34
#define MT6359_TOP_RSV1                      0x36
#define MT6359_DRV_CON0                      0x38
#define MT6359_DRV_CON1                      0x3a
#define MT6359_DRV_CON2                      0x3c
#define MT6359_DRV_CON3                      0x3e
#define MT6359_DRV_CON4                      0x40
#define MT6359_FILTER_CON0                   0x42
#define MT6359_FILTER_CON1                   0x44
#define MT6359_FILTER_CON2                   0x46
#define MT6359_FILTER_CON3                   0x48
#define MT6359_TOP_STATUS                    0x4a
#define MT6359_TOP_STATUS_SET                0x4c
#define MT6359_TOP_STATUS_CLR                0x4e
#define MT6359_TOP_TRAP                      0x50
#define MT6359_TOP1_ID                       0x80
#define MT6359_TOP1_REV0                     0x82
#define MT6359_TOP1_DSN_DBI                  0x84
#define MT6359_TOP1_DSN_DXI                  0x86
#define MT6359_GPIO_DIR0                     0x88
#define MT6359_GPIO_DIR0_SET                 0x8a
#define MT6359_GPIO_DIR0_CLR                 0x8c
#define MT6359_GPIO_DIR1                     0x8e
#define MT6359_GPIO_DIR1_SET                 0x90
#define MT6359_GPIO_DIR1_CLR                 0x92
#define MT6359_GPIO_PULLEN0                  0x94
#define MT6359_GPIO_PULLEN0_SET              0x96
#define MT6359_GPIO_PULLEN0_CLR              0x98
#define MT6359_GPIO_PULLEN1                  0x9a
#define MT6359_GPIO_PULLEN1_SET              0x9c
#define MT6359_GPIO_PULLEN1_CLR              0x9e
#define MT6359_GPIO_PULLSEL0                 0xa0
#define MT6359_GPIO_PULLSEL0_SET             0xa2
#define MT6359_GPIO_PULLSEL0_CLR             0xa4
#define MT6359_GPIO_PULLSEL1                 0xa6
#define MT6359_GPIO_PULLSEL1_SET             0xa8
#define MT6359_GPIO_PULLSEL1_CLR             0xaa
#define MT6359_GPIO_DINV0                    0xac
#define MT6359_GPIO_DINV0_SET                0xae
#define MT6359_GPIO_DINV0_CLR                0xb0
#define MT6359_GPIO_DINV1                    0xb2
#define MT6359_GPIO_DINV1_SET                0xb4
#define MT6359_GPIO_DINV1_CLR                0xb6
#define MT6359_GPIO_DOUT0                    0xb8
#define MT6359_GPIO_DOUT0_SET                0xba
#define MT6359_GPIO_DOUT0_CLR                0xbc
#define MT6359_GPIO_DOUT1                    0xbe
#define MT6359_GPIO_DOUT1_SET                0xc0
#define MT6359_GPIO_DOUT1_CLR                0xc2
#define MT6359_GPIO_PI0                      0xc4
#define MT6359_GPIO_PI1                      0xc6
#define MT6359_GPIO_POE0                     0xc8
#define MT6359_GPIO_POE1                     0xca
#define MT6359_GPIO_MODE0                    0xcc
#define MT6359_GPIO_MODE0_SET                0xce
#define MT6359_GPIO_MODE0_CLR                0xd0
#define MT6359_GPIO_MODE1                    0xd2
#define MT6359_GPIO_MODE1_SET                0xd4
#define MT6359_GPIO_MODE1_CLR                0xd6
#define MT6359_GPIO_MODE2                    0xd8
#define MT6359_GPIO_MODE2_SET                0xda
#define MT6359_GPIO_MODE2_CLR                0xdc
#define MT6359_GPIO_MODE3                    0xde
#define MT6359_GPIO_MODE3_SET                0xe0
#define MT6359_GPIO_MODE3_CLR                0xe2
#define MT6359_GPIO_MODE4                    0xe4
#define MT6359_GPIO_MODE4_SET                0xe6
#define MT6359_GPIO_MODE4_CLR                0xe8
#define MT6359_GPIO_RSV                      0xea
#define MT6359_TOP2_ID                       0x100
#define MT6359_TOP2_REV0                     0x102
#define MT6359_TOP2_DSN_DBI                  0x104
#define MT6359_TOP2_DSN_DXI                  0x106
#define MT6359_TOP_PAM0                      0x108
#define MT6359_TOP_PAM1                      0x10a
#define MT6359_TOP_CKPDN_CON0                0x10c
#define MT6359_TOP_CKPDN_CON0_SET            0x10e
#define MT6359_TOP_CKPDN_CON0_CLR            0x110
#define MT6359_TOP_CKPDN_CON1                0x112
#define MT6359_TOP_CKPDN_CON1_SET            0x114
#define MT6359_TOP_CKPDN_CON1_CLR            0x116
#define MT6359_TOP_CKSEL_CON0                0x118
#define MT6359_TOP_CKSEL_CON0_SET            0x11a
#define MT6359_TOP_CKSEL_CON0_CLR            0x11c
#define MT6359_TOP_CKSEL_CON1                0x11e
#define MT6359_TOP_CKSEL_CON1_SET            0x120
#define MT6359_TOP_CKSEL_CON1_CLR            0x122
#define MT6359_TOP_CKDIVSEL_CON0             0x124
#define MT6359_TOP_CKDIVSEL_CON0_SET         0x126
#define MT6359_TOP_CKDIVSEL_CON0_CLR         0x128
#define MT6359_TOP_CKHWEN_CON0               0x12a
#define MT6359_TOP_CKHWEN_CON0_SET           0x12c
#define MT6359_TOP_CKHWEN_CON0_CLR           0x12e
#define MT6359_TOP_CKTST_CON0                0x130
#define MT6359_TOP_CKTST_CON1                0x132
#define MT6359_TOP_CLK_CON0                  0x134
#define MT6359_TOP_CLK_CON1                  0x136
#define MT6359_TOP_CLK_DCM0                  0x138
#define MT6359_TOP_RST_CON0                  0x13a
#define MT6359_TOP_RST_CON0_SET              0x13c
#define MT6359_TOP_RST_CON0_CLR              0x13e
#define MT6359_TOP_RST_CON1                  0x140
#define MT6359_TOP_RST_CON1_SET              0x142
#define MT6359_TOP_RST_CON1_CLR              0x144
#define MT6359_TOP_RST_CON2                  0x146
#define MT6359_TOP_RST_CON3                  0x148
#define MT6359_TOP_RST_MISC                  0x14a
#define MT6359_TOP_RST_MISC_SET              0x14c
#define MT6359_TOP_RST_MISC_CLR              0x14e
#define MT6359_TOP_RST_STATUS                0x150
#define MT6359_TOP_RST_STATUS_SET            0x152
#define MT6359_TOP_RST_STATUS_CLR            0x154
#define MT6359_TOP2_ELR_NUM                  0x156
#define MT6359_TOP2_ELR0                     0x158
#define MT6359_TOP2_ELR1                     0x15a
#define MT6359_TOP3_ID                       0x180
#define MT6359_TOP3_REV0                     0x182
#define MT6359_TOP3_DSN_DBI                  0x184
#define MT6359_TOP3_DSN_DXI                  0x186
#define MT6359_MISC_TOP_INT_CON0             0x188
#define MT6359_MISC_TOP_INT_CON0_SET         0x18a
#define MT6359_MISC_TOP_INT_CON0_CLR         0x18c
#define MT6359_MISC_TOP_INT_MASK_CON0        0x18e
#define MT6359_MISC_TOP_INT_MASK_CON0_SET    0x190
#define MT6359_MISC_TOP_INT_MASK_CON0_CLR    0x192
#define MT6359_MISC_TOP_INT_STATUS0          0x194
#define MT6359_MISC_TOP_INT_RAW_STATUS0      0x196
#define MT6359_TOP_INT_MASK_CON0             0x198
#define MT6359_TOP_INT_MASK_CON0_SET         0x19a
#define MT6359_TOP_INT_MASK_CON0_CLR         0x19c
#define MT6359_TOP_INT_STATUS0               0x19e
#define MT6359_TOP_INT_RAW_STATUS0           0x1a0
#define MT6359_TOP_INT_CON0                  0x1a2
#define MT6359_TOP_DCXO_CKEN_SW              0x1a4
#define MT6359_PMRC_CON0                     0x1a6
#define MT6359_PMRC_CON0_SET                 0x1a8
#define MT6359_PMRC_CON0_CLR                 0x1aa
#define MT6359_PMRC_CON1                     0x1ac
#define MT6359_PMRC_CON1_SET                 0x1ae
#define MT6359_PMRC_CON1_CLR                 0x1b0
#define MT6359_PMRC_CON2                     0x1b2
#define MT6359_PLT0_ID                       0x380
#define MT6359_PLT0_REV0                     0x382
#define MT6359_PLT0_REV1                     0x384
#define MT6359_PLT0_DSN_DXI                  0x386
#define MT6359_TOP_CLK_TRIM                  0x388
#define MT6359_OTP_CON0                      0x38a
#define MT6359_OTP_CON1                      0x38c
#define MT6359_OTP_CON2                      0x38e
#define MT6359_OTP_CON3                      0x390
#define MT6359_OTP_CON4                      0x392
#define MT6359_OTP_CON5                      0x394
#define MT6359_OTP_CON6                      0x396
#define MT6359_OTP_CON7                      0x398
#define MT6359_OTP_CON8                      0x39a
#define MT6359_OTP_CON9                      0x39c
#define MT6359_OTP_CON10                     0x39e
#define MT6359_OTP_CON11                     0x3a0
#define MT6359_OTP_CON12                     0x3a2
#define MT6359_OTP_CON13                     0x3a4
#define MT6359_OTP_CON14                     0x3a6
#define MT6359_TOP_TMA_KEY                   0x3a8
#define MT6359_TOP_MDB_CONF0                 0x3aa
#define MT6359_TOP_MDB_CONF1                 0x3ac
#define MT6359_TOP_MDB_CONF2                 0x3ae
#define MT6359_TOP_MDB_CONF3                 0x3b0
#define MT6359_PLT0_ELR_NUM                  0x3b2
#define MT6359_PLT0_ELR0                     0x3b4
#define MT6359_SPISLV_ID                     0x400
#define MT6359_SPISLV_REV0                   0x402
#define MT6359_SPISLV_REV1                   0x404
#define MT6359_SPISLV_DSN_DXI                0x406
#define MT6359_RG_SPI_CON0                   0x408
#define MT6359_RG_SPI_RECORD0                0x40a
#define MT6359_DEW_DIO_EN                    0x40c
#define MT6359_DEW_READ_TEST                 0x40e
#define MT6359_DEW_WRITE_TEST                0x410
#define MT6359_DEW_CRC_SWRST                 0x412
#define MT6359_DEW_CRC_EN                    0x414
#define MT6359_DEW_CRC_VAL                   0x416
#define MT6359_DEW_CIPHER_KEY_SEL            0x418
#define MT6359_DEW_CIPHER_IV_SEL             0x41a
#define MT6359_DEW_CIPHER_EN                 0x41c
#define MT6359_DEW_CIPHER_RDY                0x41e
#define MT6359_DEW_CIPHER_MODE               0x420
#define MT6359_DEW_CIPHER_SWRST              0x422
#define MT6359_DEW_RDDMY_NO                  0x424
#define MT6359_RG_SPI_CON2                   0x426
#define MT6359_RECORD_CMD0                   0x428
#define MT6359_RECORD_CMD1                   0x42a
#define MT6359_RECORD_CMD2                   0x42c
#define MT6359_RECORD_CMD3                   0x42e
#define MT6359_RECORD_CMD4                   0x430
#define MT6359_RECORD_CMD5                   0x432
#define MT6359_RECORD_WDATA0                 0x434
#define MT6359_RECORD_WDATA1                 0x436
#define MT6359_RECORD_WDATA2                 0x438
#define MT6359_RECORD_WDATA3                 0x43a
#define MT6359_RECORD_WDATA4                 0x43c
#define MT6359_RECORD_WDATA5                 0x43e
#define MT6359_RG_SPI_CON9                   0x440
#define MT6359_RG_SPI_CON10                  0x442
#define MT6359_RG_SPI_CON11                  0x444
#define MT6359_RG_SPI_CON12                  0x446
#define MT6359_RG_SPI_CON13                  0x448
#define MT6359_SPISLV_KEY                    0x44a
#define MT6359_INT_TYPE_CON0                 0x44c
#define MT6359_INT_TYPE_CON0_SET             0x44e
#define MT6359_INT_TYPE_CON0_CLR             0x450
#define MT6359_INT_STA                       0x452
#define MT6359_RG_SPI_CON1                   0x454
#define MT6359_TOP_SPI_CON0                  0x456
#define MT6359_TOP_SPI_CON1                  0x458
#define MT6359_SCK_TOP_DSN_ID                0x500
#define MT6359_SCK_TOP_DSN_REV0              0x502
#define MT6359_SCK_TOP_DBI                   0x504
#define MT6359_SCK_TOP_DXI                   0x506
#define MT6359_SCK_TOP_TPM0                  0x508
#define MT6359_SCK_TOP_TPM1                  0x50a
#define MT6359_SCK_TOP_CON0                  0x50c
#define MT6359_SCK_TOP_CON1                  0x50e
#define MT6359_SCK_TOP_TEST_OUT              0x510
#define MT6359_SCK_TOP_TEST_CON0             0x512
#define MT6359_SCK_TOP_CKPDN_CON0            0x514
#define MT6359_SCK_TOP_CKPDN_CON0_SET        0x516
#define MT6359_SCK_TOP_CKPDN_CON0_CLR        0x518
#define MT6359_SCK_TOP_CKHWEN_CON0           0x51a
#define MT6359_SCK_TOP_CKHWEN_CON0_SET       0x51c
#define MT6359_SCK_TOP_CKHWEN_CON0_CLR       0x51e
#define MT6359_SCK_TOP_CKTST_CON             0x520
#define MT6359_SCK_TOP_RST_CON0              0x522
#define MT6359_SCK_TOP_RST_CON0_SET          0x524
#define MT6359_SCK_TOP_RST_CON0_CLR          0x526
#define MT6359_SCK_TOP_INT_CON0              0x528
#define MT6359_SCK_TOP_INT_CON0_SET          0x52a
#define MT6359_SCK_TOP_INT_CON0_CLR          0x52c
#define MT6359_SCK_TOP_INT_MASK_CON0         0x52e
#define MT6359_SCK_TOP_INT_MASK_CON0_SET     0x530
#define MT6359_SCK_TOP_INT_MASK_CON0_CLR     0x532
#define MT6359_SCK_TOP_INT_STATUS0           0x534
#define MT6359_SCK_TOP_INT_RAW_STATUS0       0x536
#define MT6359_SCK_TOP_INT_MISC_CON          0x538
#define MT6359_EOSC_CALI_CON0                0x53a
#define MT6359_EOSC_CALI_CON1                0x53c
#define MT6359_RTC_MIX_CON0                  0x53e
#define MT6359_RTC_MIX_CON1                  0x540
#define MT6359_RTC_MIX_CON2                  0x542
#define MT6359_RTC_DIG_CON0                  0x544
#define MT6359_FQMTR_CON0                    0x546
#define MT6359_FQMTR_CON1                    0x548
#define MT6359_FQMTR_CON2                    0x54a
#define MT6359_XO_BUF_CTL0                   0x54c
#define MT6359_XO_BUF_CTL1                   0x54e
#define MT6359_XO_BUF_CTL2                   0x550
#define MT6359_XO_BUF_CTL3                   0x552
#define MT6359_XO_BUF_CTL4                   0x554
#define MT6359_XO_CONN_BT0                   0x556
#define MT6359_RTC_DSN_ID                    0x580
#define MT6359_RTC_DSN_REV0                  0x582
#define MT6359_RTC_DBI                       0x584
#define MT6359_RTC_DXI                       0x586
#define MT6359_RTC_BBPU                      0x588
#define MT6359_RTC_IRQ_STA                   0x58a
#define MT6359_RTC_IRQ_EN                    0x58c
#define MT6359_RTC_CII_EN                    0x58e
#define MT6359_RTC_AL_MASK                   0x590
#define MT6359_RTC_TC_SEC                    0x592
#define MT6359_RTC_TC_MIN                    0x594
#define MT6359_RTC_TC_HOU                    0x596
#define MT6359_RTC_TC_DOM                    0x598
#define MT6359_RTC_TC_DOW                    0x59a
#define MT6359_RTC_TC_MTH                    0x59c
#define MT6359_RTC_TC_YEA                    0x59e
#define MT6359_RTC_AL_SEC                    0x5a0
#define MT6359_RTC_AL_MIN                    0x5a2
#define MT6359_RTC_AL_HOU                    0x5a4
#define MT6359_RTC_AL_DOM                    0x5a6
#define MT6359_RTC_AL_DOW                    0x5a8
#define MT6359_RTC_AL_MTH                    0x5aa
#define MT6359_RTC_AL_YEA                    0x5ac
#define MT6359_RTC_OSC32CON                  0x5ae
#define MT6359_RTC_POWERKEY1                 0x5b0
#define MT6359_RTC_POWERKEY2                 0x5b2
#define MT6359_RTC_PDN1                      0x5b4
#define MT6359_RTC_PDN2                      0x5b6
#define MT6359_RTC_SPAR0                     0x5b8
#define MT6359_RTC_SPAR1                     0x5ba
#define MT6359_RTC_PROT                      0x5bc
#define MT6359_RTC_DIFF                      0x5be
#define MT6359_RTC_CALI                      0x5c0
#define MT6359_RTC_WRTGR                     0x5c2
#define MT6359_RTC_CON                       0x5c4
#define MT6359_RTC_SEC_CTRL                  0x5c6
#define MT6359_RTC_INT_CNT                   0x5c8
#define MT6359_RTC_SEC_DAT0                  0x5ca
#define MT6359_RTC_SEC_DAT1                  0x5cc
#define MT6359_RTC_SEC_DAT2                  0x5ce
#define MT6359_RTC_SEC_DSN_ID                0x600
#define MT6359_RTC_SEC_DSN_REV0              0x602
#define MT6359_RTC_SEC_DBI                   0x604
#define MT6359_RTC_SEC_DXI                   0x606
#define MT6359_RTC_TC_SEC_SEC                0x608
#define MT6359_RTC_TC_MIN_SEC                0x60a
#define MT6359_RTC_TC_HOU_SEC                0x60c
#define MT6359_RTC_TC_DOM_SEC                0x60e
#define MT6359_RTC_TC_DOW_SEC                0x610
#define MT6359_RTC_TC_MTH_SEC                0x612
#define MT6359_RTC_TC_YEA_SEC                0x614
#define MT6359_RTC_SEC_CK_PDN                0x616
#define MT6359_RTC_SEC_WRTGR                 0x618
#define MT6359_DCXO_DSN_ID                   0x780
#define MT6359_DCXO_DSN_REV0                 0x782
#define MT6359_DCXO_DSN_DBI                  0x784
#define MT6359_DCXO_DSN_DXI                  0x786
#define MT6359_DCXO_CW00                     0x788
#define MT6359_DCXO_CW00_SET                 0x78a
#define MT6359_DCXO_CW00_CLR                 0x78c
#define MT6359_DCXO_CW01                     0x78e
#define MT6359_DCXO_CW02                     0x790
#define MT6359_DCXO_CW03                     0x792
#define MT6359_DCXO_CW04                     0x794
#define MT6359_DCXO_CW05                     0x796
#define MT6359_DCXO_CW06                     0x798
#define MT6359_DCXO_CW07                     0x79a
#define MT6359_DCXO_CW08                     0x79c
#define MT6359_DCXO_CW09                     0x79e
#define MT6359_DCXO_CW09_SET                 0x7a0
#define MT6359_DCXO_CW09_CLR                 0x7a2
#define MT6359_DCXO_CW10                     0x7a4
#define MT6359_DCXO_CW11                     0x7a6
#define MT6359_DCXO_CW12                     0x7a8
#define MT6359_DCXO_CW13                     0x7aa
#define MT6359_DCXO_CW14                     0x7ac
#define MT6359_DCXO_CW15                     0x7ae
#define MT6359_DCXO_CW16                     0x7b0
#define MT6359_DCXO_CW17                     0x7b2
#define MT6359_DCXO_CW18                     0x7b4
#define MT6359_DCXO_CW19                     0x7b6
#define MT6359_DCXO_ELR_NUM                  0x7b8
#define MT6359_DCXO_ELR0                     0x7ba
#define MT6359_PSC_TOP_ID                    0x900
#define MT6359_PSC_TOP_REV0                  0x902
#define MT6359_PSC_TOP_DBI                   0x904
#define MT6359_PSC_TOP_DXI                   0x906
#define MT6359_PSC_TPM0                      0x908
#define MT6359_PSC_TPM1                      0x90a
#define MT6359_PSC_TOP_CLKCTL_0              0x90c
#define MT6359_PSC_TOP_RSTCTL_0              0x90e
#define MT6359_PSC_TOP_INT_CON0              0x910
#define MT6359_PSC_TOP_INT_CON0_SET          0x912
#define MT6359_PSC_TOP_INT_CON0_CLR          0x914
#define MT6359_PSC_TOP_INT_MASK_CON0         0x916
#define MT6359_PSC_TOP_INT_MASK_CON0_SET     0x918
#define MT6359_PSC_TOP_INT_MASK_CON0_CLR     0x91a
#define MT6359_PSC_TOP_INT_STATUS0           0x91c
#define MT6359_PSC_TOP_INT_RAW_STATUS0       0x91e
#define MT6359_PSC_TOP_INT_MISC_CON          0x920
#define MT6359_PSC_TOP_INT_MISC_CON_SET      0x922
#define MT6359_PSC_TOP_INT_MISC_CON_CLR      0x924
#define MT6359_PSC_TOP_MON_CTL               0x926
#define MT6359_STRUP_ID                      0x980
#define MT6359_STRUP_REV0                    0x982
#define MT6359_STRUP_DBI                     0x984
#define MT6359_STRUP_DSN_FPI                 0x986
#define MT6359_STRUP_ANA_CON0                0x988
#define MT6359_STRUP_ANA_CON1                0x98a
#define MT6359_STRUP_ANA_CON2                0x98c
#define MT6359_STRUP_ANA_CON3                0x98e
#define MT6359_STRUP_ELR_NUM                 0x990
#define MT6359_STRUP_ELR_0                   0x992
#define MT6359_PSEQ_ID                       0xa00
#define MT6359_PSEQ_REV0                     0xa02
#define MT6359_PSEQ_DBI                      0xa04
#define MT6359_PSEQ_DXI                      0xa06
#define MT6359_PPCCTL0                       0xa08
#define MT6359_PPCCTL1                       0xa0a
#define MT6359_PPCCFG0                       0xa0c
#define MT6359_STRUP_CON9                    0xa0e
#define MT6359_STRUP_CON11                   0xa10
#define MT6359_STRUP_CON12                   0xa12
#define MT6359_STRUP_CON13                   0xa14
#define MT6359_PWRKEY_PRESS_STS              0xa16
#define MT6359_PORFLAG                       0xa18
#define MT6359_STRUP_CON4                    0xa1a
#define MT6359_STRUP_CON1                    0xa1c
#define MT6359_STRUP_CON2                    0xa1e
#define MT6359_STRUP_CON5                    0xa20
#define MT6359_STRUP_CON19                   0xa22
#define MT6359_STRUP_PGDEB0                  0xa24
#define MT6359_STRUP_PGDEB1                  0xa26
#define MT6359_STRUP_PGENB0                  0xa28
#define MT6359_STRUP_PGENB1                  0xa2a
#define MT6359_STRUP_OCENB0                  0xa2c
#define MT6359_STRUP_OCENB1                  0xa2e
#define MT6359_PPCTST0                       0xa30
#define MT6359_PPCCTL2                       0xa32
#define MT6359_STRUP_CON10                   0xa34
#define MT6359_STRUP_CON3                    0xa36
#define MT6359_STRUP_CON6                    0xa38
#define MT6359_CPSWKEY                       0xa3a
#define MT6359_CPSCFG0                       0xa3c
#define MT6359_CPSDSA0                       0xa3e
#define MT6359_CPSDSA1                       0xa40
#define MT6359_CPSDSA2                       0xa42
#define MT6359_CPSDSA3                       0xa44
#define MT6359_CPSDSA4                       0xa46
#define MT6359_CPSDSA5                       0xa48
#define MT6359_CPSDSA6                       0xa4a
#define MT6359_CPSDSA7                       0xa4c
#define MT6359_CPSDSA8                       0xa4e
#define MT6359_CPSDSA9                       0xa50
#define MT6359_PSEQ_ELR_NUM                  0xa52
#define MT6359_PSEQ_ELR0                     0xa54
#define MT6359_PSEQ_ELR1                     0xa56
#define MT6359_PSEQ_ELR2                     0xa58
#define MT6359_PSEQ_ELR3                     0xa5a
#define MT6359_CPSUSA_ELR0                   0xa5c
#define MT6359_CPSUSA_ELR1                   0xa5e
#define MT6359_CPSUSA_ELR2                   0xa60
#define MT6359_CPSUSA_ELR3                   0xa62
#define MT6359_CPSUSA_ELR4                   0xa64
#define MT6359_CPSUSA_ELR5                   0xa66
#define MT6359_CPSUSA_ELR6                   0xa68
#define MT6359_CPSUSA_ELR7                   0xa6a
#define MT6359_CPSUSA_ELR8                   0xa6c
#define MT6359_CPSUSA_ELR9                   0xa6e
#define MT6359_CHRDET_ID                     0xa80
#define MT6359_CHRDET_REV0                   0xa82
#define MT6359_CHRDET_DBI                    0xa84
#define MT6359_CHRDET_DXI                    0xa86
#define MT6359_CHR_CON0                      0xa88
#define MT6359_CHR_CON1                      0xa8a
#define MT6359_CHR_CON2                      0xa8c
#define MT6359_PCHR_VREF_ANA_DA0             0xa8e
#define MT6359_PCHR_VREF_ANA_CON0            0xa90
#define MT6359_PCHR_VREF_ANA_CON1            0xa92
#define MT6359_PCHR_VREF_ANA_CON2            0xa94
#define MT6359_PCHR_VREF_ANA_CON3            0xa96
#define MT6359_PCHR_VREF_ANA_CON4            0xa98
#define MT6359_FGD_BGR_ANA_CON0              0xa9a
#define MT6359_PCHR_VREF_ELR_NUM             0xa9c
#define MT6359_PCHR_VREF_ELR_0               0xa9e
#define MT6359_BM_TOP_DSN_ID                 0xc00
#define MT6359_BM_TOP_DSN_REV0               0xc02
#define MT6359_BM_TOP_DBI                    0xc04
#define MT6359_BM_TOP_DXI                    0xc06
#define MT6359_BM_TPM0                       0xc08
#define MT6359_BM_TPM1                       0xc0a
#define MT6359_BM_TOP_CKPDN_CON0             0xc0c
#define MT6359_BM_TOP_CKPDN_CON0_SET         0xc0e
#define MT6359_BM_TOP_CKPDN_CON0_CLR         0xc10
#define MT6359_BM_TOP_CKSEL_CON0             0xc12
#define MT6359_BM_TOP_CKSEL_CON0_SET         0xc14
#define MT6359_BM_TOP_CKSEL_CON0_CLR         0xc16
#define MT6359_BM_TOP_CKDIVSEL_CON0          0xc18
#define MT6359_BM_TOP_CKDIVSEL_CON0_SET      0xc1a
#define MT6359_BM_TOP_CKDIVSEL_CON0_CLR      0xc1c
#define MT6359_BM_TOP_CKHWEN_CON0            0xc1e
#define MT6359_BM_TOP_CKHWEN_CON0_SET        0xc20
#define MT6359_BM_TOP_CKHWEN_CON0_CLR        0xc22
#define MT6359_BM_TOP_CKTST_CON0             0xc24
#define MT6359_BM_TOP_RST_CON0               0xc26
#define MT6359_BM_TOP_RST_CON0_SET           0xc28
#define MT6359_BM_TOP_RST_CON0_CLR           0xc2a
#define MT6359_BM_TOP_RST_CON1               0xc2c
#define MT6359_BM_TOP_RST_CON1_SET           0xc2e
#define MT6359_BM_TOP_RST_CON1_CLR           0xc30
#define MT6359_BM_TOP_INT_CON0               0xc32
#define MT6359_BM_TOP_INT_CON0_SET           0xc34
#define MT6359_BM_TOP_INT_CON0_CLR           0xc36
#define MT6359_BM_TOP_INT_CON1               0xc38
#define MT6359_BM_TOP_INT_CON1_SET           0xc3a
#define MT6359_BM_TOP_INT_CON1_CLR           0xc3c
#define MT6359_BM_TOP_INT_MASK_CON0          0xc3e
#define MT6359_BM_TOP_INT_MASK_CON0_SET      0xc40
#define MT6359_BM_TOP_INT_MASK_CON0_CLR      0xc42
#define MT6359_BM_TOP_INT_MASK_CON1          0xc44
#define MT6359_BM_TOP_INT_MASK_CON1_SET      0xc46
#define MT6359_BM_TOP_INT_MASK_CON1_CLR      0xc48
#define MT6359_BM_TOP_INT_STATUS0            0xc4a
#define MT6359_BM_TOP_INT_STATUS1            0xc4c
#define MT6359_BM_TOP_INT_RAW_STATUS0        0xc4e
#define MT6359_BM_TOP_INT_RAW_STATUS1        0xc50
#define MT6359_BM_TOP_INT_MISC_CON           0xc52
#define MT6359_BM_TOP_DBG_CON                0xc54
#define MT6359_BM_TOP_RSV0                   0xc56
#define MT6359_BM_WKEY0                      0xc58
#define MT6359_BM_WKEY1                      0xc5a
#define MT6359_BM_WKEY2                      0xc5c
#define MT6359_FGADC_ANA_DSN_ID              0xc80
#define MT6359_FGADC_ANA_DSN_REV0            0xc82
#define MT6359_FGADC_ANA_DSN_DBI             0xc84
#define MT6359_FGADC_ANA_DSN_DXI             0xc86
#define MT6359_FGADC_ANA_CON0                0xc88
#define MT6359_FGADC_ANA_TEST_CON0           0xc8a
#define MT6359_FGADC_ANA_ELR_NUM             0xc8c
#define MT6359_FGADC_ANA_ELR0                0xc8e
#define MT6359_FGADC0_DSN_ID                 0xd00
#define MT6359_FGADC0_DSN_REV0               0xd02
#define MT6359_FGADC0_DSN_DBI                0xd04
#define MT6359_FGADC0_DSN_DXI                0xd06
#define MT6359_FGADC_CON0                    0xd08
#define MT6359_FGADC_CON1                    0xd0a
#define MT6359_FGADC_CON2                    0xd0c
#define MT6359_FGADC_CON3                    0xd0e
#define MT6359_FGADC_CON4                    0xd10
#define MT6359_FGADC_CON5                    0xd12
#define MT6359_FGADC_RST_CON0                0xd14
#define MT6359_FGADC_CAR_CON0                0xd16
#define MT6359_FGADC_CAR_CON1                0xd18
#define MT6359_FGADC_CARTH_CON0              0xd1c
#define MT6359_FGADC_CARTH_CON1              0xd1e
#define MT6359_FGADC_CARTH_CON2              0xd20
#define MT6359_FGADC_CARTH_CON3              0xd22
#define MT6359_FGADC_NCAR_CON0               0xd24
#define MT6359_FGADC_NCAR_CON1               0xd26
#define MT6359_FGADC_NCAR_CON2               0xd28
#define MT6359_FGADC_NCAR_CON3               0xd2a
#define MT6359_FGADC_IAVG_CON0               0xd2c
#define MT6359_FGADC_IAVG_CON1               0xd2e
#define MT6359_FGADC_IAVG_CON2               0xd30
#define MT6359_FGADC_IAVG_CON3               0xd32
#define MT6359_FGADC_IAVG_CON4               0xd34
#define MT6359_FGADC_IAVG_CON5               0xd36
#define MT6359_FGADC_NTER_CON0               0xd38
#define MT6359_FGADC_NTER_CON1               0xd3a
#define MT6359_FGADC_SON_CON0                0xd3e
#define MT6359_FGADC_SON_CON1                0xd40
#define MT6359_FGADC_SON_CON2                0xd42
#define MT6359_FGADC_SOFF_CON0               0xd44
#define MT6359_FGADC_SOFF_CON1               0xd46
#define MT6359_FGADC_SOFF_CON2               0xd48
#define MT6359_FGADC_SOFF_CON3               0xd4a
#define MT6359_FGADC_SOFF_CON4               0xd4c
#define MT6359_FGADC_ZCV_CON0                0xd4e
#define MT6359_FGADC_ZCV_CON1                0xd50
#define MT6359_FGADC_ZCV_CON2                0xd52
#define MT6359_FGADC_ZCV_CON3                0xd54
#define MT6359_FGADC_ZCVTH_CON0              0xd58
#define MT6359_FGADC_ZCVTH_CON1              0xd5a
#define MT6359_FGADC1_DSN_ID                 0xd80
#define MT6359_FGADC1_DSN_REV0               0xd82
#define MT6359_FGADC1_DSN_DBI                0xd84
#define MT6359_FGADC1_DSN_DXI                0xd86
#define MT6359_FGADC_R_CON0                  0xd88
#define MT6359_FGADC_CUR_CON0                0xd8a
#define MT6359_FGADC_CUR_CON1                0xd8c
#define MT6359_FGADC_CUR_CON2                0xd8e
#define MT6359_FGADC_CUR_CON3                0xd90
#define MT6359_FGADC_OFFSET_CON0             0xd92
#define MT6359_FGADC_OFFSET_CON1             0xd94
#define MT6359_FGADC_GAIN_CON0               0xd96
#define MT6359_FGADC_TEST_CON0               0xd98
#define MT6359_SYSTEM_INFO_CON0              0xd9a
#define MT6359_SYSTEM_INFO_CON1              0xd9c
#define MT6359_SYSTEM_INFO_CON2              0xd9e
#define MT6359_BATON_ANA_DSN_ID              0xe00
#define MT6359_BATON_ANA_DSN_REV0            0xe02
#define MT6359_BATON_ANA_DSN_DBI             0xe04
#define MT6359_BATON_ANA_DSN_DXI             0xe06
#define MT6359_BATON_ANA_CON0                0xe08
#define MT6359_BATON_ANA_MON0                0xe0a
#define MT6359_BIF_ANA_MON0                  0xe0c
#define MT6359_BATON_DSN_ID                  0xe80
#define MT6359_BATON_DSN_REV0                0xe82
#define MT6359_BATON_DSN_DBI                 0xe84
#define MT6359_BATON_DSN_DXI                 0xe86
#define MT6359_BATON_CON0                    0xe88
#define MT6359_BATON_CON1                    0xe8a
#define MT6359_BATON_CON2                    0xe8c
#define MT6359_BIF_DSN_ID                    0xf00
#define MT6359_BIF_DSN_REV0                  0xf02
#define MT6359_BIF_DSN_DBI                   0xf04
#define MT6359_BIF_DSN_DXI                   0xf06
#define MT6359_BIF_CON0                      0xf08
#define MT6359_BIF_CON1                      0xf0a
#define MT6359_BIF_CON2                      0xf0c
#define MT6359_BIF_CON3                      0xf0e
#define MT6359_BIF_CON4                      0xf10
#define MT6359_BIF_CON5                      0xf12
#define MT6359_BIF_CON6                      0xf14
#define MT6359_BIF_CON7                      0xf16
#define MT6359_BIF_CON8                      0xf18
#define MT6359_BIF_CON9                      0xf1a
#define MT6359_BIF_CON10                     0xf1c
#define MT6359_BIF_CON11                     0xf1e
#define MT6359_BIF_CON12                     0xf20
#define MT6359_BIF_CON13                     0xf22
#define MT6359_BIF_CON14                     0xf24
#define MT6359_BIF_CON15                     0xf26
#define MT6359_BIF_CON16                     0xf28
#define MT6359_BIF_CON17                     0xf2a
#define MT6359_BIF_CON18                     0xf2c
#define MT6359_BIF_CON19                     0xf2e
#define MT6359_BIF_CON20                     0xf30
#define MT6359_BIF_CON21                     0xf32
#define MT6359_BIF_CON22                     0xf34
#define MT6359_BIF_CON23                     0xf36
#define MT6359_BIF_CON24                     0xf38
#define MT6359_BIF_CON25                     0xf3a
#define MT6359_BIF_CON26                     0xf3c
#define MT6359_BIF_CON27                     0xf3e
#define MT6359_BIF_CON28                     0xf40
#define MT6359_BIF_CON29                     0xf42
#define MT6359_BIF_CON30                     0xf44
#define MT6359_BIF_CON31                     0xf46
#define MT6359_BIF_CON32                     0xf48
#define MT6359_BIF_CON33                     0xf4a
#define MT6359_BIF_CON34                     0xf4c
#define MT6359_BIF_CON35                     0xf4e
#define MT6359_BIF_CON36                     0xf50
#define MT6359_BIF_CON37                     0xf52
#define MT6359_BIF_CON38                     0xf54
#define MT6359_BIF_CON39                     0xf56
#define MT6359_HK_TOP_ID                     0xf80
#define MT6359_HK_TOP_REV0                   0xf82
#define MT6359_HK_TOP_DBI                    0xf84
#define MT6359_HK_TOP_DXI                    0xf86
#define MT6359_HK_TPM0                       0xf88
#define MT6359_HK_TPM1                       0xf8a
#define MT6359_HK_TOP_CLK_CON0               0xf8c
#define MT6359_HK_TOP_CLK_CON1               0xf8e
#define MT6359_HK_TOP_RST_CON0               0xf90
#define MT6359_HK_TOP_INT_CON0               0xf92
#define MT6359_HK_TOP_INT_CON0_SET           0xf94
#define MT6359_HK_TOP_INT_CON0_CLR           0xf96
#define MT6359_HK_TOP_INT_MASK_CON0          0xf98
#define MT6359_HK_TOP_INT_MASK_CON0_SET      0xf9a
#define MT6359_HK_TOP_INT_MASK_CON0_CLR      0xf9c
#define MT6359_HK_TOP_INT_STATUS0            0xf9e
#define MT6359_HK_TOP_INT_RAW_STATUS0        0xfa0
#define MT6359_HK_TOP_MON_CON0               0xfa2
#define MT6359_HK_TOP_MON_CON1               0xfa4
#define MT6359_HK_TOP_MON_CON2               0xfa6
#define MT6359_HK_TOP_CHR_CON                0xfa8
#define MT6359_HK_TOP_ANA_CON                0xfaa
#define MT6359_HK_TOP_AUXADC_ANA             0xfac
#define MT6359_HK_TOP_STRUP                  0xfae
#define MT6359_HK_TOP_LDO_CON                0xfb0
#define MT6359_HK_TOP_LDO_STATUS             0xfb2
#define MT6359_HK_TOP_WKEY                   0xfb4
#define MT6359_AUXADC_DSN_ID                 0x1000
#define MT6359_AUXADC_DSN_REV0               0x1002
#define MT6359_AUXADC_DSN_DBI                0x1004
#define MT6359_AUXADC_DSN_FPI                0x1006
#define MT6359_AUXADC_ANA_CON0               0x1008
#define MT6359_AUXADC_ANA_CON1               0x100a
#define MT6359_AUXADC_ELR_NUM                0x100c
#define MT6359_AUXADC_ELR_0                  0x100e
#define MT6359_AUXADC_DIG_1_DSN_ID           0x1080
#define MT6359_AUXADC_DIG_1_DSN_REV0         0x1082
#define MT6359_AUXADC_DIG_1_DSN_DBI          0x1084
#define MT6359_AUXADC_DIG_1_DSN_DXI          0x1086
#define MT6359_AUXADC_ADC0                   0x1088
#define MT6359_AUXADC_ADC1                   0x108a
#define MT6359_AUXADC_ADC2                   0x108c
#define MT6359_AUXADC_ADC3                   0x108e
#define MT6359_AUXADC_ADC4                   0x1090
#define MT6359_AUXADC_ADC5                   0x1092
#define MT6359_AUXADC_ADC6                   0x1094
#define MT6359_AUXADC_ADC7                   0x1096
#define MT6359_AUXADC_ADC8                   0x1098
#define MT6359_AUXADC_ADC9                   0x109a
#define MT6359_AUXADC_ADC10                  0x109c
#define MT6359_AUXADC_ADC11                  0x109e
#define MT6359_AUXADC_ADC12                  0x10a0
#define MT6359_AUXADC_ADC15                  0x10a2
#define MT6359_AUXADC_ADC16                  0x10a4
#define MT6359_AUXADC_ADC17                  0x10a6
#define MT6359_AUXADC_ADC18                  0x10a8
#define MT6359_AUXADC_ADC19                  0x10aa
#define MT6359_AUXADC_ADC20                  0x10ac
#define MT6359_AUXADC_ADC21                  0x10ae
#define MT6359_AUXADC_ADC22                  0x10b0
#define MT6359_AUXADC_ADC23                  0x10b2
#define MT6359_AUXADC_ADC24                  0x10b4
#define MT6359_AUXADC_ADC26                  0x10b6
#define MT6359_AUXADC_ADC27                  0x10b8
#define MT6359_AUXADC_ADC30                  0x10ba
#define MT6359_AUXADC_ADC32                  0x10bc
#define MT6359_AUXADC_ADC33                  0x10be
#define MT6359_AUXADC_ADC34                  0x10c0
#define MT6359_AUXADC_ADC37                  0x10c2
#define MT6359_AUXADC_ADC38                  0x10c4
#define MT6359_AUXADC_ADC39                  0x10c6
#define MT6359_AUXADC_ADC40                  0x10c8
#define MT6359_AUXADC_STA0                   0x10ca
#define MT6359_AUXADC_STA1                   0x10cc
#define MT6359_AUXADC_STA2                   0x10ce
#define MT6359_AUXADC_DIG_2_DSN_ID           0x1100
#define MT6359_AUXADC_DIG_2_DSN_REV0         0x1102
#define MT6359_AUXADC_DIG_2_DSN_DBI          0x1104
#define MT6359_AUXADC_DIG_2_DSN_DXI          0x1106
#define MT6359_AUXADC_RQST0                  0x1108
#define MT6359_AUXADC_RQST1                  0x110a
#define MT6359_AUXADC_DIG_3_DSN_ID           0x1180
#define MT6359_AUXADC_DIG_3_DSN_REV0         0x1182
#define MT6359_AUXADC_DIG_3_DSN_DBI          0x1184
#define MT6359_AUXADC_DIG_3_DSN_DXI          0x1186
#define MT6359_AUXADC_CON0                   0x1188
#define MT6359_AUXADC_CON0_SET               0x118a
#define MT6359_AUXADC_CON0_CLR               0x118c
#define MT6359_AUXADC_CON1                   0x118e
#define MT6359_AUXADC_CON2                   0x1190
#define MT6359_AUXADC_CON3                   0x1192
#define MT6359_AUXADC_CON4                   0x1194
#define MT6359_AUXADC_CON5                   0x1196
#define MT6359_AUXADC_CON6                   0x1198
#define MT6359_AUXADC_CON7                   0x119a
#define MT6359_AUXADC_CON8                   0x119c
#define MT6359_AUXADC_CON9                   0x119e
#define MT6359_AUXADC_CON10                  0x11a0
#define MT6359_AUXADC_CON11                  0x11a2
#define MT6359_AUXADC_CON12                  0x11a4
#define MT6359_AUXADC_CON13                  0x11a6
#define MT6359_AUXADC_CON14                  0x11a8
#define MT6359_AUXADC_CON15                  0x11aa
#define MT6359_AUXADC_CON16                  0x11ac
#define MT6359_AUXADC_CON17                  0x11ae
#define MT6359_AUXADC_CON18                  0x11b0
#define MT6359_AUXADC_CON19                  0x11b2
#define MT6359_AUXADC_CON20                  0x11b4
#define MT6359_AUXADC_CON21                  0x11b6
#define MT6359_AUXADC_AUTORPT0               0x11b8
#define MT6359_AUXADC_ACCDET                 0x11ba
#define MT6359_AUXADC_DBG0                   0x11bc
#define MT6359_AUXADC_NAG_0                  0x11be
#define MT6359_AUXADC_NAG_1                  0x11c0
#define MT6359_AUXADC_NAG_2                  0x11c2
#define MT6359_AUXADC_NAG_3                  0x11c4
#define MT6359_AUXADC_NAG_4                  0x11c6
#define MT6359_AUXADC_NAG_5                  0x11c8
#define MT6359_AUXADC_NAG_6                  0x11ca
#define MT6359_AUXADC_NAG_7                  0x11cc
#define MT6359_AUXADC_NAG_8                  0x11ce
#define MT6359_AUXADC_NAG_9                  0x11d0
#define MT6359_AUXADC_NAG_10                 0x11d2
#define MT6359_AUXADC_NAG_11                 0x11d4
#define MT6359_AUXADC_DIG_3_ELR_NUM          0x11d6
#define MT6359_AUXADC_DIG_3_ELR0             0x11d8
#define MT6359_AUXADC_DIG_3_ELR1             0x11da
#define MT6359_AUXADC_DIG_3_ELR2             0x11dc
#define MT6359_AUXADC_DIG_3_ELR3             0x11de
#define MT6359_AUXADC_DIG_3_ELR4             0x11e0
#define MT6359_AUXADC_DIG_3_ELR5             0x11e2
#define MT6359_AUXADC_DIG_3_ELR6             0x11e4
#define MT6359_AUXADC_DIG_3_ELR7             0x11e6
#define MT6359_AUXADC_DIG_3_ELR8             0x11e8
#define MT6359_AUXADC_DIG_3_ELR9             0x11ea
#define MT6359_AUXADC_DIG_3_ELR10            0x11ec
#define MT6359_AUXADC_DIG_3_ELR11            0x11ee
#define MT6359_AUXADC_DIG_3_ELR12            0x11f0
#define MT6359_AUXADC_DIG_3_ELR13            0x11f2
#define MT6359_AUXADC_DIG_3_ELR14            0x11f4
#define MT6359_AUXADC_DIG_3_ELR15            0x11f6
#define MT6359_AUXADC_DIG_3_ELR16            0x11f8
#define MT6359_AUXADC_DIG_4_DSN_ID           0x1200
#define MT6359_AUXADC_DIG_4_DSN_REV0         0x1202
#define MT6359_AUXADC_DIG_4_DSN_DBI          0x1204
#define MT6359_AUXADC_DIG_4_DSN_DXI          0x1206
#define MT6359_AUXADC_IMP0                   0x1208
#define MT6359_AUXADC_IMP1                   0x120a
#define MT6359_AUXADC_IMP2                   0x120c
#define MT6359_AUXADC_IMP3                   0x120e
#define MT6359_AUXADC_IMP4                   0x1210
#define MT6359_AUXADC_IMP5                   0x1212
#define MT6359_AUXADC_LBAT0                  0x1214
#define MT6359_AUXADC_LBAT1                  0x1216
#define MT6359_AUXADC_LBAT2                  0x1218
#define MT6359_AUXADC_LBAT3                  0x121a
#define MT6359_AUXADC_LBAT4                  0x121c
#define MT6359_AUXADC_LBAT5                  0x121e
#define MT6359_AUXADC_LBAT6                  0x1220
#define MT6359_AUXADC_LBAT7                  0x1222
#define MT6359_AUXADC_LBAT8                  0x1224
#define MT6359_AUXADC_BAT_TEMP_0             0x1226
#define MT6359_AUXADC_BAT_TEMP_1             0x1228
#define MT6359_AUXADC_BAT_TEMP_2             0x122a
#define MT6359_AUXADC_BAT_TEMP_3             0x122c
#define MT6359_AUXADC_BAT_TEMP_4             0x122e
#define MT6359_AUXADC_BAT_TEMP_5             0x1230
#define MT6359_AUXADC_BAT_TEMP_6             0x1232
#define MT6359_AUXADC_BAT_TEMP_7             0x1234
#define MT6359_AUXADC_BAT_TEMP_8             0x1236
#define MT6359_AUXADC_BAT_TEMP_9             0x1238
#define MT6359_AUXADC_LBAT2_0                0x123a
#define MT6359_AUXADC_LBAT2_1                0x123c
#define MT6359_AUXADC_LBAT2_2                0x123e
#define MT6359_AUXADC_LBAT2_3                0x1240
#define MT6359_AUXADC_LBAT2_4                0x1242
#define MT6359_AUXADC_LBAT2_5                0x1244
#define MT6359_AUXADC_LBAT2_6                0x1246
#define MT6359_AUXADC_LBAT2_7                0x1248
#define MT6359_AUXADC_LBAT2_8                0x124a
#define MT6359_AUXADC_THR0                   0x124c
#define MT6359_AUXADC_THR1                   0x124e
#define MT6359_AUXADC_THR2                   0x1250
#define MT6359_AUXADC_THR3                   0x1252
#define MT6359_AUXADC_THR4                   0x1254
#define MT6359_AUXADC_THR5                   0x1256
#define MT6359_AUXADC_THR6                   0x1258
#define MT6359_AUXADC_THR7                   0x125a
#define MT6359_AUXADC_THR8                   0x125c
#define MT6359_AUXADC_MDRT_0                 0x125e
#define MT6359_AUXADC_MDRT_1                 0x1260
#define MT6359_AUXADC_MDRT_2                 0x1262
#define MT6359_AUXADC_MDRT_3                 0x1264
#define MT6359_AUXADC_MDRT_4                 0x1266
#define MT6359_AUXADC_MDRT_5                 0x1268
#define MT6359_AUXADC_DCXO_MDRT_1            0x126a
#define MT6359_AUXADC_DCXO_MDRT_2            0x126c
#define MT6359_AUXADC_DCXO_MDRT_3            0x126e
#define MT6359_AUXADC_DCXO_MDRT_4            0x1270
#define MT6359_AUXADC_RSV_1                  0x1272
#define MT6359_AUXADC_PRI_NEW                0x1274
#define MT6359_AUXADC_SPL_LIST_0             0x1276
#define MT6359_AUXADC_SPL_LIST_1             0x1278
#define MT6359_AUXADC_SPL_LIST_2             0x127a
#define MT6359_BUCK_TOP_DSN_ID               0x1400
#define MT6359_BUCK_TOP_DSN_REV0             0x1402
#define MT6359_BUCK_TOP_DBI                  0x1404
#define MT6359_BUCK_TOP_DXI                  0x1406
#define MT6359_BUCK_TOP_PAM0                 0x1408
#define MT6359_BUCK_TOP_PAM1                 0x140a
#define MT6359_BUCK_TOP_CLK_CON0             0x140c
#define MT6359_BUCK_TOP_CLK_CON0_SET         0x140e
#define MT6359_BUCK_TOP_CLK_CON0_CLR         0x1410
#define MT6359_BUCK_TOP_CLK_HWEN_CON0        0x1412
#define MT6359_BUCK_TOP_CLK_HWEN_CON0_SET    0x1414
#define MT6359_BUCK_TOP_CLK_HWEN_CON0_CLR    0x1416
#define MT6359_BUCK_TOP_INT_CON0             0x1418
#define MT6359_BUCK_TOP_INT_CON0_SET         0x141a
#define MT6359_BUCK_TOP_INT_CON0_CLR         0x141c
#define MT6359_BUCK_TOP_INT_MASK_CON0        0x141e
#define MT6359_BUCK_TOP_INT_MASK_CON0_SET    0x1420
#define MT6359_BUCK_TOP_INT_MASK_CON0_CLR    0x1422
#define MT6359_BUCK_TOP_INT_STATUS0          0x1424
#define MT6359_BUCK_TOP_INT_RAW_STATUS0      0x1426
#define MT6359_BUCK_TOP_VOW_CON              0x1428
#define MT6359_BUCK_TOP_STB_CON              0x142a
#define MT6359_BUCK_TOP_VGP2_MINFREQ_CON     0x142c
#define MT6359_BUCK_TOP_VPA_MINFREQ_CON      0x142e
#define MT6359_BUCK_TOP_OC_CON0              0x1430
#define MT6359_BUCK_TOP_KEY_PROT             0x1432
#define MT6359_BUCK_TOP_WDTDBG0              0x1434
#define MT6359_BUCK_TOP_WDTDBG1              0x1436
#define MT6359_BUCK_TOP_WDTDBG2              0x1438
#define MT6359_BUCK_TOP_WDTDBG3              0x143a
#define MT6359_BUCK_TOP_WDTDBG4              0x143c
#define MT6359_BUCK_TOP_ELR_NUM              0x143e
#define MT6359_BUCK_TOP_ELR0                 0x1440
#define MT6359_BUCK_TOP_ELR1                 0x1442
#define MT6359_BUCK_TOP_ELR2                 0x1444
#define MT6359_BUCK_VPU_DSN_ID               0x1480
#define MT6359_BUCK_VPU_DSN_REV0             0x1482
#define MT6359_BUCK_VPU_DSN_DBI              0x1484
#define MT6359_BUCK_VPU_DSN_DXI              0x1486
#define MT6359_BUCK_VPU_CON0                 0x1488
#define MT6359_BUCK_VPU_CON0_SET             0x148a
#define MT6359_BUCK_VPU_CON0_CLR             0x148c
#define MT6359_BUCK_VPU_CON1                 0x148e
#define MT6359_BUCK_VPU_SLP_CON              0x1490
#define MT6359_BUCK_VPU_CFG0                 0x1492
#define MT6359_BUCK_VPU_OP_EN                0x1494
#define MT6359_BUCK_VPU_OP_EN_SET            0x1496
#define MT6359_BUCK_VPU_OP_EN_CLR            0x1498
#define MT6359_BUCK_VPU_OP_CFG               0x149a
#define MT6359_BUCK_VPU_OP_CFG_SET           0x149c
#define MT6359_BUCK_VPU_OP_CFG_CLR           0x149e
#define MT6359_BUCK_VPU_OP_MODE              0x14a0
#define MT6359_BUCK_VPU_OP_MODE_SET          0x14a2
#define MT6359_BUCK_VPU_OP_MODE_CLR          0x14a4
#define MT6359_BUCK_VPU_DBG0                 0x14a6
#define MT6359_BUCK_VPU_DBG1                 0x14a8
#define MT6359_BUCK_VPU_ELR_NUM              0x14aa
#define MT6359_BUCK_VPU_ELR0                 0x14ac
#define MT6359_BUCK_VCORE_DSN_ID             0x1500
#define MT6359_BUCK_VCORE_DSN_REV0           0x1502
#define MT6359_BUCK_VCORE_DSN_DBI            0x1504
#define MT6359_BUCK_VCORE_DSN_DXI            0x1506
#define MT6359_BUCK_VCORE_CON0               0x1508
#define MT6359_BUCK_VCORE_CON0_SET           0x150a
#define MT6359_BUCK_VCORE_CON0_CLR           0x150c
#define MT6359_BUCK_VCORE_CON1               0x150e
#define MT6359_BUCK_VCORE_SLP_CON            0x1510
#define MT6359_BUCK_VCORE_CFG0               0x1512
#define MT6359_BUCK_VCORE_OP_EN              0x1514
#define MT6359_BUCK_VCORE_OP_EN_SET          0x1516
#define MT6359_BUCK_VCORE_OP_EN_CLR          0x1518
#define MT6359_BUCK_VCORE_OP_CFG             0x151a
#define MT6359_BUCK_VCORE_OP_CFG_SET         0x151c
#define MT6359_BUCK_VCORE_OP_CFG_CLR         0x151e
#define MT6359_BUCK_VCORE_OP_MODE            0x1520
#define MT6359_BUCK_VCORE_OP_MODE_SET        0x1522
#define MT6359_BUCK_VCORE_OP_MODE_CLR        0x1524
#define MT6359_BUCK_VCORE_DBG0               0x1526
#define MT6359_BUCK_VCORE_DBG1               0x1528
#define MT6359_BUCK_VCORE_SSHUB_CON0         0x152a
#define MT6359_BUCK_VCORE_SPI_CON0           0x152c
#define MT6359_BUCK_VCORE_BT_LP_CON0         0x152e
#define MT6359_BUCK_VCORE_STALL_TRACK0       0x1530
#define MT6359_BUCK_VCORE_ELR_NUM            0x1532
#define MT6359_BUCK_VCORE_ELR0               0x1534
#define MT6359_BUCK_VGPU11_DSN_ID            0x1580
#define MT6359_BUCK_VGPU11_DSN_REV0          0x1582
#define MT6359_BUCK_VGPU11_DSN_DBI           0x1584
#define MT6359_BUCK_VGPU11_DSN_DXI           0x1586
#define MT6359_BUCK_VGPU11_CON0              0x1588
#define MT6359_BUCK_VGPU11_CON0_SET          0x158a
#define MT6359_BUCK_VGPU11_CON0_CLR          0x158c
#define MT6359_BUCK_VGPU11_CON1              0x158e
#define MT6359_BUCK_VGPU11_SLP_CON           0x1590
#define MT6359_BUCK_VGPU11_CFG0              0x1592
#define MT6359_BUCK_VGPU11_OP_EN             0x1594
#define MT6359_BUCK_VGPU11_OP_EN_SET         0x1596
#define MT6359_BUCK_VGPU11_OP_EN_CLR         0x1598
#define MT6359_BUCK_VGPU11_OP_CFG            0x159a
#define MT6359_BUCK_VGPU11_OP_CFG_SET        0x159c
#define MT6359_BUCK_VGPU11_OP_CFG_CLR        0x159e
#define MT6359_BUCK_VGPU11_OP_MODE           0x15a0
#define MT6359_BUCK_VGPU11_OP_MODE_SET       0x15a2
#define MT6359_BUCK_VGPU11_OP_MODE_CLR       0x15a4
#define MT6359_BUCK_VGPU11_DBG0              0x15a6
#define MT6359_BUCK_VGPU11_DBG1              0x15a8
#define MT6359_BUCK_VGPU11_ELR_NUM           0x15aa
#define MT6359_BUCK_VGPU11_ELR0              0x15ac
#define MT6359_BUCK_VGPU12_DSN_ID            0x1600
#define MT6359_BUCK_VGPU12_DSN_REV0          0x1602
#define MT6359_BUCK_VGPU12_DSN_DBI           0x1604
#define MT6359_BUCK_VGPU12_DSN_DXI           0x1606
#define MT6359_BUCK_VGPU12_CON0              0x1608
#define MT6359_BUCK_VGPU12_CON0_SET          0x160a
#define MT6359_BUCK_VGPU12_CON0_CLR          0x160c
#define MT6359_BUCK_VGPU12_CON1              0x160e
#define MT6359_BUCK_VGPU12_SLP_CON           0x1610
#define MT6359_BUCK_VGPU12_CFG0              0x1612
#define MT6359_BUCK_VGPU12_OP_EN             0x1614
#define MT6359_BUCK_VGPU12_OP_EN_SET         0x1616
#define MT6359_BUCK_VGPU12_OP_EN_CLR         0x1618
#define MT6359_BUCK_VGPU12_OP_CFG            0x161a
#define MT6359_BUCK_VGPU12_OP_CFG_SET        0x161c
#define MT6359_BUCK_VGPU12_OP_CFG_CLR        0x161e
#define MT6359_BUCK_VGPU12_OP_MODE           0x1620
#define MT6359_BUCK_VGPU12_OP_MODE_SET       0x1622
#define MT6359_BUCK_VGPU12_OP_MODE_CLR       0x1624
#define MT6359_BUCK_VGPU12_DBG0              0x1626
#define MT6359_BUCK_VGPU12_DBG1              0x1628
#define MT6359_BUCK_VGPU12_ELR_NUM           0x162a
#define MT6359_BUCK_VGPU12_ELR0              0x162c
#define MT6359_BUCK_VMODEM_DSN_ID            0x1680
#define MT6359_BUCK_VMODEM_DSN_REV0          0x1682
#define MT6359_BUCK_VMODEM_DSN_DBI           0x1684
#define MT6359_BUCK_VMODEM_DSN_DXI           0x1686
#define MT6359_BUCK_VMODEM_CON0              0x1688
#define MT6359_BUCK_VMODEM_CON0_SET          0x168a
#define MT6359_BUCK_VMODEM_CON0_CLR          0x168c
#define MT6359_BUCK_VMODEM_CON1              0x168e
#define MT6359_BUCK_VMODEM_SLP_CON           0x1690
#define MT6359_BUCK_VMODEM_CFG0              0x1692
#define MT6359_BUCK_VMODEM_OP_EN             0x1694
#define MT6359_BUCK_VMODEM_OP_EN_SET         0x1696
#define MT6359_BUCK_VMODEM_OP_EN_CLR         0x1698
#define MT6359_BUCK_VMODEM_OP_CFG            0x169a
#define MT6359_BUCK_VMODEM_OP_CFG_SET        0x169c
#define MT6359_BUCK_VMODEM_OP_CFG_CLR        0x169e
#define MT6359_BUCK_VMODEM_OP_MODE           0x16a0
#define MT6359_BUCK_VMODEM_OP_MODE_SET       0x16a2
#define MT6359_BUCK_VMODEM_OP_MODE_CLR       0x16a4
#define MT6359_BUCK_VMODEM_DBG0              0x16a6
#define MT6359_BUCK_VMODEM_DBG1              0x16a8
#define MT6359_BUCK_VMODEM_STALL_TRACK0      0x16aa
#define MT6359_BUCK_VMODEM_ELR_NUM           0x16ac
#define MT6359_BUCK_VMODEM_ELR0              0x16ae
#define MT6359_BUCK_VPROC1_DSN_ID            0x1700
#define MT6359_BUCK_VPROC1_DSN_REV0          0x1702
#define MT6359_BUCK_VPROC1_DSN_DBI           0x1704
#define MT6359_BUCK_VPROC1_DSN_DXI           0x1706
#define MT6359_BUCK_VPROC1_CON0              0x1708
#define MT6359_BUCK_VPROC1_CON0_SET          0x170a
#define MT6359_BUCK_VPROC1_CON0_CLR          0x170c
#define MT6359_BUCK_VPROC1_CON1              0x170e
#define MT6359_BUCK_VPROC1_SLP_CON           0x1710
#define MT6359_BUCK_VPROC1_CFG0              0x1712
#define MT6359_BUCK_VPROC1_OP_EN             0x1714
#define MT6359_BUCK_VPROC1_OP_EN_SET         0x1716
#define MT6359_BUCK_VPROC1_OP_EN_CLR         0x1718
#define MT6359_BUCK_VPROC1_OP_CFG            0x171a
#define MT6359_BUCK_VPROC1_OP_CFG_SET        0x171c
#define MT6359_BUCK_VPROC1_OP_CFG_CLR        0x171e
#define MT6359_BUCK_VPROC1_OP_MODE           0x1720
#define MT6359_BUCK_VPROC1_OP_MODE_SET       0x1722
#define MT6359_BUCK_VPROC1_OP_MODE_CLR       0x1724
#define MT6359_BUCK_VPROC1_DBG0              0x1726
#define MT6359_BUCK_VPROC1_DBG1              0x1728
#define MT6359_BUCK_VPROC1_STALL_TRACK0      0x172a
#define MT6359_BUCK_VPROC1_ELR_NUM           0x172c
#define MT6359_BUCK_VPROC1_ELR0              0x172e
#define MT6359_BUCK_VPROC2_DSN_ID            0x1780
#define MT6359_BUCK_VPROC2_DSN_REV0          0x1782
#define MT6359_BUCK_VPROC2_DSN_DBI           0x1784
#define MT6359_BUCK_VPROC2_DSN_DXI           0x1786
#define MT6359_BUCK_VPROC2_CON0              0x1788
#define MT6359_BUCK_VPROC2_CON0_SET          0x178a
#define MT6359_BUCK_VPROC2_CON0_CLR          0x178c
#define MT6359_BUCK_VPROC2_CON1              0x178e
#define MT6359_BUCK_VPROC2_SLP_CON           0x1790
#define MT6359_BUCK_VPROC2_CFG0              0x1792
#define MT6359_BUCK_VPROC2_OP_EN             0x1794
#define MT6359_BUCK_VPROC2_OP_EN_SET         0x1796
#define MT6359_BUCK_VPROC2_OP_EN_CLR         0x1798
#define MT6359_BUCK_VPROC2_OP_CFG            0x179a
#define MT6359_BUCK_VPROC2_OP_CFG_SET        0x179c
#define MT6359_BUCK_VPROC2_OP_CFG_CLR        0x179e
#define MT6359_BUCK_VPROC2_OP_MODE           0x17a0
#define MT6359_BUCK_VPROC2_OP_MODE_SET       0x17a2
#define MT6359_BUCK_VPROC2_OP_MODE_CLR       0x17a4
#define MT6359_BUCK_VPROC2_DBG0              0x17a6
#define MT6359_BUCK_VPROC2_DBG1              0x17a8
#define MT6359_BUCK_VPROC2_TRACK0            0x17aa
#define MT6359_BUCK_VPROC2_TRACK1            0x17ac
#define MT6359_BUCK_VPROC2_STALL_TRACK0      0x17ae
#define MT6359_BUCK_VPROC2_ELR_NUM           0x17b0
#define MT6359_BUCK_VPROC2_ELR0              0x17b2
#define MT6359_BUCK_VS1_DSN_ID               0x1800
#define MT6359_BUCK_VS1_DSN_REV0             0x1802
#define MT6359_BUCK_VS1_DSN_DBI              0x1804
#define MT6359_BUCK_VS1_DSN_DXI              0x1806
#define MT6359_BUCK_VS1_CON0                 0x1808
#define MT6359_BUCK_VS1_CON0_SET             0x180a
#define MT6359_BUCK_VS1_CON0_CLR             0x180c
#define MT6359_BUCK_VS1_CON1                 0x180e
#define MT6359_BUCK_VS1_SLP_CON              0x1810
#define MT6359_BUCK_VS1_CFG0                 0x1812
#define MT6359_BUCK_VS1_OP_EN                0x1814
#define MT6359_BUCK_VS1_OP_EN_SET            0x1816
#define MT6359_BUCK_VS1_OP_EN_CLR            0x1818
#define MT6359_BUCK_VS1_OP_CFG               0x181a
#define MT6359_BUCK_VS1_OP_CFG_SET           0x181c
#define MT6359_BUCK_VS1_OP_CFG_CLR           0x181e
#define MT6359_BUCK_VS1_OP_MODE              0x1820
#define MT6359_BUCK_VS1_OP_MODE_SET          0x1822
#define MT6359_BUCK_VS1_OP_MODE_CLR          0x1824
#define MT6359_BUCK_VS1_DBG0                 0x1826
#define MT6359_BUCK_VS1_DBG1                 0x1828
#define MT6359_BUCK_VS1_VOTER                0x182a
#define MT6359_BUCK_VS1_VOTER_SET            0x182c
#define MT6359_BUCK_VS1_VOTER_CLR            0x182e
#define MT6359_BUCK_VS1_VOTER_CFG            0x1830
#define MT6359_BUCK_VS1_ELR_NUM              0x1832
#define MT6359_BUCK_VS1_ELR0                 0x1834
#define MT6359_BUCK_VS2_DSN_ID               0x1880
#define MT6359_BUCK_VS2_DSN_REV0             0x1882
#define MT6359_BUCK_VS2_DSN_DBI              0x1884
#define MT6359_BUCK_VS2_DSN_DXI              0x1886
#define MT6359_BUCK_VS2_CON0                 0x1888
#define MT6359_BUCK_VS2_CON0_SET             0x188a
#define MT6359_BUCK_VS2_CON0_CLR             0x188c
#define MT6359_BUCK_VS2_CON1                 0x188e
#define MT6359_BUCK_VS2_SLP_CON              0x1890
#define MT6359_BUCK_VS2_CFG0                 0x1892
#define MT6359_BUCK_VS2_OP_EN                0x1894
#define MT6359_BUCK_VS2_OP_EN_SET            0x1896
#define MT6359_BUCK_VS2_OP_EN_CLR            0x1898
#define MT6359_BUCK_VS2_OP_CFG               0x189a
#define MT6359_BUCK_VS2_OP_CFG_SET           0x189c
#define MT6359_BUCK_VS2_OP_CFG_CLR           0x189e
#define MT6359_BUCK_VS2_OP_MODE              0x18a0
#define MT6359_BUCK_VS2_OP_MODE_SET          0x18a2
#define MT6359_BUCK_VS2_OP_MODE_CLR          0x18a4
#define MT6359_BUCK_VS2_DBG0                 0x18a6
#define MT6359_BUCK_VS2_DBG1                 0x18a8
#define MT6359_BUCK_VS2_VOTER                0x18aa
#define MT6359_BUCK_VS2_VOTER_SET            0x18ac
#define MT6359_BUCK_VS2_VOTER_CLR            0x18ae
#define MT6359_BUCK_VS2_VOTER_CFG            0x18b0
#define MT6359_BUCK_VS2_ELR_NUM              0x18b2
#define MT6359_BUCK_VS2_ELR0                 0x18b4
#define MT6359_BUCK_VPA_DSN_ID               0x1900
#define MT6359_BUCK_VPA_DSN_REV0             0x1902
#define MT6359_BUCK_VPA_DSN_DBI              0x1904
#define MT6359_BUCK_VPA_DSN_DXI              0x1906
#define MT6359_BUCK_VPA_CON0                 0x1908
#define MT6359_BUCK_VPA_CON0_SET             0x190a
#define MT6359_BUCK_VPA_CON0_CLR             0x190c
#define MT6359_BUCK_VPA_CON1                 0x190e
#define MT6359_BUCK_VPA_CFG0                 0x1910
#define MT6359_BUCK_VPA_CFG1                 0x1912
#define MT6359_BUCK_VPA_DBG0                 0x1914
#define MT6359_BUCK_VPA_DBG1                 0x1916
#define MT6359_BUCK_VPA_DLC_CON0             0x1918
#define MT6359_BUCK_VPA_DLC_CON1             0x191a
#define MT6359_BUCK_VPA_DLC_CON2             0x191c
#define MT6359_BUCK_VPA_MSFG_CON0            0x191e
#define MT6359_BUCK_VPA_MSFG_CON1            0x1920
#define MT6359_BUCK_VPA_MSFG_RRATE0          0x1922
#define MT6359_BUCK_VPA_MSFG_RRATE1          0x1924
#define MT6359_BUCK_VPA_MSFG_RRATE2          0x1926
#define MT6359_BUCK_VPA_MSFG_RTHD0           0x1928
#define MT6359_BUCK_VPA_MSFG_RTHD1           0x192a
#define MT6359_BUCK_VPA_MSFG_RTHD2           0x192c
#define MT6359_BUCK_VPA_MSFG_FRATE0          0x192e
#define MT6359_BUCK_VPA_MSFG_FRATE1          0x1930
#define MT6359_BUCK_VPA_MSFG_FRATE2          0x1932
#define MT6359_BUCK_VPA_MSFG_FTHD0           0x1934
#define MT6359_BUCK_VPA_MSFG_FTHD1           0x1936
#define MT6359_BUCK_VPA_MSFG_FTHD2           0x1938
#define MT6359_BUCK_ANA0_DSN_ID              0x1980
#define MT6359_BUCK_ANA0_DSN_REV0            0x1982
#define MT6359_BUCK_ANA0_DSN_DBI             0x1984
#define MT6359_BUCK_ANA0_DSN_FPI             0x1986
#define MT6359_SMPS_ANA_CON0                 0x1988
#define MT6359_VGPUVCORE_ANA_CON0            0x198a
#define MT6359_VGPUVCORE_ANA_CON1            0x198c
#define MT6359_VGPUVCORE_ANA_CON2            0x198e
#define MT6359_VGPUVCORE_ANA_CON3            0x1990
#define MT6359_VGPUVCORE_ANA_CON4            0x1992
#define MT6359_VGPUVCORE_ANA_CON5            0x1994
#define MT6359_VGPUVCORE_ANA_CON6            0x1996
#define MT6359_VGPUVCORE_ANA_CON7            0x1998
#define MT6359_VGPUVCORE_ANA_CON8            0x199a
#define MT6359_VGPUVCORE_ANA_CON9            0x199c
#define MT6359_VGPUVCORE_ANA_CON10           0x199e
#define MT6359_VGPUVCORE_ANA_CON11           0x19a0
#define MT6359_VGPUVCORE_ANA_CON12           0x19a2
#define MT6359_VGPUVCORE_ANA_CON13           0x19a4
#define MT6359_VGPUVCORE_ANA_CON14           0x19a6
#define MT6359_VGPUVCORE_ANA_CON15           0x19a8
#define MT6359_VGPUVCORE_ANA_CON16           0x19aa
#define MT6359_VPROC1_ANA_CON0               0x19ac
#define MT6359_VPROC1_ANA_CON1               0x19ae
#define MT6359_VPROC1_ANA_CON2               0x19b0
#define MT6359_VPROC1_ANA_CON3               0x19b2
#define MT6359_VPROC1_ANA_CON4               0x19b4
#define MT6359_VPROC1_ANA_CON5               0x19b6
#define MT6359_BUCK_ANA0_ELR_NUM             0x19b8
#define MT6359_SMPS_ELR_0                    0x19ba
#define MT6359_SMPS_ELR_1                    0x19bc
#define MT6359_SMPS_ELR_2                    0x19be
#define MT6359_SMPS_ELR_3                    0x19c0
#define MT6359_SMPS_ELR_4                    0x19c2
#define MT6359_SMPS_ELR_5                    0x19c4
#define MT6359_SMPS_ELR_6                    0x19c6
#define MT6359_SMPS_ELR_7                    0x19c8
#define MT6359_SMPS_ELR_8                    0x19ca
#define MT6359_SMPS_ELR_9                    0x19cc
#define MT6359_SMPS_ELR_10                   0x19ce
#define MT6359_SMPS_ELR_11                   0x19d0
#define MT6359_SMPS_ELR_12                   0x19d2
#define MT6359_SMPS_ELR_13                   0x19d4
#define MT6359_SMPS_ELR_14                   0x19d6
#define MT6359_SMPS_ELR_15                   0x19d8
#define MT6359_SMPS_ELR_16                   0x19da
#define MT6359_SMPS_ELR_17                   0x19dc
#define MT6359_SMPS_ELR_18                   0x19de
#define MT6359_BUCK_ANA1_DSN_ID              0x1a00
#define MT6359_BUCK_ANA1_DSN_REV0            0x1a02
#define MT6359_BUCK_ANA1_DSN_DBI             0x1a04
#define MT6359_BUCK_ANA1_DSN_FPI             0x1a06
#define MT6359_VPROC2_ANA_CON0               0x1a08
#define MT6359_VPROC2_ANA_CON1               0x1a0a
#define MT6359_VPROC2_ANA_CON2               0x1a0c
#define MT6359_VPROC2_ANA_CON3               0x1a0e
#define MT6359_VPROC2_ANA_CON4               0x1a10
#define MT6359_VPROC2_ANA_CON5               0x1a12
#define MT6359_VMODEM_ANA_CON0               0x1a14
#define MT6359_VMODEM_ANA_CON1               0x1a16
#define MT6359_VMODEM_ANA_CON2               0x1a18
#define MT6359_VMODEM_ANA_CON3               0x1a1a
#define MT6359_VMODEM_ANA_CON4               0x1a1c
#define MT6359_VMODEM_ANA_CON5               0x1a1e
#define MT6359_VPU_ANA_CON0                  0x1a20
#define MT6359_VPU_ANA_CON1                  0x1a22
#define MT6359_VPU_ANA_CON2                  0x1a24
#define MT6359_VPU_ANA_CON3                  0x1a26
#define MT6359_VPU_ANA_CON4                  0x1a28
#define MT6359_VPU_ANA_CON5                  0x1a2a
#define MT6359_VS1_ANA_CON0                  0x1a2c
#define MT6359_VS1_ANA_CON1                  0x1a2e
#define MT6359_VS1_ANA_CON2                  0x1a30
#define MT6359_VS1_ANA_CON3                  0x1a32
#define MT6359_VS2_ANA_CON0                  0x1a34
#define MT6359_VS2_ANA_CON1                  0x1a36
#define MT6359_VS2_ANA_CON2                  0x1a38
#define MT6359_VS2_ANA_CON3                  0x1a3a
#define MT6359_VPA_ANA_CON0                  0x1a3c
#define MT6359_VPA_ANA_CON1                  0x1a3e
#define MT6359_VPA_ANA_CON2                  0x1a40
#define MT6359_VPA_ANA_CON3                  0x1a42
#define MT6359_VPA_ANA_CON4                  0x1a44
#define MT6359_BUCK_ANA1_ELR_NUM             0x1a46
#define MT6359_VPROC2_ELR_0                  0x1a48
#define MT6359_VPROC2_ELR_1                  0x1a4a
#define MT6359_VPROC2_ELR_2                  0x1a4c
#define MT6359_VPROC2_ELR_3                  0x1a4e
#define MT6359_VPROC2_ELR_4                  0x1a50
#define MT6359_VPROC2_ELR_5                  0x1a52
#define MT6359_VPROC2_ELR_6                  0x1a54
#define MT6359_VPROC2_ELR_7                  0x1a56
#define MT6359_VPROC2_ELR_8                  0x1a58
#define MT6359_VPROC2_ELR_9                  0x1a5a
#define MT6359_VPROC2_ELR_10                 0x1a5c
#define MT6359_VPROC2_ELR_11                 0x1a5e
#define MT6359_VPROC2_ELR_12                 0x1a60
#define MT6359_VPROC2_ELR_13                 0x1a62
#define MT6359_VPROC2_ELR_14                 0x1a64
#define MT6359_VPROC2_ELR_15                 0x1a66
#define MT6359_LDO_TOP_ID                    0x1b00
#define MT6359_LDO_TOP_REV0                  0x1b02
#define MT6359_LDO_TOP_DBI                   0x1b04
#define MT6359_LDO_TOP_DXI                   0x1b06
#define MT6359_LDO_TPM0                      0x1b08
#define MT6359_LDO_TPM1                      0x1b0a
#define MT6359_LDO_TOP_CKPDN_CON0            0x1b0c
#define MT6359_TOP_TOP_CKHWEN_CON0           0x1b0e
#define MT6359_LDO_TOP_CLK_DCM_CON0          0x1b10
#define MT6359_LDO_TOP_CLK_VSRAM_CON0        0x1b12
#define MT6359_LDO_TOP_INT_CON0              0x1b14
#define MT6359_LDO_TOP_INT_CON0_SET          0x1b16
#define MT6359_LDO_TOP_INT_CON0_CLR          0x1b18
#define MT6359_LDO_TOP_INT_CON1              0x1b1a
#define MT6359_LDO_TOP_INT_MASK_CON0         0x1b1c
#define MT6359_LDO_TOP_INT_MASK_CON0_SET     0x1b1e
#define MT6359_LDO_TOP_INT_MASK_CON0_CLR     0x1b20
#define MT6359_LDO_TOP_INT_MASK_CON1         0x1b22
#define MT6359_LDO_TOP_INT_MASK_CON1_SET     0x1b24
#define MT6359_LDO_TOP_INT_MASK_CON1_CLR     0x1b26
#define MT6359_LDO_TOP_INT_STATUS0           0x1b28
#define MT6359_LDO_TOP_INT_STATUS1           0x1b2a
#define MT6359_LDO_TOP_INT_RAW_STATUS0       0x1b2c
#define MT6359_LDO_TOP_INT_RAW_STATUS1       0x1b2e
#define MT6359_LDO_TEST_CON0                 0x1b30
#define MT6359_LDO_TOP_CON                   0x1b32
#define MT6359_VRTC28_CON                    0x1b34
#define MT6359_VAUX18_ACK                    0x1b36
#define MT6359_VBIF28_ACK                    0x1b38
#define MT6359_VOW_DVS_CON                   0x1b3a
#define MT6359_VXO22_CON                     0x1b3c
#define MT6359_LDO_TOP_ELR_NUM               0x1b3e
#define MT6359_LDO_VSRAM_PROC1_ELR           0x1b40
#define MT6359_LDO_VSRAM_PROC2_ELR           0x1b42
#define MT6359_LDO_VSRAM_OTHERS_ELR          0x1b44
#define MT6359_LDO_VSRAM_MD_ELR              0x1b46
#define MT6359_LDO_GNR0_DSN_ID               0x1b80
#define MT6359_LDO_GNR0_DSN_REV0             0x1b82
#define MT6359_LDO_GNR0_DSN_DBI              0x1b84
#define MT6359_LDO_GNR0_DSN_DXI              0x1b86
#define MT6359_LDO_VFE28_CON0                0x1b88
#define MT6359_LDO_VFE28_MON                 0x1b8a
#define MT6359_LDO_VFE28_OP_EN               0x1b8c
#define MT6359_LDO_VFE28_OP_EN_SET           0x1b8e
#define MT6359_LDO_VFE28_OP_EN_CLR           0x1b90
#define MT6359_LDO_VFE28_OP_CFG              0x1b92
#define MT6359_LDO_VFE28_OP_CFG_SET          0x1b94
#define MT6359_LDO_VFE28_OP_CFG_CLR          0x1b96
#define MT6359_LDO_VXO22_CON0                0x1b98
#define MT6359_LDO_VXO22_MON                 0x1b9a
#define MT6359_LDO_VXO22_OP_EN               0x1b9c
#define MT6359_LDO_VXO22_OP_EN_SET           0x1b9e
#define MT6359_LDO_VXO22_OP_EN_CLR           0x1ba0
#define MT6359_LDO_VXO22_OP_CFG              0x1ba2
#define MT6359_LDO_VXO22_OP_CFG_SET          0x1ba4
#define MT6359_LDO_VXO22_OP_CFG_CLR          0x1ba6
#define MT6359_LDO_VRF18_CON0                0x1ba8
#define MT6359_LDO_VRF18_MON                 0x1baa
#define MT6359_LDO_VRF18_OP_EN               0x1bac
#define MT6359_LDO_VRF18_OP_EN_SET           0x1bae
#define MT6359_LDO_VRF18_OP_EN_CLR           0x1bb0
#define MT6359_LDO_VRF18_OP_CFG              0x1bb2
#define MT6359_LDO_VRF18_OP_CFG_SET          0x1bb4
#define MT6359_LDO_VRF18_OP_CFG_CLR          0x1bb6
#define MT6359_LDO_VRF12_CON0                0x1bb8
#define MT6359_LDO_VRF12_MON                 0x1bba
#define MT6359_LDO_VRF12_OP_EN               0x1bbc
#define MT6359_LDO_VRF12_OP_EN_SET           0x1bbe
#define MT6359_LDO_VRF12_OP_EN_CLR           0x1bc0
#define MT6359_LDO_VRF12_OP_CFG              0x1bc2
#define MT6359_LDO_VRF12_OP_CFG_SET          0x1bc4
#define MT6359_LDO_VRF12_OP_CFG_CLR          0x1bc6
#define MT6359_LDO_VEFUSE_CON0               0x1bc8
#define MT6359_LDO_VEFUSE_MON                0x1bca
#define MT6359_LDO_VEFUSE_OP_EN              0x1bcc
#define MT6359_LDO_VEFUSE_OP_EN_SET          0x1bce
#define MT6359_LDO_VEFUSE_OP_EN_CLR          0x1bd0
#define MT6359_LDO_VEFUSE_OP_CFG             0x1bd2
#define MT6359_LDO_VEFUSE_OP_CFG_SET         0x1bd4
#define MT6359_LDO_VEFUSE_OP_CFG_CLR         0x1bd6
#define MT6359_LDO_VCN33_1_CON0              0x1bd8
#define MT6359_LDO_VCN33_1_MON               0x1bda
#define MT6359_LDO_VCN33_1_OP_EN             0x1bdc
#define MT6359_LDO_VCN33_1_OP_EN_SET         0x1bde
#define MT6359_LDO_VCN33_1_OP_EN_CLR         0x1be0
#define MT6359_LDO_VCN33_1_OP_CFG            0x1be2
#define MT6359_LDO_VCN33_1_OP_CFG_SET        0x1be4
#define MT6359_LDO_VCN33_1_OP_CFG_CLR        0x1be6
#define MT6359_LDO_VCN33_1_MULTI_SW          0x1be8
#define MT6359_LDO_GNR1_DSN_ID               0x1c00
#define MT6359_LDO_GNR1_DSN_REV0             0x1c02
#define MT6359_LDO_GNR1_DSN_DBI              0x1c04
#define MT6359_LDO_GNR1_DSN_DXI              0x1c06
#define MT6359_LDO_VCN33_2_CON0              0x1c08
#define MT6359_LDO_VCN33_2_MON               0x1c0a
#define MT6359_LDO_VCN33_2_OP_EN             0x1c0c
#define MT6359_LDO_VCN33_2_OP_EN_SET         0x1c0e
#define MT6359_LDO_VCN33_2_OP_EN_CLR         0x1c10
#define MT6359_LDO_VCN33_2_OP_CFG            0x1c12
#define MT6359_LDO_VCN33_2_OP_CFG_SET        0x1c14
#define MT6359_LDO_VCN33_2_OP_CFG_CLR        0x1c16
#define MT6359_LDO_VCN33_2_MULTI_SW          0x1c18
#define MT6359_LDO_VCN13_CON0                0x1c1a
#define MT6359_LDO_VCN13_MON                 0x1c1c
#define MT6359_LDO_VCN13_OP_EN               0x1c1e
#define MT6359_LDO_VCN13_OP_EN_SET           0x1c20
#define MT6359_LDO_VCN13_OP_EN_CLR           0x1c22
#define MT6359_LDO_VCN13_OP_CFG              0x1c24
#define MT6359_LDO_VCN13_OP_CFG_SET          0x1c26
#define MT6359_LDO_VCN13_OP_CFG_CLR          0x1c28
#define MT6359_LDO_VCN18_CON0                0x1c2a
#define MT6359_LDO_VCN18_MON                 0x1c2c
#define MT6359_LDO_VCN18_OP_EN               0x1c2e
#define MT6359_LDO_VCN18_OP_EN_SET           0x1c30
#define MT6359_LDO_VCN18_OP_EN_CLR           0x1c32
#define MT6359_LDO_VCN18_OP_CFG              0x1c34
#define MT6359_LDO_VCN18_OP_CFG_SET          0x1c36
#define MT6359_LDO_VCN18_OP_CFG_CLR          0x1c38
#define MT6359_LDO_VA09_CON0                 0x1c3a
#define MT6359_LDO_VA09_MON                  0x1c3c
#define MT6359_LDO_VA09_OP_EN                0x1c3e
#define MT6359_LDO_VA09_OP_EN_SET            0x1c40
#define MT6359_LDO_VA09_OP_EN_CLR            0x1c42
#define MT6359_LDO_VA09_OP_CFG               0x1c44
#define MT6359_LDO_VA09_OP_CFG_SET           0x1c46
#define MT6359_LDO_VA09_OP_CFG_CLR           0x1c48
#define MT6359_LDO_VCAMIO_CON0               0x1c4a
#define MT6359_LDO_VCAMIO_MON                0x1c4c
#define MT6359_LDO_VCAMIO_OP_EN              0x1c4e
#define MT6359_LDO_VCAMIO_OP_EN_SET          0x1c50
#define MT6359_LDO_VCAMIO_OP_EN_CLR          0x1c52
#define MT6359_LDO_VCAMIO_OP_CFG             0x1c54
#define MT6359_LDO_VCAMIO_OP_CFG_SET         0x1c56
#define MT6359_LDO_VCAMIO_OP_CFG_CLR         0x1c58
#define MT6359_LDO_VA12_CON0                 0x1c5a
#define MT6359_LDO_VA12_MON                  0x1c5c
#define MT6359_LDO_VA12_OP_EN                0x1c5e
#define MT6359_LDO_VA12_OP_EN_SET            0x1c60
#define MT6359_LDO_VA12_OP_EN_CLR            0x1c62
#define MT6359_LDO_VA12_OP_CFG               0x1c64
#define MT6359_LDO_VA12_OP_CFG_SET           0x1c66
#define MT6359_LDO_VA12_OP_CFG_CLR           0x1c68
#define MT6359_LDO_GNR2_DSN_ID               0x1c80
#define MT6359_LDO_GNR2_DSN_REV0             0x1c82
#define MT6359_LDO_GNR2_DSN_DBI              0x1c84
#define MT6359_LDO_GNR2_DSN_DXI              0x1c86
#define MT6359_LDO_VAUX18_CON0               0x1c88
#define MT6359_LDO_VAUX18_MON                0x1c8a
#define MT6359_LDO_VAUX18_OP_EN              0x1c8c
#define MT6359_LDO_VAUX18_OP_EN_SET          0x1c8e
#define MT6359_LDO_VAUX18_OP_EN_CLR          0x1c90
#define MT6359_LDO_VAUX18_OP_CFG             0x1c92
#define MT6359_LDO_VAUX18_OP_CFG_SET         0x1c94
#define MT6359_LDO_VAUX18_OP_CFG_CLR         0x1c96
#define MT6359_LDO_VAUD18_CON0               0x1c98
#define MT6359_LDO_VAUD18_MON                0x1c9a
#define MT6359_LDO_VAUD18_OP_EN              0x1c9c
#define MT6359_LDO_VAUD18_OP_EN_SET          0x1c9e
#define MT6359_LDO_VAUD18_OP_EN_CLR          0x1ca0
#define MT6359_LDO_VAUD18_OP_CFG             0x1ca2
#define MT6359_LDO_VAUD18_OP_CFG_SET         0x1ca4
#define MT6359_LDO_VAUD18_OP_CFG_CLR         0x1ca6
#define MT6359_LDO_VIO18_CON0                0x1ca8
#define MT6359_LDO_VIO18_MON                 0x1caa
#define MT6359_LDO_VIO18_OP_EN               0x1cac
#define MT6359_LDO_VIO18_OP_EN_SET           0x1cae
#define MT6359_LDO_VIO18_OP_EN_CLR           0x1cb0
#define MT6359_LDO_VIO18_OP_CFG              0x1cb2
#define MT6359_LDO_VIO18_OP_CFG_SET          0x1cb4
#define MT6359_LDO_VIO18_OP_CFG_CLR          0x1cb6
#define MT6359_LDO_VEMC_CON0                 0x1cb8
#define MT6359_LDO_VEMC_MON                  0x1cba
#define MT6359_LDO_VEMC_OP_EN                0x1cbc
#define MT6359_LDO_VEMC_OP_EN_SET            0x1cbe
#define MT6359_LDO_VEMC_OP_EN_CLR            0x1cc0
#define MT6359_LDO_VEMC_OP_CFG               0x1cc2
#define MT6359_LDO_VEMC_OP_CFG_SET           0x1cc4
#define MT6359_LDO_VEMC_OP_CFG_CLR           0x1cc6
#define MT6359_LDO_VSIM1_CON0                0x1cc8
#define MT6359_LDO_VSIM1_MON                 0x1cca
#define MT6359_LDO_VSIM1_OP_EN               0x1ccc
#define MT6359_LDO_VSIM1_OP_EN_SET           0x1cce
#define MT6359_LDO_VSIM1_OP_EN_CLR           0x1cd0
#define MT6359_LDO_VSIM1_OP_CFG              0x1cd2
#define MT6359_LDO_VSIM1_OP_CFG_SET          0x1cd4
#define MT6359_LDO_VSIM1_OP_CFG_CLR          0x1cd6
#define MT6359_LDO_VSIM2_CON0                0x1cd8
#define MT6359_LDO_VSIM2_MON                 0x1cda
#define MT6359_LDO_VSIM2_OP_EN               0x1cdc
#define MT6359_LDO_VSIM2_OP_EN_SET           0x1cde
#define MT6359_LDO_VSIM2_OP_EN_CLR           0x1ce0
#define MT6359_LDO_VSIM2_OP_CFG              0x1ce2
#define MT6359_LDO_VSIM2_OP_CFG_SET          0x1ce4
#define MT6359_LDO_VSIM2_OP_CFG_CLR          0x1ce6
#define MT6359_LDO_GNR3_DSN_ID               0x1d00
#define MT6359_LDO_GNR3_DSN_REV0             0x1d02
#define MT6359_LDO_GNR3_DSN_DBI              0x1d04
#define MT6359_LDO_GNR3_DSN_DXI              0x1d06
#define MT6359_LDO_VUSB_CON0                 0x1d08
#define MT6359_LDO_VUSB_MON                  0x1d0a
#define MT6359_LDO_VUSB_OP_EN                0x1d0c
#define MT6359_LDO_VUSB_OP_EN_SET            0x1d0e
#define MT6359_LDO_VUSB_OP_EN_CLR            0x1d10
#define MT6359_LDO_VUSB_OP_CFG               0x1d12
#define MT6359_LDO_VUSB_OP_CFG_SET           0x1d14
#define MT6359_LDO_VUSB_OP_CFG_CLR           0x1d16
#define MT6359_LDO_VUSB_MULTI_SW             0x1d18
#define MT6359_LDO_VRFCK_CON0                0x1d1a
#define MT6359_LDO_VRFCK_MON                 0x1d1c
#define MT6359_LDO_VRFCK_OP_EN               0x1d1e
#define MT6359_LDO_VRFCK_OP_EN_SET           0x1d20
#define MT6359_LDO_VRFCK_OP_EN_CLR           0x1d22
#define MT6359_LDO_VRFCK_OP_CFG              0x1d24
#define MT6359_LDO_VRFCK_OP_CFG_SET          0x1d26
#define MT6359_LDO_VRFCK_OP_CFG_CLR          0x1d28
#define MT6359_LDO_VBBCK_CON0                0x1d2a
#define MT6359_LDO_VBBCK_MON                 0x1d2c
#define MT6359_LDO_VBBCK_OP_EN               0x1d2e
#define MT6359_LDO_VBBCK_OP_EN_SET           0x1d30
#define MT6359_LDO_VBBCK_OP_EN_CLR           0x1d32
#define MT6359_LDO_VBBCK_OP_CFG              0x1d34
#define MT6359_LDO_VBBCK_OP_CFG_SET          0x1d36
#define MT6359_LDO_VBBCK_OP_CFG_CLR          0x1d38
#define MT6359_LDO_VBIF28_CON0               0x1d3a
#define MT6359_LDO_VBIF28_MON                0x1d3c
#define MT6359_LDO_VBIF28_OP_EN              0x1d3e
#define MT6359_LDO_VBIF28_OP_EN_SET          0x1d40
#define MT6359_LDO_VBIF28_OP_EN_CLR          0x1d42
#define MT6359_LDO_VBIF28_OP_CFG             0x1d44
#define MT6359_LDO_VBIF28_OP_CFG_SET         0x1d46
#define MT6359_LDO_VBIF28_OP_CFG_CLR         0x1d48
#define MT6359_LDO_VIBR_CON0                 0x1d4a
#define MT6359_LDO_VIBR_MON                  0x1d4c
#define MT6359_LDO_VIBR_OP_EN                0x1d4e
#define MT6359_LDO_VIBR_OP_EN_SET            0x1d50
#define MT6359_LDO_VIBR_OP_EN_CLR            0x1d52
#define MT6359_LDO_VIBR_OP_CFG               0x1d54
#define MT6359_LDO_VIBR_OP_CFG_SET           0x1d56
#define MT6359_LDO_VIBR_OP_CFG_CLR           0x1d58
#define MT6359_LDO_VIO28_CON0                0x1d5a
#define MT6359_LDO_VIO28_MON                 0x1d5c
#define MT6359_LDO_VIO28_OP_EN               0x1d5e
#define MT6359_LDO_VIO28_OP_EN_SET           0x1d60
#define MT6359_LDO_VIO28_OP_EN_CLR           0x1d62
#define MT6359_LDO_VIO28_OP_CFG              0x1d64
#define MT6359_LDO_VIO28_OP_CFG_SET          0x1d66
#define MT6359_LDO_VIO28_OP_CFG_CLR          0x1d68
#define MT6359_LDO_GNR4_DSN_ID               0x1d80
#define MT6359_LDO_GNR4_DSN_REV0             0x1d82
#define MT6359_LDO_GNR4_DSN_DBI              0x1d84
#define MT6359_LDO_GNR4_DSN_DXI              0x1d86
#define MT6359_LDO_VM18_CON0                 0x1d88
#define MT6359_LDO_VM18_MON                  0x1d8a
#define MT6359_LDO_VM18_OP_EN                0x1d8c
#define MT6359_LDO_VM18_OP_EN_SET            0x1d8e
#define MT6359_LDO_VM18_OP_EN_CLR            0x1d90
#define MT6359_LDO_VM18_OP_CFG               0x1d92
#define MT6359_LDO_VM18_OP_CFG_SET           0x1d94
#define MT6359_LDO_VM18_OP_CFG_CLR           0x1d96
#define MT6359_LDO_VUFS_CON0                 0x1d98
#define MT6359_LDO_VUFS_MON                  0x1d9a
#define MT6359_LDO_VUFS_OP_EN                0x1d9c
#define MT6359_LDO_VUFS_OP_EN_SET            0x1d9e
#define MT6359_LDO_VUFS_OP_EN_CLR            0x1da0
#define MT6359_LDO_VUFS_OP_CFG               0x1da2
#define MT6359_LDO_VUFS_OP_CFG_SET           0x1da4
#define MT6359_LDO_VUFS_OP_CFG_CLR           0x1da6
#define MT6359_LDO_GNR5_DSN_ID               0x1e00
#define MT6359_LDO_GNR5_DSN_REV0             0x1e02
#define MT6359_LDO_GNR5_DSN_DBI              0x1e04
#define MT6359_LDO_GNR5_DSN_DXI              0x1e06
#define MT6359_LDO_VSRAM0_DSN_ID             0x1e80
#define MT6359_LDO_VSRAM0_DSN_REV0           0x1e82
#define MT6359_LDO_VSRAM0_DSN_DBI            0x1e84
#define MT6359_LDO_VSRAM0_DSN_DXI            0x1e86
#define MT6359_LDO_VSRAM_PROC1_CON0          0x1e88
#define MT6359_LDO_VSRAM_PROC1_MON           0x1e8a
#define MT6359_LDO_VSRAM_PROC1_VOSEL0        0x1e8c
#define MT6359_LDO_VSRAM_PROC1_VOSEL1        0x1e8e
#define MT6359_LDO_VSRAM_PROC1_SFCHG         0x1e90
#define MT6359_LDO_VSRAM_PROC1_DVS           0x1e92
#define MT6359_LDO_VSRAM_PROC1_OP_EN         0x1e94
#define MT6359_LDO_VSRAM_PROC1_OP_EN_SET     0x1e96
#define MT6359_LDO_VSRAM_PROC1_OP_EN_CLR     0x1e98
#define MT6359_LDO_VSRAM_PROC1_OP_CFG        0x1e9a
#define MT6359_LDO_VSRAM_PROC1_OP_CFG_SET    0x1e9c
#define MT6359_LDO_VSRAM_PROC1_OP_CFG_CLR    0x1e9e
#define MT6359_LDO_VSRAM_PROC1_TRACK0        0x1ea0
#define MT6359_LDO_VSRAM_PROC1_TRACK1        0x1ea2
#define MT6359_LDO_VSRAM_PROC1_TRACK2        0x1ea4
#define MT6359_LDO_VSRAM_PROC2_CON0          0x1ea6
#define MT6359_LDO_VSRAM_PROC2_MON           0x1ea8
#define MT6359_LDO_VSRAM_PROC2_VOSEL0        0x1eaa
#define MT6359_LDO_VSRAM_PROC2_VOSEL1        0x1eac
#define MT6359_LDO_VSRAM_PROC2_SFCHG         0x1eae
#define MT6359_LDO_VSRAM_PROC2_DVS           0x1eb0
#define MT6359_LDO_VSRAM_PROC2_OP_EN         0x1eb2
#define MT6359_LDO_VSRAM_PROC2_OP_EN_SET     0x1eb4
#define MT6359_LDO_VSRAM_PROC2_OP_EN_CLR     0x1eb6
#define MT6359_LDO_VSRAM_PROC2_OP_CFG        0x1eb8
#define MT6359_LDO_VSRAM_PROC2_OP_CFG_SET    0x1eba
#define MT6359_LDO_VSRAM_PROC2_OP_CFG_CLR    0x1ebc
#define MT6359_LDO_VSRAM_PROC2_TRACK0        0x1ebe
#define MT6359_LDO_VSRAM_PROC2_TRACK1        0x1ec0
#define MT6359_LDO_VSRAM_PROC2_TRACK2        0x1ec2
#define MT6359_LDO_VSRAM1_DSN_ID             0x1f00
#define MT6359_LDO_VSRAM1_DSN_REV0           0x1f02
#define MT6359_LDO_VSRAM1_DSN_DBI            0x1f04
#define MT6359_LDO_VSRAM1_DSN_DXI            0x1f06
#define MT6359_LDO_VSRAM_OTHERS_CON0         0x1f08
#define MT6359_LDO_VSRAM_OTHERS_MON          0x1f0a
#define MT6359_LDO_VSRAM_OTHERS_VOSEL0       0x1f0c
#define MT6359_LDO_VSRAM_OTHERS_VOSEL1       0x1f0e
#define MT6359_LDO_VSRAM_OTHERS_SFCHG        0x1f10
#define MT6359_LDO_VSRAM_OTHERS_DVS          0x1f12
#define MT6359_LDO_VSRAM_OTHERS_OP_EN        0x1f14
#define MT6359_LDO_VSRAM_OTHERS_OP_EN_SET    0x1f16
#define MT6359_LDO_VSRAM_OTHERS_OP_EN_CLR    0x1f18
#define MT6359_LDO_VSRAM_OTHERS_OP_CFG       0x1f1a
#define MT6359_LDO_VSRAM_OTHERS_OP_CFG_SET   0x1f1c
#define MT6359_LDO_VSRAM_OTHERS_OP_CFG_CLR   0x1f1e
#define MT6359_LDO_VSRAM_OTHERS_TRACK0       0x1f20
#define MT6359_LDO_VSRAM_OTHERS_TRACK1       0x1f22
#define MT6359_LDO_VSRAM_OTHERS_TRACK2       0x1f24
#define MT6359_LDO_VSRAM_OTHERS_SSHUB        0x1f26
#define MT6359_LDO_VSRAM_OTHERS_BT           0x1f28
#define MT6359_LDO_VSRAM_OTHERS_SPI          0x1f2a
#define MT6359_LDO_VSRAM_MD_CON0             0x1f2c
#define MT6359_LDO_VSRAM_MD_MON              0x1f2e
#define MT6359_LDO_VSRAM_MD_VOSEL0           0x1f30
#define MT6359_LDO_VSRAM_MD_VOSEL1           0x1f32
#define MT6359_LDO_VSRAM_MD_SFCHG            0x1f34
#define MT6359_LDO_VSRAM_MD_DVS              0x1f36
#define MT6359_LDO_VSRAM_MD_OP_EN            0x1f38
#define MT6359_LDO_VSRAM_MD_OP_EN_SET        0x1f3a
#define MT6359_LDO_VSRAM_MD_OP_EN_CLR        0x1f3c
#define MT6359_LDO_VSRAM_MD_OP_CFG           0x1f3e
#define MT6359_LDO_VSRAM_MD_OP_CFG_SET       0x1f40
#define MT6359_LDO_VSRAM_MD_OP_CFG_CLR       0x1f42
#define MT6359_LDO_VSRAM_MD_TRACK0           0x1f44
#define MT6359_LDO_VSRAM_MD_TRACK1           0x1f46
#define MT6359_LDO_VSRAM_MD_TRACK2           0x1f48
#define MT6359_LDO_ANA0_DSN_ID               0x1f80
#define MT6359_LDO_ANA0_DSN_REV0             0x1f82
#define MT6359_LDO_ANA0_DSN_DBI              0x1f84
#define MT6359_LDO_ANA0_DSN_FPI              0x1f86
#define MT6359_VFE28_ANA_CON0                0x1f88
#define MT6359_VFE28_ANA_CON1                0x1f8a
#define MT6359_VAUX18_ANA_CON0               0x1f8c
#define MT6359_VAUX18_ANA_CON1               0x1f8e
#define MT6359_VUSB_ANA_CON0                 0x1f90
#define MT6359_VUSB_ANA_CON1                 0x1f92
#define MT6359_VBIF28_ANA_CON0               0x1f94
#define MT6359_VBIF28_ANA_CON1               0x1f96
#define MT6359_VCN33_1_ANA_CON0              0x1f98
#define MT6359_VCN33_1_ANA_CON1              0x1f9a
#define MT6359_VCN33_2_ANA_CON0              0x1f9c
#define MT6359_VCN33_2_ANA_CON1              0x1f9e
#define MT6359_VEMC_ANA_CON0                 0x1fa0
#define MT6359_VEMC_ANA_CON1                 0x1fa2
#define MT6359_VSIM1_ANA_CON0                0x1fa4
#define MT6359_VSIM1_ANA_CON1                0x1fa6
#define MT6359_VSIM2_ANA_CON0                0x1fa8
#define MT6359_VSIM2_ANA_CON1                0x1faa
#define MT6359_VIO28_ANA_CON0                0x1fac
#define MT6359_VIO28_ANA_CON1                0x1fae
#define MT6359_VIBR_ANA_CON0                 0x1fb0
#define MT6359_VIBR_ANA_CON1                 0x1fb2
#define MT6359_ADLDO_ANA_CON0                0x1fb4
#define MT6359_LDO_ANA0_ELR_NUM              0x1fb6
#define MT6359_VFE28_ELR_0                   0x1fb8
#define MT6359_VFE28_ELR_1                   0x1fba
#define MT6359_VFE28_ELR_2                   0x1fbc
#define MT6359_VFE28_ELR_3                   0x1fbe
#define MT6359_VFE28_ELR_4                   0x1fc0
#define MT6359_LDO_ANA1_DSN_ID               0x2000
#define MT6359_LDO_ANA1_DSN_REV0             0x2002
#define MT6359_LDO_ANA1_DSN_DBI              0x2004
#define MT6359_LDO_ANA1_DSN_FPI              0x2006
#define MT6359_VRF18_ANA_CON0                0x2008
#define MT6359_VRF18_ANA_CON1                0x200a
#define MT6359_VEFUSE_ANA_CON0               0x200c
#define MT6359_VEFUSE_ANA_CON1               0x200e
#define MT6359_VCN18_ANA_CON0                0x2010
#define MT6359_VCN18_ANA_CON1                0x2012
#define MT6359_VCAMIO_ANA_CON0               0x2014
#define MT6359_VCAMIO_ANA_CON1               0x2016
#define MT6359_VAUD18_ANA_CON0               0x2018
#define MT6359_VAUD18_ANA_CON1               0x201a
#define MT6359_VIO18_ANA_CON0                0x201c
#define MT6359_VIO18_ANA_CON1                0x201e
#define MT6359_VM18_ANA_CON0                 0x2020
#define MT6359_VM18_ANA_CON1                 0x2022
#define MT6359_VUFS_ANA_CON0                 0x2024
#define MT6359_VUFS_ANA_CON1                 0x2026
#define MT6359_SLDO20_ANA_CON0               0x2028
#define MT6359_VRF12_ANA_CON0                0x202a
#define MT6359_VRF12_ANA_CON1                0x202c
#define MT6359_VCN13_ANA_CON0                0x202e
#define MT6359_VCN13_ANA_CON1                0x2030
#define MT6359_VA09_ANA_CON0                 0x2032
#define MT6359_VA09_ANA_CON1                 0x2034
#define MT6359_VA12_ANA_CON0                 0x2036
#define MT6359_VA12_ANA_CON1                 0x2038
#define MT6359_VSRAM_PROC1_ANA_CON0          0x203a
#define MT6359_VSRAM_PROC1_ANA_CON1          0x203c
#define MT6359_VSRAM_PROC2_ANA_CON0          0x203e
#define MT6359_VSRAM_PROC2_ANA_CON1          0x2040
#define MT6359_VSRAM_OTHERS_ANA_CON0         0x2042
#define MT6359_VSRAM_OTHERS_ANA_CON1         0x2044
#define MT6359_VSRAM_MD_ANA_CON0             0x2046
#define MT6359_VSRAM_MD_ANA_CON1             0x2048
#define MT6359_SLDO14_ANA_CON0               0x204a
#define MT6359_LDO_ANA1_ELR_NUM              0x204c
#define MT6359_VRF18_ELR_0                   0x204e
#define MT6359_VRF18_ELR_1                   0x2050
#define MT6359_VRF18_ELR_2                   0x2052
#define MT6359_VRF18_ELR_3                   0x2054
#define MT6359_LDO_ANA2_DSN_ID               0x2080
#define MT6359_LDO_ANA2_DSN_REV0             0x2082
#define MT6359_LDO_ANA2_DSN_DBI              0x2084
#define MT6359_LDO_ANA2_DSN_FPI              0x2086
#define MT6359_VXO22_ANA_CON0                0x2088
#define MT6359_VXO22_ANA_CON1                0x208a
#define MT6359_VRFCK_ANA_CON0                0x208c
#define MT6359_VRFCK_ANA_CON1                0x208e
#define MT6359_VRFCK_1_ANA_CON0              0x2090
#define MT6359_VRFCK_1_ANA_CON1              0x2092
#define MT6359_VBBCK_ANA_CON0                0x2094
#define MT6359_VBBCK_ANA_CON1                0x2096
#define MT6359_LDO_ANA2_ELR_NUM              0x2098
#define MT6359_DCXO_ADLDO_BIAS_ELR_0         0x209a
#define MT6359_DCXO_ADLDO_BIAS_ELR_1         0x209c
#define MT6359_DUMMYLOAD_DSN_ID              0x2100
#define MT6359_DUMMYLOAD_DSN_REV0            0x2102
#define MT6359_DUMMYLOAD_DSN_DBI             0x2104
#define MT6359_DUMMYLOAD_DSN_FPI             0x2106
#define MT6359_DUMMYLOAD_ANA_CON0            0x2108
#define MT6359_ISINK0_CON1                   0x210a
#define MT6359_ISINK1_CON1                   0x210c
#define MT6359_ISINK_ANA1_SMPL               0x210e
#define MT6359_ISINK_EN_CTRL_SMPL            0x2110
#define MT6359_DUMMYLOAD_ELR_NUM             0x2112
#define MT6359_DUMMYLOAD_ELR_0               0x2114
#define MT6359_AUD_TOP_ID                    0x2300
#define MT6359_AUD_TOP_REV0                  0x2302
#define MT6359_AUD_TOP_DBI                   0x2304
#define MT6359_AUD_TOP_DXI                   0x2306
#define MT6359_AUD_TOP_CKPDN_TPM0            0x2308
#define MT6359_AUD_TOP_CKPDN_TPM1            0x230a
#define MT6359_AUD_TOP_CKPDN_CON0            0x230c
#define MT6359_AUD_TOP_CKPDN_CON0_SET        0x230e
#define MT6359_AUD_TOP_CKPDN_CON0_CLR        0x2310
#define MT6359_AUD_TOP_CKSEL_CON0            0x2312
#define MT6359_AUD_TOP_CKSEL_CON0_SET        0x2314
#define MT6359_AUD_TOP_CKSEL_CON0_CLR        0x2316
#define MT6359_AUD_TOP_CKTST_CON0            0x2318
#define MT6359_AUD_TOP_CLK_HWEN_CON0         0x231a
#define MT6359_AUD_TOP_CLK_HWEN_CON0_SET     0x231c
#define MT6359_AUD_TOP_CLK_HWEN_CON0_CLR     0x231e
#define MT6359_AUD_TOP_RST_CON0              0x2320
#define MT6359_AUD_TOP_RST_CON0_SET          0x2322
#define MT6359_AUD_TOP_RST_CON0_CLR          0x2324
#define MT6359_AUD_TOP_RST_BANK_CON0         0x2326
#define MT6359_AUD_TOP_INT_CON0              0x2328
#define MT6359_AUD_TOP_INT_CON0_SET          0x232a
#define MT6359_AUD_TOP_INT_CON0_CLR          0x232c
#define MT6359_AUD_TOP_INT_MASK_CON0         0x232e
#define MT6359_AUD_TOP_INT_MASK_CON0_SET     0x2330
#define MT6359_AUD_TOP_INT_MASK_CON0_CLR     0x2332
#define MT6359_AUD_TOP_INT_STATUS0           0x2334
#define MT6359_AUD_TOP_INT_RAW_STATUS0       0x2336
#define MT6359_AUD_TOP_INT_MISC_CON0         0x2338
#define MT6359_AUD_TOP_MON_CON0              0x233a
#define MT6359_AUDIO_DIG_DSN_ID              0x2380
#define MT6359_AUDIO_DIG_DSN_REV0            0x2382
#define MT6359_AUDIO_DIG_DSN_DBI             0x2384
#define MT6359_AUDIO_DIG_DSN_DXI             0x2386
#define MT6359_AFE_UL_DL_CON0                0x2388
#define MT6359_AFE_DL_SRC2_CON0_L            0x238a
#define MT6359_AFE_UL_SRC_CON0_H             0x238c
#define MT6359_AFE_UL_SRC_CON0_L             0x238e
#define MT6359_AFE_ADDA6_L_SRC_CON0_H        0x2390
#define MT6359_AFE_ADDA6_UL_SRC_CON0_L       0x2392
#define MT6359_AFE_TOP_CON0                  0x2394
#define MT6359_AUDIO_TOP_CON0                0x2396
#define MT6359_AFE_MON_DEBUG0                0x2398
#define MT6359_AFUNC_AUD_CON0                0x239a
#define MT6359_AFUNC_AUD_CON1                0x239c
#define MT6359_AFUNC_AUD_CON2                0x239e
#define MT6359_AFUNC_AUD_CON3                0x23a0
#define MT6359_AFUNC_AUD_CON4                0x23a2
#define MT6359_AFUNC_AUD_CON5                0x23a4
#define MT6359_AFUNC_AUD_CON6                0x23a6
#define MT6359_AFUNC_AUD_CON7                0x23a8
#define MT6359_AFUNC_AUD_CON8                0x23aa
#define MT6359_AFUNC_AUD_CON9                0x23ac
#define MT6359_AFUNC_AUD_CON10               0x23ae
#define MT6359_AFUNC_AUD_CON11               0x23b0
#define MT6359_AFUNC_AUD_CON12               0x23b2
#define MT6359_AFUNC_AUD_MON0                0x23b4
#define MT6359_AFUNC_AUD_MON1                0x23b6
#define MT6359_AUDRC_TUNE_MON0               0x23b8
#define MT6359_AFE_ADDA_MTKAIF_FIFO_CFG0     0x23ba
#define MT6359_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 0x23bc
#define MT6359_AFE_ADDA_MTKAIF_MON0          0x23be
#define MT6359_AFE_ADDA_MTKAIF_MON1          0x23c0
#define MT6359_AFE_ADDA_MTKAIF_MON2          0x23c2
#define MT6359_AFE_ADDA6_MTKAIF_MON3         0x23c4
#define MT6359_AFE_ADDA_MTKAIF_MON4          0x23c6
#define MT6359_AFE_ADDA_MTKAIF_MON5          0x23c8
#define MT6359_AFE_ADDA_MTKAIF_CFG0          0x23ca
#define MT6359_AFE_ADDA_MTKAIF_RX_CFG0       0x23cc
#define MT6359_AFE_ADDA_MTKAIF_RX_CFG1       0x23ce
#define MT6359_AFE_ADDA_MTKAIF_RX_CFG2       0x23d0
#define MT6359_AFE_ADDA_MTKAIF_RX_CFG3       0x23d2
#define MT6359_AFE_ADDA_MTKAIF_SYNCWORD_CFG0 0x23d4
#define MT6359_AFE_ADDA_MTKAIF_SYNCWORD_CFG1 0x23d6
#define MT6359_AFE_SGEN_CFG0                 0x23d8
#define MT6359_AFE_SGEN_CFG1                 0x23da
#define MT6359_AFE_ADC_ASYNC_FIFO_CFG        0x23dc
#define MT6359_AFE_ADC_ASYNC_FIFO_CFG1       0x23de
#define MT6359_AFE_DCCLK_CFG0                0x23e0
#define MT6359_AFE_DCCLK_CFG1                0x23e2
#define MT6359_AUDIO_DIG_CFG                 0x23e4
#define MT6359_AUDIO_DIG_CFG1                0x23e6
#define MT6359_AFE_AUD_PAD_TOP               0x23e8
#define MT6359_AFE_AUD_PAD_TOP_MON           0x23ea
#define MT6359_AFE_AUD_PAD_TOP_MON1          0x23ec
#define MT6359_AFE_AUD_PAD_TOP_MON2          0x23ee
#define MT6359_AFE_DL_NLE_CFG                0x23f0
#define MT6359_AFE_DL_NLE_MON                0x23f2
#define MT6359_AFE_CG_EN_MON                 0x23f4
#define MT6359_AFE_MIC_ARRAY_CFG             0x23f6
#define MT6359_AFE_CHOP_CFG0                 0x23f8
#define MT6359_AFE_MTKAIF_MUX_CFG            0x23fa
#define MT6359_AUDIO_DIG_2ND_DSN_ID          0x2400
#define MT6359_AUDIO_DIG_2ND_DSN_REV0        0x2402
#define MT6359_AUDIO_DIG_2ND_DSN_DBI         0x2404
#define MT6359_AUDIO_DIG_2ND_DSN_DXI         0x2406
#define MT6359_AFE_PMIC_NEWIF_CFG3           0x2408
#define MT6359_AFE_VOW_TOP_CON0              0x240a
#define MT6359_AFE_VOW_TOP_CON1              0x240c
#define MT6359_AFE_VOW_TOP_CON2              0x240e
#define MT6359_AFE_VOW_TOP_CON3              0x2410
#define MT6359_AFE_VOW_TOP_CON4              0x2412
#define MT6359_AFE_VOW_TOP_MON0              0x2414
#define MT6359_AFE_VOW_VAD_CFG0              0x2416
#define MT6359_AFE_VOW_VAD_CFG1              0x2418
#define MT6359_AFE_VOW_VAD_CFG2              0x241a
#define MT6359_AFE_VOW_VAD_CFG3              0x241c
#define MT6359_AFE_VOW_VAD_CFG4              0x241e
#define MT6359_AFE_VOW_VAD_CFG5              0x2420
#define MT6359_AFE_VOW_VAD_CFG6              0x2422
#define MT6359_AFE_VOW_VAD_CFG7              0x2424
#define MT6359_AFE_VOW_VAD_CFG8              0x2426
#define MT6359_AFE_VOW_VAD_CFG9              0x2428
#define MT6359_AFE_VOW_VAD_CFG10             0x242a
#define MT6359_AFE_VOW_VAD_CFG11             0x242c
#define MT6359_AFE_VOW_VAD_CFG12             0x242e
#define MT6359_AFE_VOW_VAD_MON0              0x2430
#define MT6359_AFE_VOW_VAD_MON1              0x2432
#define MT6359_AFE_VOW_VAD_MON2              0x2434
#define MT6359_AFE_VOW_VAD_MON3              0x2436
#define MT6359_AFE_VOW_VAD_MON4              0x2438
#define MT6359_AFE_VOW_VAD_MON5              0x243a
#define MT6359_AFE_VOW_VAD_MON6              0x243c
#define MT6359_AFE_VOW_VAD_MON7              0x243e
#define MT6359_AFE_VOW_VAD_MON8              0x2440
#define MT6359_AFE_VOW_VAD_MON9              0x2442
#define MT6359_AFE_VOW_VAD_MON10             0x2444
#define MT6359_AFE_VOW_VAD_MON11             0x2446
#define MT6359_AFE_VOW_TGEN_CFG0             0x2448
#define MT6359_AFE_VOW_TGEN_CFG1             0x244a
#define MT6359_AFE_VOW_HPF_CFG0              0x244c
#define MT6359_AFE_VOW_HPF_CFG1              0x244e
#define MT6359_AUDIO_DIG_3RD_DSN_ID          0x2480
#define MT6359_AUDIO_DIG_3RD_DSN_REV0        0x2482
#define MT6359_AUDIO_DIG_3RD_DSN_DBI         0x2484
#define MT6359_AUDIO_DIG_3RD_DSN_DXI         0x2486
#define MT6359_AFE_VOW_PERIODIC_CFG0         0x2488
#define MT6359_AFE_VOW_PERIODIC_CFG1         0x248a
#define MT6359_AFE_VOW_PERIODIC_CFG2         0x248c
#define MT6359_AFE_VOW_PERIODIC_CFG3         0x248e
#define MT6359_AFE_VOW_PERIODIC_CFG4         0x2490
#define MT6359_AFE_VOW_PERIODIC_CFG5         0x2492
#define MT6359_AFE_VOW_PERIODIC_CFG6         0x2494
#define MT6359_AFE_VOW_PERIODIC_CFG7         0x2496
#define MT6359_AFE_VOW_PERIODIC_CFG8         0x2498
#define MT6359_AFE_VOW_PERIODIC_CFG9         0x249a
#define MT6359_AFE_VOW_PERIODIC_CFG10        0x249c
#define MT6359_AFE_VOW_PERIODIC_CFG11        0x249e
#define MT6359_AFE_VOW_PERIODIC_CFG12        0x24a0
#define MT6359_AFE_VOW_PERIODIC_CFG13        0x24a2
#define MT6359_AFE_VOW_PERIODIC_CFG14        0x24a4
#define MT6359_AFE_VOW_PERIODIC_CFG15        0x24a6
#define MT6359_AFE_VOW_PERIODIC_CFG16        0x24a8
#define MT6359_AFE_VOW_PERIODIC_CFG17        0x24aa
#define MT6359_AFE_VOW_PERIODIC_CFG18        0x24ac
#define MT6359_AFE_VOW_PERIODIC_CFG19        0x24ae
#define MT6359_AFE_VOW_PERIODIC_CFG20        0x24b0
#define MT6359_AFE_VOW_PERIODIC_CFG21        0x24b2
#define MT6359_AFE_VOW_PERIODIC_CFG22        0x24b4
#define MT6359_AFE_VOW_PERIODIC_CFG23        0x24b6
#define MT6359_AFE_VOW_PERIODIC_CFG24        0x24b8
#define MT6359_AFE_VOW_PERIODIC_CFG25        0x24ba
#define MT6359_AFE_VOW_PERIODIC_CFG26        0x24bc
#define MT6359_AFE_VOW_PERIODIC_CFG27        0x24be
#define MT6359_AFE_VOW_PERIODIC_CFG28        0x24c0
#define MT6359_AFE_VOW_PERIODIC_CFG29        0x24c2
#define MT6359_AFE_VOW_PERIODIC_CFG30        0x24c4
#define MT6359_AFE_VOW_PERIODIC_CFG31        0x24c6
#define MT6359_AFE_VOW_PERIODIC_CFG32        0x24c8
#define MT6359_AFE_VOW_PERIODIC_CFG33        0x24ca
#define MT6359_AFE_VOW_PERIODIC_CFG34        0x24cc
#define MT6359_AFE_VOW_PERIODIC_CFG35        0x24ce
#define MT6359_AFE_VOW_PERIODIC_CFG36        0x24d0
#define MT6359_AFE_VOW_PERIODIC_CFG37        0x24d2
#define MT6359_AFE_VOW_PERIODIC_CFG38        0x24d4
#define MT6359_AFE_VOW_PERIODIC_CFG39        0x24d6
#define MT6359_AFE_VOW_PERIODIC_MON0         0x24d8
#define MT6359_AFE_VOW_PERIODIC_MON1         0x24da
#define MT6359_AFE_VOW_PERIODIC_MON2         0x24dc
#define MT6359_AFE_NCP_CFG0                  0x24de
#define MT6359_AFE_NCP_CFG1                  0x24e0
#define MT6359_AFE_NCP_CFG2                  0x24e2
#define MT6359_AUDENC_DSN_ID                 0x2500
#define MT6359_AUDENC_DSN_REV0               0x2502
#define MT6359_AUDENC_DSN_DBI                0x2504
#define MT6359_AUDENC_DSN_FPI                0x2506
#define MT6359_AUDENC_ANA_CON0               0x2508
#define MT6359_AUDENC_ANA_CON1               0x250a
#define MT6359_AUDENC_ANA_CON2               0x250c
#define MT6359_AUDENC_ANA_CON3               0x250e
#define MT6359_AUDENC_ANA_CON4               0x2510
#define MT6359_AUDENC_ANA_CON5               0x2512
#define MT6359_AUDENC_ANA_CON6               0x2514
#define MT6359_AUDENC_ANA_CON7               0x2516
#define MT6359_AUDENC_ANA_CON8               0x2518
#define MT6359_AUDENC_ANA_CON9               0x251a
#define MT6359_AUDENC_ANA_CON10              0x251c
#define MT6359_AUDENC_ANA_CON11              0x251e
#define MT6359_AUDENC_ANA_CON12              0x2520
#define MT6359_AUDENC_ANA_CON13              0x2522
#define MT6359_AUDENC_ANA_CON14              0x2524
#define MT6359_AUDENC_ANA_CON15              0x2526
#define MT6359_AUDENC_ANA_CON16              0x2528
#define MT6359_AUDENC_ANA_CON17              0x252a
#define MT6359_AUDENC_ANA_CON18              0x252c
#define MT6359_AUDENC_ANA_CON19              0x252e
#define MT6359_AUDENC_ANA_CON20              0x2530
#define MT6359_AUDENC_ANA_CON21              0x2532
#define MT6359_AUDENC_ANA_CON22              0x2534
#define MT6359_AUDENC_ANA_CON23              0x2536
#define MT6359_AUDDEC_DSN_ID                 0x2580
#define MT6359_AUDDEC_DSN_REV0               0x2582
#define MT6359_AUDDEC_DSN_DBI                0x2584
#define MT6359_AUDDEC_DSN_FPI                0x2586
#define MT6359_AUDDEC_ANA_CON0               0x2588
#define MT6359_AUDDEC_ANA_CON1               0x258a
#define MT6359_AUDDEC_ANA_CON2               0x258c
#define MT6359_AUDDEC_ANA_CON3               0x258e
#define MT6359_AUDDEC_ANA_CON4               0x2590
#define MT6359_AUDDEC_ANA_CON5               0x2592
#define MT6359_AUDDEC_ANA_CON6               0x2594
#define MT6359_AUDDEC_ANA_CON7               0x2596
#define MT6359_AUDDEC_ANA_CON8               0x2598
#define MT6359_AUDDEC_ANA_CON9               0x259a
#define MT6359_AUDDEC_ANA_CON10              0x259c
#define MT6359_AUDDEC_ANA_CON11              0x259e
#define MT6359_AUDDEC_ANA_CON12              0x25a0
#define MT6359_AUDDEC_ANA_CON13              0x25a2
#define MT6359_AUDDEC_ANA_CON14              0x25a4
#define MT6359_AUDZCD_DSN_ID                 0x2600
#define MT6359_AUDZCD_DSN_REV0               0x2602
#define MT6359_AUDZCD_DSN_DBI                0x2604
#define MT6359_AUDZCD_DSN_FPI                0x2606
#define MT6359_ZCD_CON0                      0x2608
#define MT6359_ZCD_CON1                      0x260a
#define MT6359_ZCD_CON2                      0x260c
#define MT6359_ZCD_CON3                      0x260e
#define MT6359_ZCD_CON4                      0x2610
#define MT6359_ZCD_CON5                      0x2612
#define MT6359_ACCDET_DSN_DIG_ID             0x2680
#define MT6359_ACCDET_DSN_DIG_REV0           0x2682
#define MT6359_ACCDET_DSN_DBI                0x2684
#define MT6359_ACCDET_DSN_FPI                0x2686
#define MT6359_ACCDET_CON0                   0x2688
#define MT6359_ACCDET_CON1                   0x268a
#define MT6359_ACCDET_CON2                   0x268c
#define MT6359_ACCDET_CON3                   0x268e
#define MT6359_ACCDET_CON4                   0x2690
#define MT6359_ACCDET_CON5                   0x2692
#define MT6359_ACCDET_CON6                   0x2694
#define MT6359_ACCDET_CON7                   0x2696
#define MT6359_ACCDET_CON8                   0x2698
#define MT6359_ACCDET_CON9                   0x269a
#define MT6359_ACCDET_CON10                  0x269c
#define MT6359_ACCDET_CON11                  0x269e
#define MT6359_ACCDET_CON12                  0x26a0
#define MT6359_ACCDET_CON13                  0x26a2
#define MT6359_ACCDET_CON14                  0x26a4
#define MT6359_ACCDET_CON15                  0x26a6
#define MT6359_ACCDET_CON16                  0x26a8
#define MT6359_ACCDET_CON17                  0x26aa
#define MT6359_ACCDET_CON18                  0x26ac
#define MT6359_ACCDET_CON19                  0x26ae
#define MT6359_ACCDET_CON20                  0x26b0
#define MT6359_ACCDET_CON21                  0x26b2
#define MT6359_ACCDET_CON22                  0x26b4
#define MT6359_ACCDET_CON23                  0x26b6
#define MT6359_ACCDET_CON24                  0x26b8
#define MT6359_ACCDET_CON25                  0x26ba
#define MT6359_ACCDET_CON26                  0x26bc
#define MT6359_ACCDET_CON27                  0x26be
#define MT6359_ACCDET_CON28                  0x26c0
#define MT6359_ACCDET_CON29                  0x26c2
#define MT6359_ACCDET_CON30                  0x26c4
#define MT6359_ACCDET_CON31                  0x26c6
#define MT6359_ACCDET_CON32                  0x26c8
#define MT6359_ACCDET_CON33                  0x26ca
#define MT6359_ACCDET_CON34                  0x26cc
#define MT6359_ACCDET_CON35                  0x26ce
#define MT6359_ACCDET_CON36                  0x26d0
#define MT6359_ACCDET_CON37                  0x26d2
#define MT6359_ACCDET_CON38                  0x26d4
#define MT6359_ACCDET_CON39                  0x26d6
#define MT6359_ACCDET_CON40                  0x26d8

#define MT6359_BUCK_TOP_ANA_ID_ADDR                           \
	MT6359_BUCK_TOP_DSN_ID
#define MT6359_BUCK_TOP_ANA_ID_MASK                           0xFF
#define MT6359_BUCK_TOP_ANA_ID_SHIFT                          0
#define MT6359_BUCK_TOP_DIG_ID_ADDR                           \
	MT6359_BUCK_TOP_DSN_ID
#define MT6359_BUCK_TOP_DIG_ID_MASK                           0xFF
#define MT6359_BUCK_TOP_DIG_ID_SHIFT                          8
#define MT6359_BUCK_TOP_ANA_MINOR_REV_ADDR                    \
	MT6359_BUCK_TOP_DSN_REV0
#define MT6359_BUCK_TOP_ANA_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_TOP_ANA_MINOR_REV_SHIFT                   0
#define MT6359_BUCK_TOP_ANA_MAJOR_REV_ADDR                    \
	MT6359_BUCK_TOP_DSN_REV0
#define MT6359_BUCK_TOP_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_TOP_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_BUCK_TOP_DIG_MINOR_REV_ADDR                    \
	MT6359_BUCK_TOP_DSN_REV0
#define MT6359_BUCK_TOP_DIG_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_TOP_DIG_MINOR_REV_SHIFT                   8
#define MT6359_BUCK_TOP_DIG_MAJOR_REV_ADDR                    \
	MT6359_BUCK_TOP_DSN_REV0
#define MT6359_BUCK_TOP_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_TOP_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_BUCK_TOP_CBS_ADDR                              \
	MT6359_BUCK_TOP_DBI
#define MT6359_BUCK_TOP_CBS_MASK                              0x3
#define MT6359_BUCK_TOP_CBS_SHIFT                             0
#define MT6359_BUCK_TOP_BIX_ADDR                              \
	MT6359_BUCK_TOP_DBI
#define MT6359_BUCK_TOP_BIX_MASK                              0x3
#define MT6359_BUCK_TOP_BIX_SHIFT                             2
#define MT6359_BUCK_TOP_ESP_ADDR                              \
	MT6359_BUCK_TOP_DBI
#define MT6359_BUCK_TOP_ESP_MASK                              0xFF
#define MT6359_BUCK_TOP_ESP_SHIFT                             8
#define MT6359_BUCK_TOP_FPI_ADDR                              \
	MT6359_BUCK_TOP_DXI
#define MT6359_BUCK_TOP_FPI_MASK                              0xFF
#define MT6359_BUCK_TOP_FPI_SHIFT                             0
#define MT6359_BUCK_TOP_CLK_OFFSET_ADDR                       \
	MT6359_BUCK_TOP_PAM0
#define MT6359_BUCK_TOP_CLK_OFFSET_MASK                       0xFF
#define MT6359_BUCK_TOP_CLK_OFFSET_SHIFT                      0
#define MT6359_BUCK_TOP_RST_OFFSET_ADDR                       \
	MT6359_BUCK_TOP_PAM0
#define MT6359_BUCK_TOP_RST_OFFSET_MASK                       0xFF
#define MT6359_BUCK_TOP_RST_OFFSET_SHIFT                      8
#define MT6359_BUCK_TOP_INT_OFFSET_ADDR                       \
	MT6359_BUCK_TOP_PAM1
#define MT6359_BUCK_TOP_INT_OFFSET_MASK                       0xFF
#define MT6359_BUCK_TOP_INT_OFFSET_SHIFT                      0
#define MT6359_BUCK_TOP_INT_LEN_ADDR                          \
	MT6359_BUCK_TOP_PAM1
#define MT6359_BUCK_TOP_INT_LEN_MASK                          0xFF
#define MT6359_BUCK_TOP_INT_LEN_SHIFT                         8
#define MT6359_RG_BUCK32K_CK_PDN_ADDR                         \
	MT6359_BUCK_TOP_CLK_CON0
#define MT6359_RG_BUCK32K_CK_PDN_MASK                         0x1
#define MT6359_RG_BUCK32K_CK_PDN_SHIFT                        0
#define MT6359_RG_BUCK1M_CK_PDN_ADDR                          \
	MT6359_BUCK_TOP_CLK_CON0
#define MT6359_RG_BUCK1M_CK_PDN_MASK                          0x1
#define MT6359_RG_BUCK1M_CK_PDN_SHIFT                         1
#define MT6359_RG_BUCK26M_CK_PDN_ADDR                         \
	MT6359_BUCK_TOP_CLK_CON0
#define MT6359_RG_BUCK26M_CK_PDN_MASK                         0x1
#define MT6359_RG_BUCK26M_CK_PDN_SHIFT                        2
#define MT6359_RG_BUCK_VPA_ANA_2M_CK_PDN_ADDR                 \
	MT6359_BUCK_TOP_CLK_CON0
#define MT6359_RG_BUCK_VPA_ANA_2M_CK_PDN_MASK                 0x1
#define MT6359_RG_BUCK_VPA_ANA_2M_CK_PDN_SHIFT                3
#define MT6359_RG_BUCK_TOP_CLK_CON0_SET_ADDR                  \
	MT6359_BUCK_TOP_CLK_CON0_SET
#define MT6359_RG_BUCK_TOP_CLK_CON0_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_TOP_CLK_CON0_SET_SHIFT                 0
#define MT6359_RG_BUCK_TOP_CLK_CON0_CLR_ADDR                  \
	MT6359_BUCK_TOP_CLK_CON0_CLR
#define MT6359_RG_BUCK_TOP_CLK_CON0_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_TOP_CLK_CON0_CLR_SHIFT                 0
#define MT6359_RG_BUCK32K_CK_PDN_HWEN_ADDR                    \
	MT6359_BUCK_TOP_CLK_HWEN_CON0
#define MT6359_RG_BUCK32K_CK_PDN_HWEN_MASK                    0x1
#define MT6359_RG_BUCK32K_CK_PDN_HWEN_SHIFT                   0
#define MT6359_RG_BUCK1M_CK_PDN_HWEN_ADDR                     \
	MT6359_BUCK_TOP_CLK_HWEN_CON0
#define MT6359_RG_BUCK1M_CK_PDN_HWEN_MASK                     0x1
#define MT6359_RG_BUCK1M_CK_PDN_HWEN_SHIFT                    1
#define MT6359_RG_BUCK26M_CK_PDN_HWEN_ADDR                    \
	MT6359_BUCK_TOP_CLK_HWEN_CON0
#define MT6359_RG_BUCK26M_CK_PDN_HWEN_MASK                    0x1
#define MT6359_RG_BUCK26M_CK_PDN_HWEN_SHIFT                   2
#define MT6359_RG_BUCK_SLEEP_CTRL_MODE_ADDR                   \
	MT6359_BUCK_TOP_CLK_HWEN_CON0
#define MT6359_RG_BUCK_SLEEP_CTRL_MODE_MASK                   0x1
#define MT6359_RG_BUCK_SLEEP_CTRL_MODE_SHIFT                  3
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_SET_ADDR             \
	MT6359_BUCK_TOP_CLK_HWEN_CON0_SET
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_SET_MASK             0xFFFF
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_SET_SHIFT            0
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_CLR_ADDR             \
	MT6359_BUCK_TOP_CLK_HWEN_CON0_CLR
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_CLR_MASK             0xFFFF
#define MT6359_RG_BUCK_TOP_CLK_HWEN_CON0_CLR_SHIFT            0
#define MT6359_RG_INT_EN_VPU_OC_ADDR                          \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VPU_OC_MASK                          0x1
#define MT6359_RG_INT_EN_VPU_OC_SHIFT                         0
#define MT6359_RG_INT_EN_VCORE_OC_ADDR                        \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCORE_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VCORE_OC_SHIFT                       1
#define MT6359_RG_INT_EN_VGPU11_OC_ADDR                       \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VGPU11_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VGPU11_OC_SHIFT                      2
#define MT6359_RG_INT_EN_VGPU12_OC_ADDR                       \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VGPU12_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VGPU12_OC_SHIFT                      3
#define MT6359_RG_INT_EN_VMODEM_OC_ADDR                       \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VMODEM_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VMODEM_OC_SHIFT                      4
#define MT6359_RG_INT_EN_VPROC1_OC_ADDR                       \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VPROC1_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VPROC1_OC_SHIFT                      5
#define MT6359_RG_INT_EN_VPROC2_OC_ADDR                       \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VPROC2_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VPROC2_OC_SHIFT                      6
#define MT6359_RG_INT_EN_VS1_OC_ADDR                          \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VS1_OC_MASK                          0x1
#define MT6359_RG_INT_EN_VS1_OC_SHIFT                         7
#define MT6359_RG_INT_EN_VS2_OC_ADDR                          \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VS2_OC_MASK                          0x1
#define MT6359_RG_INT_EN_VS2_OC_SHIFT                         8
#define MT6359_RG_INT_EN_VPA_OC_ADDR                          \
	MT6359_BUCK_TOP_INT_CON0
#define MT6359_RG_INT_EN_VPA_OC_MASK                          0x1
#define MT6359_RG_INT_EN_VPA_OC_SHIFT                         9
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_SET_ADDR               \
	MT6359_BUCK_TOP_INT_CON0_SET
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_SET_MASK               0xFFFF
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_SET_SHIFT              0
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_CLR_ADDR               \
	MT6359_BUCK_TOP_INT_CON0_CLR
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_CLR_MASK               0xFFFF
#define MT6359_RG_BUCK_TOP_INT_EN_CON0_CLR_SHIFT              0
#define MT6359_RG_INT_MASK_VPU_OC_ADDR                        \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VPU_OC_MASK                        0x1
#define MT6359_RG_INT_MASK_VPU_OC_SHIFT                       0
#define MT6359_RG_INT_MASK_VCORE_OC_ADDR                      \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCORE_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VCORE_OC_SHIFT                     1
#define MT6359_RG_INT_MASK_VGPU11_OC_ADDR                     \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VGPU11_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VGPU11_OC_SHIFT                    2
#define MT6359_RG_INT_MASK_VGPU12_OC_ADDR                     \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VGPU12_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VGPU12_OC_SHIFT                    3
#define MT6359_RG_INT_MASK_VMODEM_OC_ADDR                     \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VMODEM_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VMODEM_OC_SHIFT                    4
#define MT6359_RG_INT_MASK_VPROC1_OC_ADDR                     \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VPROC1_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VPROC1_OC_SHIFT                    5
#define MT6359_RG_INT_MASK_VPROC2_OC_ADDR                     \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VPROC2_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VPROC2_OC_SHIFT                    6
#define MT6359_RG_INT_MASK_VS1_OC_ADDR                        \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VS1_OC_MASK                        0x1
#define MT6359_RG_INT_MASK_VS1_OC_SHIFT                       7
#define MT6359_RG_INT_MASK_VS2_OC_ADDR                        \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VS2_OC_MASK                        0x1
#define MT6359_RG_INT_MASK_VS2_OC_SHIFT                       8
#define MT6359_RG_INT_MASK_VPA_OC_ADDR                        \
	MT6359_BUCK_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VPA_OC_MASK                        0x1
#define MT6359_RG_INT_MASK_VPA_OC_SHIFT                       9
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_SET_ADDR             \
	MT6359_BUCK_TOP_INT_MASK_CON0_SET
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_SET_MASK             0xFFFF
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_SET_SHIFT            0
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_CLR_ADDR             \
	MT6359_BUCK_TOP_INT_MASK_CON0_CLR
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_CLR_MASK             0xFFFF
#define MT6359_RG_BUCK_TOP_INT_MASK_CON0_CLR_SHIFT            0
#define MT6359_RG_INT_STATUS_VPU_OC_ADDR                      \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VPU_OC_MASK                      0x1
#define MT6359_RG_INT_STATUS_VPU_OC_SHIFT                     0
#define MT6359_RG_INT_STATUS_VCORE_OC_ADDR                    \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCORE_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VCORE_OC_SHIFT                   1
#define MT6359_RG_INT_STATUS_VGPU11_OC_ADDR                   \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VGPU11_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VGPU11_OC_SHIFT                  2
#define MT6359_RG_INT_STATUS_VGPU12_OC_ADDR                   \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VGPU12_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VGPU12_OC_SHIFT                  3
#define MT6359_RG_INT_STATUS_VMODEM_OC_ADDR                   \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VMODEM_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VMODEM_OC_SHIFT                  4
#define MT6359_RG_INT_STATUS_VPROC1_OC_ADDR                   \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VPROC1_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VPROC1_OC_SHIFT                  5
#define MT6359_RG_INT_STATUS_VPROC2_OC_ADDR                   \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VPROC2_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VPROC2_OC_SHIFT                  6
#define MT6359_RG_INT_STATUS_VS1_OC_ADDR                      \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VS1_OC_MASK                      0x1
#define MT6359_RG_INT_STATUS_VS1_OC_SHIFT                     7
#define MT6359_RG_INT_STATUS_VS2_OC_ADDR                      \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VS2_OC_MASK                      0x1
#define MT6359_RG_INT_STATUS_VS2_OC_SHIFT                     8
#define MT6359_RG_INT_STATUS_VPA_OC_ADDR                      \
	MT6359_BUCK_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VPA_OC_MASK                      0x1
#define MT6359_RG_INT_STATUS_VPA_OC_SHIFT                     9
#define MT6359_RG_INT_RAW_STATUS_VPU_OC_ADDR                  \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VPU_OC_MASK                  0x1
#define MT6359_RG_INT_RAW_STATUS_VPU_OC_SHIFT                 0
#define MT6359_RG_INT_RAW_STATUS_VCORE_OC_ADDR                \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCORE_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VCORE_OC_SHIFT               1
#define MT6359_RG_INT_RAW_STATUS_VGPU11_OC_ADDR               \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VGPU11_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VGPU11_OC_SHIFT              2
#define MT6359_RG_INT_RAW_STATUS_VGPU12_OC_ADDR               \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VGPU12_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VGPU12_OC_SHIFT              3
#define MT6359_RG_INT_RAW_STATUS_VMODEM_OC_ADDR               \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VMODEM_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VMODEM_OC_SHIFT              4
#define MT6359_RG_INT_RAW_STATUS_VPROC1_OC_ADDR               \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VPROC1_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VPROC1_OC_SHIFT              5
#define MT6359_RG_INT_RAW_STATUS_VPROC2_OC_ADDR               \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VPROC2_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VPROC2_OC_SHIFT              6
#define MT6359_RG_INT_RAW_STATUS_VS1_OC_ADDR                  \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VS1_OC_MASK                  0x1
#define MT6359_RG_INT_RAW_STATUS_VS1_OC_SHIFT                 7
#define MT6359_RG_INT_RAW_STATUS_VS2_OC_ADDR                  \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VS2_OC_MASK                  0x1
#define MT6359_RG_INT_RAW_STATUS_VS2_OC_SHIFT                 8
#define MT6359_RG_INT_RAW_STATUS_VPA_OC_ADDR                  \
	MT6359_BUCK_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VPA_OC_MASK                  0x1
#define MT6359_RG_INT_RAW_STATUS_VPA_OC_SHIFT                 9
#define MT6359_RG_VOW_BUCK_VCORE_DVS_DONE_ADDR                \
	MT6359_BUCK_TOP_VOW_CON
#define MT6359_RG_VOW_BUCK_VCORE_DVS_DONE_MASK                0x1
#define MT6359_RG_VOW_BUCK_VCORE_DVS_DONE_SHIFT               0
#define MT6359_RG_VOW_BUCK_VCORE_DVS_SW_MODE_ADDR             \
	MT6359_BUCK_TOP_VOW_CON
#define MT6359_RG_VOW_BUCK_VCORE_DVS_SW_MODE_MASK             0x1
#define MT6359_RG_VOW_BUCK_VCORE_DVS_SW_MODE_SHIFT            1
#define MT6359_RG_BUCK_STB_MAX_ADDR                           \
	MT6359_BUCK_TOP_STB_CON
#define MT6359_RG_BUCK_STB_MAX_MASK                           0x1FF
#define MT6359_RG_BUCK_STB_MAX_SHIFT                          0
#define MT6359_RG_BUCK_VGP2_MINFREQ_LATENCY_MAX_ADDR          \
	MT6359_BUCK_TOP_VGP2_MINFREQ_CON
#define MT6359_RG_BUCK_VGP2_MINFREQ_LATENCY_MAX_MASK          0xFF
#define MT6359_RG_BUCK_VGP2_MINFREQ_LATENCY_MAX_SHIFT         0
#define MT6359_RG_BUCK_VGP2_MINFREQ_DURATION_MAX_ADDR         \
	MT6359_BUCK_TOP_VGP2_MINFREQ_CON
#define MT6359_RG_BUCK_VGP2_MINFREQ_DURATION_MAX_MASK         0x7
#define MT6359_RG_BUCK_VGP2_MINFREQ_DURATION_MAX_SHIFT        8
#define MT6359_RG_BUCK_VPA_MINFREQ_LATENCY_MAX_ADDR           \
	MT6359_BUCK_TOP_VPA_MINFREQ_CON
#define MT6359_RG_BUCK_VPA_MINFREQ_LATENCY_MAX_MASK           0xFF
#define MT6359_RG_BUCK_VPA_MINFREQ_LATENCY_MAX_SHIFT          0
#define MT6359_RG_BUCK_VPA_MINFREQ_DURATION_MAX_ADDR          \
	MT6359_BUCK_TOP_VPA_MINFREQ_CON
#define MT6359_RG_BUCK_VPA_MINFREQ_DURATION_MAX_MASK          0xF
#define MT6359_RG_BUCK_VPA_MINFREQ_DURATION_MAX_SHIFT         8
#define MT6359_RG_BUCK_VPU_OC_SDN_STATUS_ADDR                 \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VPU_OC_SDN_STATUS_MASK                 0x1
#define MT6359_RG_BUCK_VPU_OC_SDN_STATUS_SHIFT                0
#define MT6359_RG_BUCK_VCORE_OC_SDN_STATUS_ADDR               \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VCORE_OC_SDN_STATUS_MASK               0x1
#define MT6359_RG_BUCK_VCORE_OC_SDN_STATUS_SHIFT              1
#define MT6359_RG_BUCK_VGPU11_OC_SDN_STATUS_ADDR              \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VGPU11_OC_SDN_STATUS_MASK              0x1
#define MT6359_RG_BUCK_VGPU11_OC_SDN_STATUS_SHIFT             2
#define MT6359_RG_BUCK_VGPU12_OC_SDN_STATUS_ADDR              \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VGPU12_OC_SDN_STATUS_MASK              0x1
#define MT6359_RG_BUCK_VGPU12_OC_SDN_STATUS_SHIFT             3
#define MT6359_RG_BUCK_VMODEM_OC_SDN_STATUS_ADDR              \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VMODEM_OC_SDN_STATUS_MASK              0x1
#define MT6359_RG_BUCK_VMODEM_OC_SDN_STATUS_SHIFT             4
#define MT6359_RG_BUCK_VPROC1_OC_SDN_STATUS_ADDR              \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VPROC1_OC_SDN_STATUS_MASK              0x1
#define MT6359_RG_BUCK_VPROC1_OC_SDN_STATUS_SHIFT             5
#define MT6359_RG_BUCK_VPROC2_OC_SDN_STATUS_ADDR              \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VPROC2_OC_SDN_STATUS_MASK              0x1
#define MT6359_RG_BUCK_VPROC2_OC_SDN_STATUS_SHIFT             6
#define MT6359_RG_BUCK_VS1_OC_SDN_STATUS_ADDR                 \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VS1_OC_SDN_STATUS_MASK                 0x1
#define MT6359_RG_BUCK_VS1_OC_SDN_STATUS_SHIFT                7
#define MT6359_RG_BUCK_VS2_OC_SDN_STATUS_ADDR                 \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VS2_OC_SDN_STATUS_MASK                 0x1
#define MT6359_RG_BUCK_VS2_OC_SDN_STATUS_SHIFT                8
#define MT6359_RG_BUCK_VPA_OC_SDN_STATUS_ADDR                 \
	MT6359_BUCK_TOP_OC_CON0
#define MT6359_RG_BUCK_VPA_OC_SDN_STATUS_MASK                 0x1
#define MT6359_RG_BUCK_VPA_OC_SDN_STATUS_SHIFT                9
#define MT6359_BUCK_TOP_WRITE_KEY_ADDR                        \
	MT6359_BUCK_TOP_KEY_PROT
#define MT6359_BUCK_TOP_WRITE_KEY_MASK                        0xFFFF
#define MT6359_BUCK_TOP_WRITE_KEY_SHIFT                       0
#define MT6359_BUCK_VPU_WDTDBG_VOSEL_ADDR                     \
	MT6359_BUCK_TOP_WDTDBG0
#define MT6359_BUCK_VPU_WDTDBG_VOSEL_MASK                     0x7F
#define MT6359_BUCK_VPU_WDTDBG_VOSEL_SHIFT                    0
#define MT6359_BUCK_VCORE_WDTDBG_VOSEL_ADDR                   \
	MT6359_BUCK_TOP_WDTDBG0
#define MT6359_BUCK_VCORE_WDTDBG_VOSEL_MASK                   0x7F
#define MT6359_BUCK_VCORE_WDTDBG_VOSEL_SHIFT                  8
#define MT6359_BUCK_VGPU11_WDTDBG_VOSEL_ADDR                  \
	MT6359_BUCK_TOP_WDTDBG1
#define MT6359_BUCK_VGPU11_WDTDBG_VOSEL_MASK                  0x7F
#define MT6359_BUCK_VGPU11_WDTDBG_VOSEL_SHIFT                 0
#define MT6359_BUCK_VGPU12_WDTDBG_VOSEL_ADDR                  \
	MT6359_BUCK_TOP_WDTDBG1
#define MT6359_BUCK_VGPU12_WDTDBG_VOSEL_MASK                  0x7F
#define MT6359_BUCK_VGPU12_WDTDBG_VOSEL_SHIFT                 8
#define MT6359_BUCK_VMODEM_WDTDBG_VOSEL_ADDR                  \
	MT6359_BUCK_TOP_WDTDBG2
#define MT6359_BUCK_VMODEM_WDTDBG_VOSEL_MASK                  0x7F
#define MT6359_BUCK_VMODEM_WDTDBG_VOSEL_SHIFT                 0
#define MT6359_BUCK_VPROC1_WDTDBG_VOSEL_ADDR                  \
	MT6359_BUCK_TOP_WDTDBG2
#define MT6359_BUCK_VPROC1_WDTDBG_VOSEL_MASK                  0x7F
#define MT6359_BUCK_VPROC1_WDTDBG_VOSEL_SHIFT                 8
#define MT6359_BUCK_VPROC2_WDTDBG_VOSEL_ADDR                  \
	MT6359_BUCK_TOP_WDTDBG3
#define MT6359_BUCK_VPROC2_WDTDBG_VOSEL_MASK                  0x7F
#define MT6359_BUCK_VPROC2_WDTDBG_VOSEL_SHIFT                 0
#define MT6359_BUCK_VS1_WDTDBG_VOSEL_ADDR                     \
	MT6359_BUCK_TOP_WDTDBG3
#define MT6359_BUCK_VS1_WDTDBG_VOSEL_MASK                     0x7F
#define MT6359_BUCK_VS1_WDTDBG_VOSEL_SHIFT                    8
#define MT6359_BUCK_VS2_WDTDBG_VOSEL_ADDR                     \
	MT6359_BUCK_TOP_WDTDBG4
#define MT6359_BUCK_VS2_WDTDBG_VOSEL_MASK                     0x7F
#define MT6359_BUCK_VS2_WDTDBG_VOSEL_SHIFT                    0
#define MT6359_BUCK_VPA_WDTDBG_VOSEL_ADDR                     \
	MT6359_BUCK_TOP_WDTDBG4
#define MT6359_BUCK_VPA_WDTDBG_VOSEL_MASK                     0x3F
#define MT6359_BUCK_VPA_WDTDBG_VOSEL_SHIFT                    8
#define MT6359_BUCK_TOP_ELR_LEN_ADDR                          \
	MT6359_BUCK_TOP_ELR_NUM
#define MT6359_BUCK_TOP_ELR_LEN_MASK                          0xFF
#define MT6359_BUCK_TOP_ELR_LEN_SHIFT                         0
#define MT6359_RG_BUCK_VPU_OC_SDN_EN_ADDR                     \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VPU_OC_SDN_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_OC_SDN_EN_SHIFT                    0
#define MT6359_RG_BUCK_VCORE_OC_SDN_EN_ADDR                   \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VCORE_OC_SDN_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_OC_SDN_EN_SHIFT                  1
#define MT6359_RG_BUCK_VGPU11_OC_SDN_EN_ADDR                  \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VGPU11_OC_SDN_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_OC_SDN_EN_SHIFT                 2
#define MT6359_RG_BUCK_VGPU12_OC_SDN_EN_ADDR                  \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VGPU12_OC_SDN_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_OC_SDN_EN_SHIFT                 3
#define MT6359_RG_BUCK_VMODEM_OC_SDN_EN_ADDR                  \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VMODEM_OC_SDN_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_OC_SDN_EN_SHIFT                 4
#define MT6359_RG_BUCK_VPROC1_OC_SDN_EN_ADDR                  \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VPROC1_OC_SDN_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_OC_SDN_EN_SHIFT                 5
#define MT6359_RG_BUCK_VPROC2_OC_SDN_EN_ADDR                  \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VPROC2_OC_SDN_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_OC_SDN_EN_SHIFT                 6
#define MT6359_RG_BUCK_VS1_OC_SDN_EN_ADDR                     \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VS1_OC_SDN_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_OC_SDN_EN_SHIFT                    7
#define MT6359_RG_BUCK_VS2_OC_SDN_EN_ADDR                     \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VS2_OC_SDN_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_OC_SDN_EN_SHIFT                    8
#define MT6359_RG_BUCK_VPA_OC_SDN_EN_ADDR                     \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_VPA_OC_SDN_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPA_OC_SDN_EN_SHIFT                    9
#define MT6359_RG_BUCK_DCM_MODE_ADDR                          \
	MT6359_BUCK_TOP_ELR0
#define MT6359_RG_BUCK_DCM_MODE_MASK                          0x1
#define MT6359_RG_BUCK_DCM_MODE_SHIFT                         10
#define MT6359_RG_BUCK_VPU_VOSEL_LIMIT_SEL_ADDR               \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VPU_VOSEL_LIMIT_SEL_MASK               0x3
#define MT6359_RG_BUCK_VPU_VOSEL_LIMIT_SEL_SHIFT              0
#define MT6359_RG_BUCK_VCORE_VOSEL_LIMIT_SEL_ADDR             \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VCORE_VOSEL_LIMIT_SEL_MASK             0x3
#define MT6359_RG_BUCK_VCORE_VOSEL_LIMIT_SEL_SHIFT            2
#define MT6359_RG_BUCK_VGPU11_VOSEL_LIMIT_SEL_ADDR            \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VGPU11_VOSEL_LIMIT_SEL_MASK            0x3
#define MT6359_RG_BUCK_VGPU11_VOSEL_LIMIT_SEL_SHIFT           4
#define MT6359_RG_BUCK_VGPU12_VOSEL_LIMIT_SEL_ADDR            \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VGPU12_VOSEL_LIMIT_SEL_MASK            0x3
#define MT6359_RG_BUCK_VGPU12_VOSEL_LIMIT_SEL_SHIFT           6
#define MT6359_RG_BUCK_VMODEM_VOSEL_LIMIT_SEL_ADDR            \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VMODEM_VOSEL_LIMIT_SEL_MASK            0x3
#define MT6359_RG_BUCK_VMODEM_VOSEL_LIMIT_SEL_SHIFT           8
#define MT6359_RG_BUCK_VPROC1_VOSEL_LIMIT_SEL_ADDR            \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VPROC1_VOSEL_LIMIT_SEL_MASK            0x3
#define MT6359_RG_BUCK_VPROC1_VOSEL_LIMIT_SEL_SHIFT           10
#define MT6359_RG_BUCK_VPROC2_VOSEL_LIMIT_SEL_ADDR            \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VPROC2_VOSEL_LIMIT_SEL_MASK            0x3
#define MT6359_RG_BUCK_VPROC2_VOSEL_LIMIT_SEL_SHIFT           12
#define MT6359_RG_BUCK_VS1_VOSEL_LIMIT_SEL_ADDR               \
	MT6359_BUCK_TOP_ELR1
#define MT6359_RG_BUCK_VS1_VOSEL_LIMIT_SEL_MASK               0x3
#define MT6359_RG_BUCK_VS1_VOSEL_LIMIT_SEL_SHIFT              14
#define MT6359_RG_BUCK_VS2_VOSEL_LIMIT_SEL_ADDR               \
	MT6359_BUCK_TOP_ELR2
#define MT6359_RG_BUCK_VS2_VOSEL_LIMIT_SEL_MASK               0x3
#define MT6359_RG_BUCK_VS2_VOSEL_LIMIT_SEL_SHIFT              0
#define MT6359_RG_BUCK_VPA_VOSEL_LIMIT_SEL_ADDR               \
	MT6359_BUCK_TOP_ELR2
#define MT6359_RG_BUCK_VPA_VOSEL_LIMIT_SEL_MASK               0x3
#define MT6359_RG_BUCK_VPA_VOSEL_LIMIT_SEL_SHIFT              2
#define MT6359_BUCK_VPU_ANA_ID_ADDR                           \
	MT6359_BUCK_VPU_DSN_ID
#define MT6359_BUCK_VPU_ANA_ID_MASK                           0xFF
#define MT6359_BUCK_VPU_ANA_ID_SHIFT                          0
#define MT6359_BUCK_VPU_DIG_ID_ADDR                           \
	MT6359_BUCK_VPU_DSN_ID
#define MT6359_BUCK_VPU_DIG_ID_MASK                           0xFF
#define MT6359_BUCK_VPU_DIG_ID_SHIFT                          8
#define MT6359_BUCK_VPU_ANA_MINOR_REV_ADDR                    \
	MT6359_BUCK_VPU_DSN_REV0
#define MT6359_BUCK_VPU_ANA_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VPU_ANA_MINOR_REV_SHIFT                   0
#define MT6359_BUCK_VPU_ANA_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VPU_DSN_REV0
#define MT6359_BUCK_VPU_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VPU_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_BUCK_VPU_DIG_MINOR_REV_ADDR                    \
	MT6359_BUCK_VPU_DSN_REV0
#define MT6359_BUCK_VPU_DIG_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VPU_DIG_MINOR_REV_SHIFT                   8
#define MT6359_BUCK_VPU_DIG_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VPU_DSN_REV0
#define MT6359_BUCK_VPU_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VPU_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_BUCK_VPU_DSN_CBS_ADDR                          \
	MT6359_BUCK_VPU_DSN_DBI
#define MT6359_BUCK_VPU_DSN_CBS_MASK                          0x3
#define MT6359_BUCK_VPU_DSN_CBS_SHIFT                         0
#define MT6359_BUCK_VPU_DSN_BIX_ADDR                          \
	MT6359_BUCK_VPU_DSN_DBI
#define MT6359_BUCK_VPU_DSN_BIX_MASK                          0x3
#define MT6359_BUCK_VPU_DSN_BIX_SHIFT                         2
#define MT6359_BUCK_VPU_DSN_ESP_ADDR                          \
	MT6359_BUCK_VPU_DSN_DBI
#define MT6359_BUCK_VPU_DSN_ESP_MASK                          0xFF
#define MT6359_BUCK_VPU_DSN_ESP_SHIFT                         8
#define MT6359_BUCK_VPU_DSN_FPI_SSHUB_ADDR                    \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_SSHUB_MASK                    0x1
#define MT6359_BUCK_VPU_DSN_FPI_SSHUB_SHIFT                   0
#define MT6359_BUCK_VPU_DSN_FPI_TRACKING_ADDR                 \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_TRACKING_MASK                 0x1
#define MT6359_BUCK_VPU_DSN_FPI_TRACKING_SHIFT                1
#define MT6359_BUCK_VPU_DSN_FPI_PREOC_ADDR                    \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_PREOC_MASK                    0x1
#define MT6359_BUCK_VPU_DSN_FPI_PREOC_SHIFT                   2
#define MT6359_BUCK_VPU_DSN_FPI_VOTER_ADDR                    \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_VOTER_MASK                    0x1
#define MT6359_BUCK_VPU_DSN_FPI_VOTER_SHIFT                   3
#define MT6359_BUCK_VPU_DSN_FPI_ULTRASONIC_ADDR               \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_ULTRASONIC_MASK               0x1
#define MT6359_BUCK_VPU_DSN_FPI_ULTRASONIC_SHIFT              4
#define MT6359_BUCK_VPU_DSN_FPI_DLC_ADDR                      \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_DLC_MASK                      0x1
#define MT6359_BUCK_VPU_DSN_FPI_DLC_SHIFT                     5
#define MT6359_BUCK_VPU_DSN_FPI_TRAP_ADDR                     \
	MT6359_BUCK_VPU_DSN_DXI
#define MT6359_BUCK_VPU_DSN_FPI_TRAP_MASK                     0x1
#define MT6359_BUCK_VPU_DSN_FPI_TRAP_SHIFT                    6
#define MT6359_RG_BUCK_VPU_EN_ADDR                            \
	MT6359_BUCK_VPU_CON0
#define MT6359_RG_BUCK_VPU_EN_MASK                            0x1
#define MT6359_RG_BUCK_VPU_EN_SHIFT                           0
#define MT6359_RG_BUCK_VPU_LP_ADDR                            \
	MT6359_BUCK_VPU_CON0
#define MT6359_RG_BUCK_VPU_LP_MASK                            0x1
#define MT6359_RG_BUCK_VPU_LP_SHIFT                           1
#define MT6359_RG_BUCK_VPU_CON0_SET_ADDR                      \
	MT6359_BUCK_VPU_CON0_SET
#define MT6359_RG_BUCK_VPU_CON0_SET_MASK                      0xFFFF
#define MT6359_RG_BUCK_VPU_CON0_SET_SHIFT                     0
#define MT6359_RG_BUCK_VPU_CON0_CLR_ADDR                      \
	MT6359_BUCK_VPU_CON0_CLR
#define MT6359_RG_BUCK_VPU_CON0_CLR_MASK                      0xFFFF
#define MT6359_RG_BUCK_VPU_CON0_CLR_SHIFT                     0
#define MT6359_RG_BUCK_VPU_VOSEL_SLEEP_ADDR                   \
	MT6359_BUCK_VPU_CON1
#define MT6359_RG_BUCK_VPU_VOSEL_SLEEP_MASK                   0x7F
#define MT6359_RG_BUCK_VPU_VOSEL_SLEEP_SHIFT                  0
#define MT6359_RG_BUCK_VPU_SELR2R_CTRL_ADDR                   \
	MT6359_BUCK_VPU_SLP_CON
#define MT6359_RG_BUCK_VPU_SELR2R_CTRL_MASK                   0x1
#define MT6359_RG_BUCK_VPU_SELR2R_CTRL_SHIFT                  0
#define MT6359_RG_BUCK_VPU_SFCHG_FRATE_ADDR                   \
	MT6359_BUCK_VPU_CFG0
#define MT6359_RG_BUCK_VPU_SFCHG_FRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VPU_SFCHG_FRATE_SHIFT                  0
#define MT6359_RG_BUCK_VPU_SFCHG_FEN_ADDR                     \
	MT6359_BUCK_VPU_CFG0
#define MT6359_RG_BUCK_VPU_SFCHG_FEN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_SFCHG_FEN_SHIFT                    7
#define MT6359_RG_BUCK_VPU_SFCHG_RRATE_ADDR                   \
	MT6359_BUCK_VPU_CFG0
#define MT6359_RG_BUCK_VPU_SFCHG_RRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VPU_SFCHG_RRATE_SHIFT                  8
#define MT6359_RG_BUCK_VPU_SFCHG_REN_ADDR                     \
	MT6359_BUCK_VPU_CFG0
#define MT6359_RG_BUCK_VPU_SFCHG_REN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_SFCHG_REN_SHIFT                    15
#define MT6359_RG_BUCK_VPU_HW0_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_BUCK_VPU_HW1_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_BUCK_VPU_HW2_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_BUCK_VPU_HW3_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_BUCK_VPU_HW4_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_BUCK_VPU_HW5_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_BUCK_VPU_HW6_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_BUCK_VPU_HW7_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_BUCK_VPU_HW8_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_BUCK_VPU_HW9_OP_EN_ADDR                     \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VPU_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_BUCK_VPU_HW10_OP_EN_ADDR                    \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_BUCK_VPU_HW11_OP_EN_ADDR                    \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_BUCK_VPU_HW12_OP_EN_ADDR                    \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_BUCK_VPU_HW13_OP_EN_ADDR                    \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_BUCK_VPU_HW14_OP_EN_ADDR                    \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_BUCK_VPU_SW_OP_EN_ADDR                      \
	MT6359_BUCK_VPU_OP_EN
#define MT6359_RG_BUCK_VPU_SW_OP_EN_MASK                      0x1
#define MT6359_RG_BUCK_VPU_SW_OP_EN_SHIFT                     15
#define MT6359_RG_BUCK_VPU_OP_EN_SET_ADDR                     \
	MT6359_BUCK_VPU_OP_EN_SET
#define MT6359_RG_BUCK_VPU_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_BUCK_VPU_OP_EN_SET_SHIFT                    0
#define MT6359_RG_BUCK_VPU_OP_EN_CLR_ADDR                     \
	MT6359_BUCK_VPU_OP_EN_CLR
#define MT6359_RG_BUCK_VPU_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_BUCK_VPU_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_BUCK_VPU_HW0_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_BUCK_VPU_HW1_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_BUCK_VPU_HW2_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_BUCK_VPU_HW3_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_BUCK_VPU_HW4_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_BUCK_VPU_HW5_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_BUCK_VPU_HW6_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_BUCK_VPU_HW7_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_BUCK_VPU_HW8_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_BUCK_VPU_HW9_OP_CFG_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VPU_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_BUCK_VPU_HW10_OP_CFG_ADDR                   \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_BUCK_VPU_HW11_OP_CFG_ADDR                   \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_BUCK_VPU_HW12_OP_CFG_ADDR                   \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_BUCK_VPU_HW13_OP_CFG_ADDR                   \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_BUCK_VPU_HW14_OP_CFG_ADDR                   \
	MT6359_BUCK_VPU_OP_CFG
#define MT6359_RG_BUCK_VPU_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_BUCK_VPU_OP_CFG_SET_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG_SET
#define MT6359_RG_BUCK_VPU_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_BUCK_VPU_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_BUCK_VPU_OP_CFG_CLR_ADDR                    \
	MT6359_BUCK_VPU_OP_CFG_CLR
#define MT6359_RG_BUCK_VPU_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_BUCK_VPU_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_BUCK_VPU_HW0_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW0_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW0_OP_MODE_SHIFT                  0
#define MT6359_RG_BUCK_VPU_HW1_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW1_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW1_OP_MODE_SHIFT                  1
#define MT6359_RG_BUCK_VPU_HW2_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW2_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW2_OP_MODE_SHIFT                  2
#define MT6359_RG_BUCK_VPU_HW3_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW3_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW3_OP_MODE_SHIFT                  3
#define MT6359_RG_BUCK_VPU_HW4_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW4_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW4_OP_MODE_SHIFT                  4
#define MT6359_RG_BUCK_VPU_HW5_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW5_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW5_OP_MODE_SHIFT                  5
#define MT6359_RG_BUCK_VPU_HW6_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW6_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW6_OP_MODE_SHIFT                  6
#define MT6359_RG_BUCK_VPU_HW7_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW7_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW7_OP_MODE_SHIFT                  7
#define MT6359_RG_BUCK_VPU_HW8_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW8_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW8_OP_MODE_SHIFT                  8
#define MT6359_RG_BUCK_VPU_HW9_OP_MODE_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW9_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VPU_HW9_OP_MODE_SHIFT                  9
#define MT6359_RG_BUCK_VPU_HW10_OP_MODE_ADDR                  \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW10_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VPU_HW10_OP_MODE_SHIFT                 10
#define MT6359_RG_BUCK_VPU_HW11_OP_MODE_ADDR                  \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW11_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VPU_HW11_OP_MODE_SHIFT                 11
#define MT6359_RG_BUCK_VPU_HW12_OP_MODE_ADDR                  \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW12_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VPU_HW12_OP_MODE_SHIFT                 12
#define MT6359_RG_BUCK_VPU_HW13_OP_MODE_ADDR                  \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW13_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VPU_HW13_OP_MODE_SHIFT                 13
#define MT6359_RG_BUCK_VPU_HW14_OP_MODE_ADDR                  \
	MT6359_BUCK_VPU_OP_MODE
#define MT6359_RG_BUCK_VPU_HW14_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VPU_HW14_OP_MODE_SHIFT                 14
#define MT6359_RG_BUCK_VPU_OP_MODE_SET_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE_SET
#define MT6359_RG_BUCK_VPU_OP_MODE_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPU_OP_MODE_SET_SHIFT                  0
#define MT6359_RG_BUCK_VPU_OP_MODE_CLR_ADDR                   \
	MT6359_BUCK_VPU_OP_MODE_CLR
#define MT6359_RG_BUCK_VPU_OP_MODE_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPU_OP_MODE_CLR_SHIFT                  0
#define MT6359_DA_VPU_VOSEL_ADDR                              \
	MT6359_BUCK_VPU_DBG0
#define MT6359_DA_VPU_VOSEL_MASK                              0x7F
#define MT6359_DA_VPU_VOSEL_SHIFT                             0
#define MT6359_DA_VPU_VOSEL_GRAY_ADDR                         \
	MT6359_BUCK_VPU_DBG0
#define MT6359_DA_VPU_VOSEL_GRAY_MASK                         0x7F
#define MT6359_DA_VPU_VOSEL_GRAY_SHIFT                        8
#define MT6359_DA_VPU_EN_ADDR                                 \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_EN_MASK                                 0x1
#define MT6359_DA_VPU_EN_SHIFT                                0
#define MT6359_DA_VPU_STB_ADDR                                \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_STB_MASK                                0x1
#define MT6359_DA_VPU_STB_SHIFT                               1
#define MT6359_DA_VPU_LOOP_SEL_ADDR                           \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_LOOP_SEL_MASK                           0x1
#define MT6359_DA_VPU_LOOP_SEL_SHIFT                          2
#define MT6359_DA_VPU_R2R_PDN_ADDR                            \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_R2R_PDN_MASK                            0x1
#define MT6359_DA_VPU_R2R_PDN_SHIFT                           3
#define MT6359_DA_VPU_DVS_EN_ADDR                             \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_DVS_EN_MASK                             0x1
#define MT6359_DA_VPU_DVS_EN_SHIFT                            4
#define MT6359_DA_VPU_DVS_DOWN_ADDR                           \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_DVS_DOWN_MASK                           0x1
#define MT6359_DA_VPU_DVS_DOWN_SHIFT                          5
#define MT6359_DA_VPU_SSH_ADDR                                \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_SSH_MASK                                0x1
#define MT6359_DA_VPU_SSH_SHIFT                               6
#define MT6359_DA_VPU_MINFREQ_DISCHARGE_ADDR                  \
	MT6359_BUCK_VPU_DBG1
#define MT6359_DA_VPU_MINFREQ_DISCHARGE_MASK                  0x1
#define MT6359_DA_VPU_MINFREQ_DISCHARGE_SHIFT                 8
#define MT6359_RG_BUCK_VPU_CK_SW_MODE_ADDR                    \
	MT6359_BUCK_VPU_DBG1
#define MT6359_RG_BUCK_VPU_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_BUCK_VPU_CK_SW_MODE_SHIFT                   12
#define MT6359_RG_BUCK_VPU_CK_SW_EN_ADDR                      \
	MT6359_BUCK_VPU_DBG1
#define MT6359_RG_BUCK_VPU_CK_SW_EN_MASK                      0x1
#define MT6359_RG_BUCK_VPU_CK_SW_EN_SHIFT                     13
#define MT6359_BUCK_VPU_ELR_LEN_ADDR                          \
	MT6359_BUCK_VPU_ELR_NUM
#define MT6359_BUCK_VPU_ELR_LEN_MASK                          0xFF
#define MT6359_BUCK_VPU_ELR_LEN_SHIFT                         0
#define MT6359_RG_BUCK_VPU_VOSEL_ADDR                         \
	MT6359_BUCK_VPU_ELR0
#define MT6359_RG_BUCK_VPU_VOSEL_MASK                         0x7F
#define MT6359_RG_BUCK_VPU_VOSEL_SHIFT                        0
#define MT6359_BUCK_VCORE_ANA_ID_ADDR                         \
	MT6359_BUCK_VCORE_DSN_ID
#define MT6359_BUCK_VCORE_ANA_ID_MASK                         0xFF
#define MT6359_BUCK_VCORE_ANA_ID_SHIFT                        0
#define MT6359_BUCK_VCORE_DIG_ID_ADDR                         \
	MT6359_BUCK_VCORE_DSN_ID
#define MT6359_BUCK_VCORE_DIG_ID_MASK                         0xFF
#define MT6359_BUCK_VCORE_DIG_ID_SHIFT                        8
#define MT6359_BUCK_VCORE_ANA_MINOR_REV_ADDR                  \
	MT6359_BUCK_VCORE_DSN_REV0
#define MT6359_BUCK_VCORE_ANA_MINOR_REV_MASK                  0xF
#define MT6359_BUCK_VCORE_ANA_MINOR_REV_SHIFT                 0
#define MT6359_BUCK_VCORE_ANA_MAJOR_REV_ADDR                  \
	MT6359_BUCK_VCORE_DSN_REV0
#define MT6359_BUCK_VCORE_ANA_MAJOR_REV_MASK                  0xF
#define MT6359_BUCK_VCORE_ANA_MAJOR_REV_SHIFT                 4
#define MT6359_BUCK_VCORE_DIG_MINOR_REV_ADDR                  \
	MT6359_BUCK_VCORE_DSN_REV0
#define MT6359_BUCK_VCORE_DIG_MINOR_REV_MASK                  0xF
#define MT6359_BUCK_VCORE_DIG_MINOR_REV_SHIFT                 8
#define MT6359_BUCK_VCORE_DIG_MAJOR_REV_ADDR                  \
	MT6359_BUCK_VCORE_DSN_REV0
#define MT6359_BUCK_VCORE_DIG_MAJOR_REV_MASK                  0xF
#define MT6359_BUCK_VCORE_DIG_MAJOR_REV_SHIFT                 12
#define MT6359_BUCK_VCORE_DSN_CBS_ADDR                        \
	MT6359_BUCK_VCORE_DSN_DBI
#define MT6359_BUCK_VCORE_DSN_CBS_MASK                        0x3
#define MT6359_BUCK_VCORE_DSN_CBS_SHIFT                       0
#define MT6359_BUCK_VCORE_DSN_BIX_ADDR                        \
	MT6359_BUCK_VCORE_DSN_DBI
#define MT6359_BUCK_VCORE_DSN_BIX_MASK                        0x3
#define MT6359_BUCK_VCORE_DSN_BIX_SHIFT                       2
#define MT6359_BUCK_VCORE_DSN_ESP_ADDR                        \
	MT6359_BUCK_VCORE_DSN_DBI
#define MT6359_BUCK_VCORE_DSN_ESP_MASK                        0xFF
#define MT6359_BUCK_VCORE_DSN_ESP_SHIFT                       8
#define MT6359_BUCK_VCORE_DSN_FPI_SSHUB_ADDR                  \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_SSHUB_MASK                  0x1
#define MT6359_BUCK_VCORE_DSN_FPI_SSHUB_SHIFT                 0
#define MT6359_BUCK_VCORE_DSN_FPI_TRACKING_ADDR               \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_TRACKING_MASK               0x1
#define MT6359_BUCK_VCORE_DSN_FPI_TRACKING_SHIFT              1
#define MT6359_BUCK_VCORE_DSN_FPI_PREOC_ADDR                  \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_PREOC_MASK                  0x1
#define MT6359_BUCK_VCORE_DSN_FPI_PREOC_SHIFT                 2
#define MT6359_BUCK_VCORE_DSN_FPI_VOTER_ADDR                  \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_VOTER_MASK                  0x1
#define MT6359_BUCK_VCORE_DSN_FPI_VOTER_SHIFT                 3
#define MT6359_BUCK_VCORE_DSN_FPI_ULTRASONIC_ADDR             \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_ULTRASONIC_MASK             0x1
#define MT6359_BUCK_VCORE_DSN_FPI_ULTRASONIC_SHIFT            4
#define MT6359_BUCK_VCORE_DSN_FPI_DLC_ADDR                    \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_DLC_MASK                    0x1
#define MT6359_BUCK_VCORE_DSN_FPI_DLC_SHIFT                   5
#define MT6359_BUCK_VCORE_DSN_FPI_TRAP_ADDR                   \
	MT6359_BUCK_VCORE_DSN_DXI
#define MT6359_BUCK_VCORE_DSN_FPI_TRAP_MASK                   0x1
#define MT6359_BUCK_VCORE_DSN_FPI_TRAP_SHIFT                  6
#define MT6359_RG_BUCK_VCORE_EN_ADDR                          \
	MT6359_BUCK_VCORE_CON0
#define MT6359_RG_BUCK_VCORE_EN_MASK                          0x1
#define MT6359_RG_BUCK_VCORE_EN_SHIFT                         0
#define MT6359_RG_BUCK_VCORE_LP_ADDR                          \
	MT6359_BUCK_VCORE_CON0
#define MT6359_RG_BUCK_VCORE_LP_MASK                          0x1
#define MT6359_RG_BUCK_VCORE_LP_SHIFT                         1
#define MT6359_RG_BUCK_VCORE_CON0_SET_ADDR                    \
	MT6359_BUCK_VCORE_CON0_SET
#define MT6359_RG_BUCK_VCORE_CON0_SET_MASK                    0xFFFF
#define MT6359_RG_BUCK_VCORE_CON0_SET_SHIFT                   0
#define MT6359_RG_BUCK_VCORE_CON0_CLR_ADDR                    \
	MT6359_BUCK_VCORE_CON0_CLR
#define MT6359_RG_BUCK_VCORE_CON0_CLR_MASK                    0xFFFF
#define MT6359_RG_BUCK_VCORE_CON0_CLR_SHIFT                   0
#define MT6359_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR                 \
	MT6359_BUCK_VCORE_CON1
#define MT6359_RG_BUCK_VCORE_VOSEL_SLEEP_MASK                 0x7F
#define MT6359_RG_BUCK_VCORE_VOSEL_SLEEP_SHIFT                0
#define MT6359_RG_BUCK_VCORE_SELR2R_CTRL_ADDR                 \
	MT6359_BUCK_VCORE_SLP_CON
#define MT6359_RG_BUCK_VCORE_SELR2R_CTRL_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_SELR2R_CTRL_SHIFT                0
#define MT6359_RG_BUCK_VCORE_SFCHG_FRATE_ADDR                 \
	MT6359_BUCK_VCORE_CFG0
#define MT6359_RG_BUCK_VCORE_SFCHG_FRATE_MASK                 0x7F
#define MT6359_RG_BUCK_VCORE_SFCHG_FRATE_SHIFT                0
#define MT6359_RG_BUCK_VCORE_SFCHG_FEN_ADDR                   \
	MT6359_BUCK_VCORE_CFG0
#define MT6359_RG_BUCK_VCORE_SFCHG_FEN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_SFCHG_FEN_SHIFT                  7
#define MT6359_RG_BUCK_VCORE_SFCHG_RRATE_ADDR                 \
	MT6359_BUCK_VCORE_CFG0
#define MT6359_RG_BUCK_VCORE_SFCHG_RRATE_MASK                 0x7F
#define MT6359_RG_BUCK_VCORE_SFCHG_RRATE_SHIFT                8
#define MT6359_RG_BUCK_VCORE_SFCHG_REN_ADDR                   \
	MT6359_BUCK_VCORE_CFG0
#define MT6359_RG_BUCK_VCORE_SFCHG_REN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_SFCHG_REN_SHIFT                  15
#define MT6359_RG_BUCK_VCORE_HW0_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_BUCK_VCORE_HW1_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_BUCK_VCORE_HW2_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_BUCK_VCORE_HW3_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_BUCK_VCORE_HW4_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_BUCK_VCORE_HW5_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_BUCK_VCORE_HW6_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_BUCK_VCORE_HW7_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_BUCK_VCORE_HW8_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_BUCK_VCORE_HW9_OP_EN_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VCORE_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_BUCK_VCORE_HW10_OP_EN_ADDR                  \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_BUCK_VCORE_HW11_OP_EN_ADDR                  \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_BUCK_VCORE_HW12_OP_EN_ADDR                  \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_BUCK_VCORE_HW13_OP_EN_ADDR                  \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_BUCK_VCORE_HW14_OP_EN_ADDR                  \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_BUCK_VCORE_SW_OP_EN_ADDR                    \
	MT6359_BUCK_VCORE_OP_EN
#define MT6359_RG_BUCK_VCORE_SW_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VCORE_SW_OP_EN_SHIFT                   15
#define MT6359_RG_BUCK_VCORE_OP_EN_SET_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN_SET
#define MT6359_RG_BUCK_VCORE_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_EN_SET_SHIFT                  0
#define MT6359_RG_BUCK_VCORE_OP_EN_CLR_ADDR                   \
	MT6359_BUCK_VCORE_OP_EN_CLR
#define MT6359_RG_BUCK_VCORE_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VCORE_HW0_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_BUCK_VCORE_HW1_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_BUCK_VCORE_HW2_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_BUCK_VCORE_HW3_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_BUCK_VCORE_HW4_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_BUCK_VCORE_HW5_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_BUCK_VCORE_HW6_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_BUCK_VCORE_HW7_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_BUCK_VCORE_HW8_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_BUCK_VCORE_HW9_OP_CFG_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_BUCK_VCORE_HW10_OP_CFG_ADDR                 \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_BUCK_VCORE_HW11_OP_CFG_ADDR                 \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_BUCK_VCORE_HW12_OP_CFG_ADDR                 \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_BUCK_VCORE_HW13_OP_CFG_ADDR                 \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_BUCK_VCORE_HW14_OP_CFG_ADDR                 \
	MT6359_BUCK_VCORE_OP_CFG
#define MT6359_RG_BUCK_VCORE_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_BUCK_VCORE_OP_CFG_SET_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG_SET
#define MT6359_RG_BUCK_VCORE_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_BUCK_VCORE_OP_CFG_CLR_ADDR                  \
	MT6359_BUCK_VCORE_OP_CFG_CLR
#define MT6359_RG_BUCK_VCORE_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VCORE_HW0_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW0_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW0_OP_MODE_SHIFT                0
#define MT6359_RG_BUCK_VCORE_HW1_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW1_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW1_OP_MODE_SHIFT                1
#define MT6359_RG_BUCK_VCORE_HW2_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW2_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW2_OP_MODE_SHIFT                2
#define MT6359_RG_BUCK_VCORE_HW3_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW3_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW3_OP_MODE_SHIFT                3
#define MT6359_RG_BUCK_VCORE_HW4_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW4_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW4_OP_MODE_SHIFT                4
#define MT6359_RG_BUCK_VCORE_HW5_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW5_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW5_OP_MODE_SHIFT                5
#define MT6359_RG_BUCK_VCORE_HW6_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW6_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW6_OP_MODE_SHIFT                6
#define MT6359_RG_BUCK_VCORE_HW7_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW7_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW7_OP_MODE_SHIFT                7
#define MT6359_RG_BUCK_VCORE_HW8_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW8_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW8_OP_MODE_SHIFT                8
#define MT6359_RG_BUCK_VCORE_HW9_OP_MODE_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW9_OP_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VCORE_HW9_OP_MODE_SHIFT                9
#define MT6359_RG_BUCK_VCORE_HW10_OP_MODE_ADDR                \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW10_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VCORE_HW10_OP_MODE_SHIFT               10
#define MT6359_RG_BUCK_VCORE_HW11_OP_MODE_ADDR                \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW11_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VCORE_HW11_OP_MODE_SHIFT               11
#define MT6359_RG_BUCK_VCORE_HW12_OP_MODE_ADDR                \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW12_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VCORE_HW12_OP_MODE_SHIFT               12
#define MT6359_RG_BUCK_VCORE_HW13_OP_MODE_ADDR                \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW13_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VCORE_HW13_OP_MODE_SHIFT               13
#define MT6359_RG_BUCK_VCORE_HW14_OP_MODE_ADDR                \
	MT6359_BUCK_VCORE_OP_MODE
#define MT6359_RG_BUCK_VCORE_HW14_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VCORE_HW14_OP_MODE_SHIFT               14
#define MT6359_RG_BUCK_VCORE_OP_MODE_SET_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE_SET
#define MT6359_RG_BUCK_VCORE_OP_MODE_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_MODE_SET_SHIFT                0
#define MT6359_RG_BUCK_VCORE_OP_MODE_CLR_ADDR                 \
	MT6359_BUCK_VCORE_OP_MODE_CLR
#define MT6359_RG_BUCK_VCORE_OP_MODE_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VCORE_OP_MODE_CLR_SHIFT                0
#define MT6359_DA_VCORE_VOSEL_ADDR                            \
	MT6359_BUCK_VCORE_DBG0
#define MT6359_DA_VCORE_VOSEL_MASK                            0x7F
#define MT6359_DA_VCORE_VOSEL_SHIFT                           0
#define MT6359_DA_VCORE_VOSEL_GRAY_ADDR                       \
	MT6359_BUCK_VCORE_DBG0
#define MT6359_DA_VCORE_VOSEL_GRAY_MASK                       0x7F
#define MT6359_DA_VCORE_VOSEL_GRAY_SHIFT                      8
#define MT6359_DA_VCORE_EN_ADDR                               \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_EN_MASK                               0x1
#define MT6359_DA_VCORE_EN_SHIFT                              0
#define MT6359_DA_VCORE_STB_ADDR                              \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_STB_MASK                              0x1
#define MT6359_DA_VCORE_STB_SHIFT                             1
#define MT6359_DA_VCORE_LOOP_SEL_ADDR                         \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_LOOP_SEL_MASK                         0x1
#define MT6359_DA_VCORE_LOOP_SEL_SHIFT                        2
#define MT6359_DA_VCORE_R2R_PDN_ADDR                          \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_R2R_PDN_MASK                          0x1
#define MT6359_DA_VCORE_R2R_PDN_SHIFT                         3
#define MT6359_DA_VCORE_DVS_EN_ADDR                           \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_DVS_EN_MASK                           0x1
#define MT6359_DA_VCORE_DVS_EN_SHIFT                          4
#define MT6359_DA_VCORE_DVS_DOWN_ADDR                         \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_DVS_DOWN_MASK                         0x1
#define MT6359_DA_VCORE_DVS_DOWN_SHIFT                        5
#define MT6359_DA_VCORE_SSH_ADDR                              \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_SSH_MASK                              0x1
#define MT6359_DA_VCORE_SSH_SHIFT                             6
#define MT6359_DA_VCORE_MINFREQ_DISCHARGE_ADDR                \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_DA_VCORE_MINFREQ_DISCHARGE_MASK                0x1
#define MT6359_DA_VCORE_MINFREQ_DISCHARGE_SHIFT               8
#define MT6359_RG_BUCK_VCORE_CK_SW_MODE_ADDR                  \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_RG_BUCK_VCORE_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VCORE_CK_SW_MODE_SHIFT                 12
#define MT6359_RG_BUCK_VCORE_CK_SW_EN_ADDR                    \
	MT6359_BUCK_VCORE_DBG1
#define MT6359_RG_BUCK_VCORE_CK_SW_EN_MASK                    0x1
#define MT6359_RG_BUCK_VCORE_CK_SW_EN_SHIFT                   13
#define MT6359_RG_BUCK_VCORE_SSHUB_EN_ADDR                    \
	MT6359_BUCK_VCORE_SSHUB_CON0
#define MT6359_RG_BUCK_VCORE_SSHUB_EN_MASK                    0x1
#define MT6359_RG_BUCK_VCORE_SSHUB_EN_SHIFT                   0
#define MT6359_RG_BUCK_VCORE_SSHUB_VOSEL_ADDR                 \
	MT6359_BUCK_VCORE_SSHUB_CON0
#define MT6359_RG_BUCK_VCORE_SSHUB_VOSEL_MASK                 0x7F
#define MT6359_RG_BUCK_VCORE_SSHUB_VOSEL_SHIFT                4
#define MT6359_RG_BUCK_VCORE_SPI_EN_ADDR                      \
	MT6359_BUCK_VCORE_SPI_CON0
#define MT6359_RG_BUCK_VCORE_SPI_EN_MASK                      0x1
#define MT6359_RG_BUCK_VCORE_SPI_EN_SHIFT                     0
#define MT6359_RG_BUCK_VCORE_SPI_VOSEL_ADDR                   \
	MT6359_BUCK_VCORE_SPI_CON0
#define MT6359_RG_BUCK_VCORE_SPI_VOSEL_MASK                   0x7F
#define MT6359_RG_BUCK_VCORE_SPI_VOSEL_SHIFT                  4
#define MT6359_RG_BUCK_VCORE_BT_LP_EN_ADDR                    \
	MT6359_BUCK_VCORE_BT_LP_CON0
#define MT6359_RG_BUCK_VCORE_BT_LP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VCORE_BT_LP_EN_SHIFT                   0
#define MT6359_RG_BUCK_VCORE_BT_LP_VOSEL_ADDR                 \
	MT6359_BUCK_VCORE_BT_LP_CON0
#define MT6359_RG_BUCK_VCORE_BT_LP_VOSEL_MASK                 0x7F
#define MT6359_RG_BUCK_VCORE_BT_LP_VOSEL_SHIFT                4
#define MT6359_RG_BUCK_VCORE_TRACK_STALL_BYPASS_ADDR          \
	MT6359_BUCK_VCORE_STALL_TRACK0
#define MT6359_RG_BUCK_VCORE_TRACK_STALL_BYPASS_MASK          0x1
#define MT6359_RG_BUCK_VCORE_TRACK_STALL_BYPASS_SHIFT         0
#define MT6359_BUCK_VCORE_ELR_LEN_ADDR                        \
	MT6359_BUCK_VCORE_ELR_NUM
#define MT6359_BUCK_VCORE_ELR_LEN_MASK                        0xFF
#define MT6359_BUCK_VCORE_ELR_LEN_SHIFT                       0
#define MT6359_RG_BUCK_VCORE_VOSEL_ADDR                       \
	MT6359_BUCK_VCORE_ELR0
#define MT6359_RG_BUCK_VCORE_VOSEL_MASK                       0x7F
#define MT6359_RG_BUCK_VCORE_VOSEL_SHIFT                      0
#define MT6359_BUCK_VGPU11_ANA_ID_ADDR                        \
	MT6359_BUCK_VGPU11_DSN_ID
#define MT6359_BUCK_VGPU11_ANA_ID_MASK                        0xFF
#define MT6359_BUCK_VGPU11_ANA_ID_SHIFT                       0
#define MT6359_BUCK_VGPU11_DIG_ID_ADDR                        \
	MT6359_BUCK_VGPU11_DSN_ID
#define MT6359_BUCK_VGPU11_DIG_ID_MASK                        0xFF
#define MT6359_BUCK_VGPU11_DIG_ID_SHIFT                       8
#define MT6359_BUCK_VGPU11_ANA_MINOR_REV_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_REV0
#define MT6359_BUCK_VGPU11_ANA_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU11_ANA_MINOR_REV_SHIFT                0
#define MT6359_BUCK_VGPU11_ANA_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_REV0
#define MT6359_BUCK_VGPU11_ANA_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU11_ANA_MAJOR_REV_SHIFT                4
#define MT6359_BUCK_VGPU11_DIG_MINOR_REV_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_REV0
#define MT6359_BUCK_VGPU11_DIG_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU11_DIG_MINOR_REV_SHIFT                8
#define MT6359_BUCK_VGPU11_DIG_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_REV0
#define MT6359_BUCK_VGPU11_DIG_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU11_DIG_MAJOR_REV_SHIFT                12
#define MT6359_BUCK_VGPU11_DSN_CBS_ADDR                       \
	MT6359_BUCK_VGPU11_DSN_DBI
#define MT6359_BUCK_VGPU11_DSN_CBS_MASK                       0x3
#define MT6359_BUCK_VGPU11_DSN_CBS_SHIFT                      0
#define MT6359_BUCK_VGPU11_DSN_BIX_ADDR                       \
	MT6359_BUCK_VGPU11_DSN_DBI
#define MT6359_BUCK_VGPU11_DSN_BIX_MASK                       0x3
#define MT6359_BUCK_VGPU11_DSN_BIX_SHIFT                      2
#define MT6359_BUCK_VGPU11_DSN_ESP_ADDR                       \
	MT6359_BUCK_VGPU11_DSN_DBI
#define MT6359_BUCK_VGPU11_DSN_ESP_MASK                       0xFF
#define MT6359_BUCK_VGPU11_DSN_ESP_SHIFT                      8
#define MT6359_BUCK_VGPU11_DSN_FPI_SSHUB_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_SSHUB_MASK                 0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_SSHUB_SHIFT                0
#define MT6359_BUCK_VGPU11_DSN_FPI_TRACKING_ADDR              \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_TRACKING_MASK              0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_TRACKING_SHIFT             1
#define MT6359_BUCK_VGPU11_DSN_FPI_PREOC_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_PREOC_MASK                 0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_PREOC_SHIFT                2
#define MT6359_BUCK_VGPU11_DSN_FPI_VOTER_ADDR                 \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_VOTER_MASK                 0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_VOTER_SHIFT                3
#define MT6359_BUCK_VGPU11_DSN_FPI_ULTRASONIC_ADDR            \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_ULTRASONIC_MASK            0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_ULTRASONIC_SHIFT           4
#define MT6359_BUCK_VGPU11_DSN_FPI_DLC_ADDR                   \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_DLC_MASK                   0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_DLC_SHIFT                  5
#define MT6359_BUCK_VGPU11_DSN_FPI_TRAP_ADDR                  \
	MT6359_BUCK_VGPU11_DSN_DXI
#define MT6359_BUCK_VGPU11_DSN_FPI_TRAP_MASK                  0x1
#define MT6359_BUCK_VGPU11_DSN_FPI_TRAP_SHIFT                 6
#define MT6359_RG_BUCK_VGPU11_EN_ADDR                         \
	MT6359_BUCK_VGPU11_CON0
#define MT6359_RG_BUCK_VGPU11_EN_MASK                         0x1
#define MT6359_RG_BUCK_VGPU11_EN_SHIFT                        0
#define MT6359_RG_BUCK_VGPU11_LP_ADDR                         \
	MT6359_BUCK_VGPU11_CON0
#define MT6359_RG_BUCK_VGPU11_LP_MASK                         0x1
#define MT6359_RG_BUCK_VGPU11_LP_SHIFT                        1
#define MT6359_RG_BUCK_VGPU11_CON0_SET_ADDR                   \
	MT6359_BUCK_VGPU11_CON0_SET
#define MT6359_RG_BUCK_VGPU11_CON0_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VGPU11_CON0_SET_SHIFT                  0
#define MT6359_RG_BUCK_VGPU11_CON0_CLR_ADDR                   \
	MT6359_BUCK_VGPU11_CON0_CLR
#define MT6359_RG_BUCK_VGPU11_CON0_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VGPU11_CON0_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VGPU11_VOSEL_SLEEP_ADDR                \
	MT6359_BUCK_VGPU11_CON1
#define MT6359_RG_BUCK_VGPU11_VOSEL_SLEEP_MASK                0x7F
#define MT6359_RG_BUCK_VGPU11_VOSEL_SLEEP_SHIFT               0
#define MT6359_RG_BUCK_VGPU11_SELR2R_CTRL_ADDR                \
	MT6359_BUCK_VGPU11_SLP_CON
#define MT6359_RG_BUCK_VGPU11_SELR2R_CTRL_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_SELR2R_CTRL_SHIFT               0
#define MT6359_RG_BUCK_VGPU11_SFCHG_FRATE_ADDR                \
	MT6359_BUCK_VGPU11_CFG0
#define MT6359_RG_BUCK_VGPU11_SFCHG_FRATE_MASK                0x7F
#define MT6359_RG_BUCK_VGPU11_SFCHG_FRATE_SHIFT               0
#define MT6359_RG_BUCK_VGPU11_SFCHG_FEN_ADDR                  \
	MT6359_BUCK_VGPU11_CFG0
#define MT6359_RG_BUCK_VGPU11_SFCHG_FEN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_SFCHG_FEN_SHIFT                 7
#define MT6359_RG_BUCK_VGPU11_SFCHG_RRATE_ADDR                \
	MT6359_BUCK_VGPU11_CFG0
#define MT6359_RG_BUCK_VGPU11_SFCHG_RRATE_MASK                0x7F
#define MT6359_RG_BUCK_VGPU11_SFCHG_RRATE_SHIFT               8
#define MT6359_RG_BUCK_VGPU11_SFCHG_REN_ADDR                  \
	MT6359_BUCK_VGPU11_CFG0
#define MT6359_RG_BUCK_VGPU11_SFCHG_REN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_SFCHG_REN_SHIFT                 15
#define MT6359_RG_BUCK_VGPU11_HW0_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_BUCK_VGPU11_HW1_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_BUCK_VGPU11_HW3_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_BUCK_VGPU11_HW4_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_BUCK_VGPU11_HW5_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_BUCK_VGPU11_HW6_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_BUCK_VGPU11_HW7_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_BUCK_VGPU11_HW8_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_BUCK_VGPU11_HW9_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU11_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_BUCK_VGPU11_HW10_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW10_OP_EN_SHIFT                10
#define MT6359_RG_BUCK_VGPU11_HW11_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW11_OP_EN_SHIFT                11
#define MT6359_RG_BUCK_VGPU11_HW12_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW12_OP_EN_SHIFT                12
#define MT6359_RG_BUCK_VGPU11_HW13_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW13_OP_EN_SHIFT                13
#define MT6359_RG_BUCK_VGPU11_HW14_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW14_OP_EN_SHIFT                14
#define MT6359_RG_BUCK_VGPU11_SW_OP_EN_ADDR                   \
	MT6359_BUCK_VGPU11_OP_EN
#define MT6359_RG_BUCK_VGPU11_SW_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VGPU11_SW_OP_EN_SHIFT                  15
#define MT6359_RG_BUCK_VGPU11_OP_EN_SET_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN_SET
#define MT6359_RG_BUCK_VGPU11_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VGPU11_OP_EN_CLR_ADDR                  \
	MT6359_BUCK_VGPU11_OP_EN_CLR
#define MT6359_RG_BUCK_VGPU11_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VGPU11_HW0_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_BUCK_VGPU11_HW1_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_BUCK_VGPU11_HW3_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_BUCK_VGPU11_HW4_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_BUCK_VGPU11_HW5_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_BUCK_VGPU11_HW6_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_BUCK_VGPU11_HW7_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_BUCK_VGPU11_HW8_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_BUCK_VGPU11_HW9_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_BUCK_VGPU11_HW10_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_BUCK_VGPU11_HW11_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_BUCK_VGPU11_HW12_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_BUCK_VGPU11_HW13_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_BUCK_VGPU11_HW14_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU11_OP_CFG
#define MT6359_RG_BUCK_VGPU11_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_BUCK_VGPU11_OP_CFG_SET_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG_SET
#define MT6359_RG_BUCK_VGPU11_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_CFG_SET_SHIFT                0
#define MT6359_RG_BUCK_VGPU11_OP_CFG_CLR_ADDR                 \
	MT6359_BUCK_VGPU11_OP_CFG_CLR
#define MT6359_RG_BUCK_VGPU11_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_BUCK_VGPU11_HW0_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW0_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW0_OP_MODE_SHIFT               0
#define MT6359_RG_BUCK_VGPU11_HW1_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW1_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW1_OP_MODE_SHIFT               1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW2_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW2_OP_MODE_SHIFT               2
#define MT6359_RG_BUCK_VGPU11_HW3_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW3_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW3_OP_MODE_SHIFT               3
#define MT6359_RG_BUCK_VGPU11_HW4_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW4_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW4_OP_MODE_SHIFT               4
#define MT6359_RG_BUCK_VGPU11_HW5_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW5_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW5_OP_MODE_SHIFT               5
#define MT6359_RG_BUCK_VGPU11_HW6_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW6_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW6_OP_MODE_SHIFT               6
#define MT6359_RG_BUCK_VGPU11_HW7_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW7_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW7_OP_MODE_SHIFT               7
#define MT6359_RG_BUCK_VGPU11_HW8_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW8_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW8_OP_MODE_SHIFT               8
#define MT6359_RG_BUCK_VGPU11_HW9_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW9_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU11_HW9_OP_MODE_SHIFT               9
#define MT6359_RG_BUCK_VGPU11_HW10_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW10_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU11_HW10_OP_MODE_SHIFT              10
#define MT6359_RG_BUCK_VGPU11_HW11_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW11_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU11_HW11_OP_MODE_SHIFT              11
#define MT6359_RG_BUCK_VGPU11_HW12_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW12_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU11_HW12_OP_MODE_SHIFT              12
#define MT6359_RG_BUCK_VGPU11_HW13_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW13_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU11_HW13_OP_MODE_SHIFT              13
#define MT6359_RG_BUCK_VGPU11_HW14_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU11_OP_MODE
#define MT6359_RG_BUCK_VGPU11_HW14_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU11_HW14_OP_MODE_SHIFT              14
#define MT6359_RG_BUCK_VGPU11_OP_MODE_SET_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE_SET
#define MT6359_RG_BUCK_VGPU11_OP_MODE_SET_MASK                0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_MODE_SET_SHIFT               0
#define MT6359_RG_BUCK_VGPU11_OP_MODE_CLR_ADDR                \
	MT6359_BUCK_VGPU11_OP_MODE_CLR
#define MT6359_RG_BUCK_VGPU11_OP_MODE_CLR_MASK                0xFFFF
#define MT6359_RG_BUCK_VGPU11_OP_MODE_CLR_SHIFT               0
#define MT6359_DA_VGPU11_VOSEL_ADDR                           \
	MT6359_BUCK_VGPU11_DBG0
#define MT6359_DA_VGPU11_VOSEL_MASK                           0x7F
#define MT6359_DA_VGPU11_VOSEL_SHIFT                          0
#define MT6359_DA_VGPU11_VOSEL_GRAY_ADDR                      \
	MT6359_BUCK_VGPU11_DBG0
#define MT6359_DA_VGPU11_VOSEL_GRAY_MASK                      0x7F
#define MT6359_DA_VGPU11_VOSEL_GRAY_SHIFT                     8
#define MT6359_DA_VGPU11_EN_ADDR                              \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_EN_MASK                              0x1
#define MT6359_DA_VGPU11_EN_SHIFT                             0
#define MT6359_DA_VGPU11_STB_ADDR                             \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_STB_MASK                             0x1
#define MT6359_DA_VGPU11_STB_SHIFT                            1
#define MT6359_DA_VGPU11_LOOP_SEL_ADDR                        \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_LOOP_SEL_MASK                        0x1
#define MT6359_DA_VGPU11_LOOP_SEL_SHIFT                       2
#define MT6359_DA_VGPU11_R2R_PDN_ADDR                         \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_R2R_PDN_MASK                         0x1
#define MT6359_DA_VGPU11_R2R_PDN_SHIFT                        3
#define MT6359_DA_VGPU11_DVS_EN_ADDR                          \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_DVS_EN_MASK                          0x1
#define MT6359_DA_VGPU11_DVS_EN_SHIFT                         4
#define MT6359_DA_VGPU11_DVS_DOWN_ADDR                        \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_DVS_DOWN_MASK                        0x1
#define MT6359_DA_VGPU11_DVS_DOWN_SHIFT                       5
#define MT6359_DA_VGPU11_SSH_ADDR                             \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_SSH_MASK                             0x1
#define MT6359_DA_VGPU11_SSH_SHIFT                            6
#define MT6359_DA_VGPU11_MINFREQ_DISCHARGE_ADDR               \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_DA_VGPU11_MINFREQ_DISCHARGE_MASK               0x1
#define MT6359_DA_VGPU11_MINFREQ_DISCHARGE_SHIFT              8
#define MT6359_RG_BUCK_VGPU11_CK_SW_MODE_ADDR                 \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_RG_BUCK_VGPU11_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VGPU11_CK_SW_MODE_SHIFT                12
#define MT6359_RG_BUCK_VGPU11_CK_SW_EN_ADDR                   \
	MT6359_BUCK_VGPU11_DBG1
#define MT6359_RG_BUCK_VGPU11_CK_SW_EN_MASK                   0x1
#define MT6359_RG_BUCK_VGPU11_CK_SW_EN_SHIFT                  13
#define MT6359_BUCK_VGPU11_ELR_LEN_ADDR                       \
	MT6359_BUCK_VGPU11_ELR_NUM
#define MT6359_BUCK_VGPU11_ELR_LEN_MASK                       0xFF
#define MT6359_BUCK_VGPU11_ELR_LEN_SHIFT                      0
#define MT6359_RG_BUCK_VGPU11_VOSEL_ADDR                      \
	MT6359_BUCK_VGPU11_ELR0
#define MT6359_RG_BUCK_VGPU11_VOSEL_MASK                      0x7F
#define MT6359_RG_BUCK_VGPU11_VOSEL_SHIFT                     0
#define MT6359_BUCK_VGPU12_ANA_ID_ADDR                        \
	MT6359_BUCK_VGPU12_DSN_ID
#define MT6359_BUCK_VGPU12_ANA_ID_MASK                        0xFF
#define MT6359_BUCK_VGPU12_ANA_ID_SHIFT                       0
#define MT6359_BUCK_VGPU12_DIG_ID_ADDR                        \
	MT6359_BUCK_VGPU12_DSN_ID
#define MT6359_BUCK_VGPU12_DIG_ID_MASK                        0xFF
#define MT6359_BUCK_VGPU12_DIG_ID_SHIFT                       8
#define MT6359_BUCK_VGPU12_ANA_MINOR_REV_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_REV0
#define MT6359_BUCK_VGPU12_ANA_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU12_ANA_MINOR_REV_SHIFT                0
#define MT6359_BUCK_VGPU12_ANA_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_REV0
#define MT6359_BUCK_VGPU12_ANA_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU12_ANA_MAJOR_REV_SHIFT                4
#define MT6359_BUCK_VGPU12_DIG_MINOR_REV_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_REV0
#define MT6359_BUCK_VGPU12_DIG_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU12_DIG_MINOR_REV_SHIFT                8
#define MT6359_BUCK_VGPU12_DIG_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_REV0
#define MT6359_BUCK_VGPU12_DIG_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VGPU12_DIG_MAJOR_REV_SHIFT                12
#define MT6359_BUCK_VGPU12_DSN_CBS_ADDR                       \
	MT6359_BUCK_VGPU12_DSN_DBI
#define MT6359_BUCK_VGPU12_DSN_CBS_MASK                       0x3
#define MT6359_BUCK_VGPU12_DSN_CBS_SHIFT                      0
#define MT6359_BUCK_VGPU12_DSN_BIX_ADDR                       \
	MT6359_BUCK_VGPU12_DSN_DBI
#define MT6359_BUCK_VGPU12_DSN_BIX_MASK                       0x3
#define MT6359_BUCK_VGPU12_DSN_BIX_SHIFT                      2
#define MT6359_BUCK_VGPU12_DSN_ESP_ADDR                       \
	MT6359_BUCK_VGPU12_DSN_DBI
#define MT6359_BUCK_VGPU12_DSN_ESP_MASK                       0xFF
#define MT6359_BUCK_VGPU12_DSN_ESP_SHIFT                      8
#define MT6359_BUCK_VGPU12_DSN_FPI_SSHUB_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_SSHUB_MASK                 0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_SSHUB_SHIFT                0
#define MT6359_BUCK_VGPU12_DSN_FPI_TRACKING_ADDR              \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_TRACKING_MASK              0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_TRACKING_SHIFT             1
#define MT6359_BUCK_VGPU12_DSN_FPI_PREOC_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_PREOC_MASK                 0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_PREOC_SHIFT                2
#define MT6359_BUCK_VGPU12_DSN_FPI_VOTER_ADDR                 \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_VOTER_MASK                 0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_VOTER_SHIFT                3
#define MT6359_BUCK_VGPU12_DSN_FPI_ULTRASONIC_ADDR            \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_ULTRASONIC_MASK            0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_ULTRASONIC_SHIFT           4
#define MT6359_BUCK_VGPU12_DSN_FPI_DLC_ADDR                   \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_DLC_MASK                   0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_DLC_SHIFT                  5
#define MT6359_BUCK_VGPU12_DSN_FPI_TRAP_ADDR                  \
	MT6359_BUCK_VGPU12_DSN_DXI
#define MT6359_BUCK_VGPU12_DSN_FPI_TRAP_MASK                  0x1
#define MT6359_BUCK_VGPU12_DSN_FPI_TRAP_SHIFT                 6
#define MT6359_RG_BUCK_VGPU12_EN_ADDR                         \
	MT6359_BUCK_VGPU12_CON0
#define MT6359_RG_BUCK_VGPU12_EN_MASK                         0x1
#define MT6359_RG_BUCK_VGPU12_EN_SHIFT                        0
#define MT6359_RG_BUCK_VGPU12_LP_ADDR                         \
	MT6359_BUCK_VGPU12_CON0
#define MT6359_RG_BUCK_VGPU12_LP_MASK                         0x1
#define MT6359_RG_BUCK_VGPU12_LP_SHIFT                        1
#define MT6359_RG_BUCK_VGPU12_CON0_SET_ADDR                   \
	MT6359_BUCK_VGPU12_CON0_SET
#define MT6359_RG_BUCK_VGPU12_CON0_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VGPU12_CON0_SET_SHIFT                  0
#define MT6359_RG_BUCK_VGPU12_CON0_CLR_ADDR                   \
	MT6359_BUCK_VGPU12_CON0_CLR
#define MT6359_RG_BUCK_VGPU12_CON0_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VGPU12_CON0_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VGPU12_VOSEL_SLEEP_ADDR                \
	MT6359_BUCK_VGPU12_CON1
#define MT6359_RG_BUCK_VGPU12_VOSEL_SLEEP_MASK                0x7F
#define MT6359_RG_BUCK_VGPU12_VOSEL_SLEEP_SHIFT               0
#define MT6359_RG_BUCK_VGPU12_SELR2R_CTRL_ADDR                \
	MT6359_BUCK_VGPU12_SLP_CON
#define MT6359_RG_BUCK_VGPU12_SELR2R_CTRL_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_SELR2R_CTRL_SHIFT               0
#define MT6359_RG_BUCK_VGPU12_SFCHG_FRATE_ADDR                \
	MT6359_BUCK_VGPU12_CFG0
#define MT6359_RG_BUCK_VGPU12_SFCHG_FRATE_MASK                0x7F
#define MT6359_RG_BUCK_VGPU12_SFCHG_FRATE_SHIFT               0
#define MT6359_RG_BUCK_VGPU12_SFCHG_FEN_ADDR                  \
	MT6359_BUCK_VGPU12_CFG0
#define MT6359_RG_BUCK_VGPU12_SFCHG_FEN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_SFCHG_FEN_SHIFT                 7
#define MT6359_RG_BUCK_VGPU12_SFCHG_RRATE_ADDR                \
	MT6359_BUCK_VGPU12_CFG0
#define MT6359_RG_BUCK_VGPU12_SFCHG_RRATE_MASK                0x7F
#define MT6359_RG_BUCK_VGPU12_SFCHG_RRATE_SHIFT               8
#define MT6359_RG_BUCK_VGPU12_SFCHG_REN_ADDR                  \
	MT6359_BUCK_VGPU12_CFG0
#define MT6359_RG_BUCK_VGPU12_SFCHG_REN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_SFCHG_REN_SHIFT                 15
#define MT6359_RG_BUCK_VGPU12_HW0_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_BUCK_VGPU12_HW1_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_BUCK_VGPU12_HW3_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_BUCK_VGPU12_HW4_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_BUCK_VGPU12_HW5_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_BUCK_VGPU12_HW6_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_BUCK_VGPU12_HW7_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_BUCK_VGPU12_HW8_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_BUCK_VGPU12_HW9_OP_EN_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VGPU12_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_BUCK_VGPU12_HW10_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW10_OP_EN_SHIFT                10
#define MT6359_RG_BUCK_VGPU12_HW11_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW11_OP_EN_SHIFT                11
#define MT6359_RG_BUCK_VGPU12_HW12_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW12_OP_EN_SHIFT                12
#define MT6359_RG_BUCK_VGPU12_HW13_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW13_OP_EN_SHIFT                13
#define MT6359_RG_BUCK_VGPU12_HW14_OP_EN_ADDR                 \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW14_OP_EN_SHIFT                14
#define MT6359_RG_BUCK_VGPU12_SW_OP_EN_ADDR                   \
	MT6359_BUCK_VGPU12_OP_EN
#define MT6359_RG_BUCK_VGPU12_SW_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VGPU12_SW_OP_EN_SHIFT                  15
#define MT6359_RG_BUCK_VGPU12_OP_EN_SET_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN_SET
#define MT6359_RG_BUCK_VGPU12_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VGPU12_OP_EN_CLR_ADDR                  \
	MT6359_BUCK_VGPU12_OP_EN_CLR
#define MT6359_RG_BUCK_VGPU12_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VGPU12_HW0_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_BUCK_VGPU12_HW1_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_BUCK_VGPU12_HW3_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_BUCK_VGPU12_HW4_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_BUCK_VGPU12_HW5_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_BUCK_VGPU12_HW6_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_BUCK_VGPU12_HW7_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_BUCK_VGPU12_HW8_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_BUCK_VGPU12_HW9_OP_CFG_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_BUCK_VGPU12_HW10_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_BUCK_VGPU12_HW11_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_BUCK_VGPU12_HW12_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_BUCK_VGPU12_HW13_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_BUCK_VGPU12_HW14_OP_CFG_ADDR                \
	MT6359_BUCK_VGPU12_OP_CFG
#define MT6359_RG_BUCK_VGPU12_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_BUCK_VGPU12_OP_CFG_SET_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG_SET
#define MT6359_RG_BUCK_VGPU12_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_CFG_SET_SHIFT                0
#define MT6359_RG_BUCK_VGPU12_OP_CFG_CLR_ADDR                 \
	MT6359_BUCK_VGPU12_OP_CFG_CLR
#define MT6359_RG_BUCK_VGPU12_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_BUCK_VGPU12_HW0_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW0_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW0_OP_MODE_SHIFT               0
#define MT6359_RG_BUCK_VGPU12_HW1_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW1_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW1_OP_MODE_SHIFT               1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW2_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW2_OP_MODE_SHIFT               2
#define MT6359_RG_BUCK_VGPU12_HW3_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW3_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW3_OP_MODE_SHIFT               3
#define MT6359_RG_BUCK_VGPU12_HW4_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW4_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW4_OP_MODE_SHIFT               4
#define MT6359_RG_BUCK_VGPU12_HW5_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW5_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW5_OP_MODE_SHIFT               5
#define MT6359_RG_BUCK_VGPU12_HW6_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW6_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW6_OP_MODE_SHIFT               6
#define MT6359_RG_BUCK_VGPU12_HW7_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW7_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW7_OP_MODE_SHIFT               7
#define MT6359_RG_BUCK_VGPU12_HW8_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW8_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW8_OP_MODE_SHIFT               8
#define MT6359_RG_BUCK_VGPU12_HW9_OP_MODE_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW9_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VGPU12_HW9_OP_MODE_SHIFT               9
#define MT6359_RG_BUCK_VGPU12_HW10_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW10_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU12_HW10_OP_MODE_SHIFT              10
#define MT6359_RG_BUCK_VGPU12_HW11_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW11_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU12_HW11_OP_MODE_SHIFT              11
#define MT6359_RG_BUCK_VGPU12_HW12_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW12_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU12_HW12_OP_MODE_SHIFT              12
#define MT6359_RG_BUCK_VGPU12_HW13_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW13_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU12_HW13_OP_MODE_SHIFT              13
#define MT6359_RG_BUCK_VGPU12_HW14_OP_MODE_ADDR               \
	MT6359_BUCK_VGPU12_OP_MODE
#define MT6359_RG_BUCK_VGPU12_HW14_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VGPU12_HW14_OP_MODE_SHIFT              14
#define MT6359_RG_BUCK_VGPU12_OP_MODE_SET_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE_SET
#define MT6359_RG_BUCK_VGPU12_OP_MODE_SET_MASK                0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_MODE_SET_SHIFT               0
#define MT6359_RG_BUCK_VGPU12_OP_MODE_CLR_ADDR                \
	MT6359_BUCK_VGPU12_OP_MODE_CLR
#define MT6359_RG_BUCK_VGPU12_OP_MODE_CLR_MASK                0xFFFF
#define MT6359_RG_BUCK_VGPU12_OP_MODE_CLR_SHIFT               0
#define MT6359_DA_VGPU12_VOSEL_ADDR                           \
	MT6359_BUCK_VGPU12_DBG0
#define MT6359_DA_VGPU12_VOSEL_MASK                           0x7F
#define MT6359_DA_VGPU12_VOSEL_SHIFT                          0
#define MT6359_DA_VGPU12_VOSEL_GRAY_ADDR                      \
	MT6359_BUCK_VGPU12_DBG0
#define MT6359_DA_VGPU12_VOSEL_GRAY_MASK                      0x7F
#define MT6359_DA_VGPU12_VOSEL_GRAY_SHIFT                     8
#define MT6359_DA_VGPU12_EN_ADDR                              \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_EN_MASK                              0x1
#define MT6359_DA_VGPU12_EN_SHIFT                             0
#define MT6359_DA_VGPU12_STB_ADDR                             \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_STB_MASK                             0x1
#define MT6359_DA_VGPU12_STB_SHIFT                            1
#define MT6359_DA_VGPU12_LOOP_SEL_ADDR                        \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_LOOP_SEL_MASK                        0x1
#define MT6359_DA_VGPU12_LOOP_SEL_SHIFT                       2
#define MT6359_DA_VGPU12_R2R_PDN_ADDR                         \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_R2R_PDN_MASK                         0x1
#define MT6359_DA_VGPU12_R2R_PDN_SHIFT                        3
#define MT6359_DA_VGPU12_DVS_EN_ADDR                          \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_DVS_EN_MASK                          0x1
#define MT6359_DA_VGPU12_DVS_EN_SHIFT                         4
#define MT6359_DA_VGPU12_DVS_DOWN_ADDR                        \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_DVS_DOWN_MASK                        0x1
#define MT6359_DA_VGPU12_DVS_DOWN_SHIFT                       5
#define MT6359_DA_VGPU12_SSH_ADDR                             \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_SSH_MASK                             0x1
#define MT6359_DA_VGPU12_SSH_SHIFT                            6
#define MT6359_DA_VGPU12_MINFREQ_DISCHARGE_ADDR               \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_DA_VGPU12_MINFREQ_DISCHARGE_MASK               0x1
#define MT6359_DA_VGPU12_MINFREQ_DISCHARGE_SHIFT              8
#define MT6359_RG_BUCK_VGPU12_CK_SW_MODE_ADDR                 \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_RG_BUCK_VGPU12_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VGPU12_CK_SW_MODE_SHIFT                12
#define MT6359_RG_BUCK_VGPU12_CK_SW_EN_ADDR                   \
	MT6359_BUCK_VGPU12_DBG1
#define MT6359_RG_BUCK_VGPU12_CK_SW_EN_MASK                   0x1
#define MT6359_RG_BUCK_VGPU12_CK_SW_EN_SHIFT                  13
#define MT6359_BUCK_VGPU12_ELR_LEN_ADDR                       \
	MT6359_BUCK_VGPU12_ELR_NUM
#define MT6359_BUCK_VGPU12_ELR_LEN_MASK                       0xFF
#define MT6359_BUCK_VGPU12_ELR_LEN_SHIFT                      0
#define MT6359_RG_BUCK_VGPU12_VOSEL_ADDR                      \
	MT6359_BUCK_VGPU12_ELR0
#define MT6359_RG_BUCK_VGPU12_VOSEL_MASK                      0x7F
#define MT6359_RG_BUCK_VGPU12_VOSEL_SHIFT                     0
#define MT6359_BUCK_VMODEM_ANA_ID_ADDR                        \
	MT6359_BUCK_VMODEM_DSN_ID
#define MT6359_BUCK_VMODEM_ANA_ID_MASK                        0xFF
#define MT6359_BUCK_VMODEM_ANA_ID_SHIFT                       0
#define MT6359_BUCK_VMODEM_DIG_ID_ADDR                        \
	MT6359_BUCK_VMODEM_DSN_ID
#define MT6359_BUCK_VMODEM_DIG_ID_MASK                        0xFF
#define MT6359_BUCK_VMODEM_DIG_ID_SHIFT                       8
#define MT6359_BUCK_VMODEM_ANA_MINOR_REV_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_REV0
#define MT6359_BUCK_VMODEM_ANA_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VMODEM_ANA_MINOR_REV_SHIFT                0
#define MT6359_BUCK_VMODEM_ANA_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_REV0
#define MT6359_BUCK_VMODEM_ANA_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VMODEM_ANA_MAJOR_REV_SHIFT                4
#define MT6359_BUCK_VMODEM_DIG_MINOR_REV_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_REV0
#define MT6359_BUCK_VMODEM_DIG_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VMODEM_DIG_MINOR_REV_SHIFT                8
#define MT6359_BUCK_VMODEM_DIG_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_REV0
#define MT6359_BUCK_VMODEM_DIG_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VMODEM_DIG_MAJOR_REV_SHIFT                12
#define MT6359_BUCK_VMODEM_DSN_CBS_ADDR                       \
	MT6359_BUCK_VMODEM_DSN_DBI
#define MT6359_BUCK_VMODEM_DSN_CBS_MASK                       0x3
#define MT6359_BUCK_VMODEM_DSN_CBS_SHIFT                      0
#define MT6359_BUCK_VMODEM_DSN_BIX_ADDR                       \
	MT6359_BUCK_VMODEM_DSN_DBI
#define MT6359_BUCK_VMODEM_DSN_BIX_MASK                       0x3
#define MT6359_BUCK_VMODEM_DSN_BIX_SHIFT                      2
#define MT6359_BUCK_VMODEM_DSN_ESP_ADDR                       \
	MT6359_BUCK_VMODEM_DSN_DBI
#define MT6359_BUCK_VMODEM_DSN_ESP_MASK                       0xFF
#define MT6359_BUCK_VMODEM_DSN_ESP_SHIFT                      8
#define MT6359_BUCK_VMODEM_DSN_FPI_SSHUB_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_SSHUB_MASK                 0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_SSHUB_SHIFT                0
#define MT6359_BUCK_VMODEM_DSN_FPI_TRACKING_ADDR              \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_TRACKING_MASK              0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_TRACKING_SHIFT             1
#define MT6359_BUCK_VMODEM_DSN_FPI_PREOC_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_PREOC_MASK                 0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_PREOC_SHIFT                2
#define MT6359_BUCK_VMODEM_DSN_FPI_VOTER_ADDR                 \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_VOTER_MASK                 0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_VOTER_SHIFT                3
#define MT6359_BUCK_VMODEM_DSN_FPI_ULTRASONIC_ADDR            \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_ULTRASONIC_MASK            0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_ULTRASONIC_SHIFT           4
#define MT6359_BUCK_VMODEM_DSN_FPI_DLC_ADDR                   \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_DLC_MASK                   0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_DLC_SHIFT                  5
#define MT6359_BUCK_VMODEM_DSN_FPI_TRAP_ADDR                  \
	MT6359_BUCK_VMODEM_DSN_DXI
#define MT6359_BUCK_VMODEM_DSN_FPI_TRAP_MASK                  0x1
#define MT6359_BUCK_VMODEM_DSN_FPI_TRAP_SHIFT                 6
#define MT6359_RG_BUCK_VMODEM_EN_ADDR                         \
	MT6359_BUCK_VMODEM_CON0
#define MT6359_RG_BUCK_VMODEM_EN_MASK                         0x1
#define MT6359_RG_BUCK_VMODEM_EN_SHIFT                        0
#define MT6359_RG_BUCK_VMODEM_LP_ADDR                         \
	MT6359_BUCK_VMODEM_CON0
#define MT6359_RG_BUCK_VMODEM_LP_MASK                         0x1
#define MT6359_RG_BUCK_VMODEM_LP_SHIFT                        1
#define MT6359_RG_BUCK_VMODEM_CON0_SET_ADDR                   \
	MT6359_BUCK_VMODEM_CON0_SET
#define MT6359_RG_BUCK_VMODEM_CON0_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VMODEM_CON0_SET_SHIFT                  0
#define MT6359_RG_BUCK_VMODEM_CON0_CLR_ADDR                   \
	MT6359_BUCK_VMODEM_CON0_CLR
#define MT6359_RG_BUCK_VMODEM_CON0_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VMODEM_CON0_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VMODEM_VOSEL_SLEEP_ADDR                \
	MT6359_BUCK_VMODEM_CON1
#define MT6359_RG_BUCK_VMODEM_VOSEL_SLEEP_MASK                0x7F
#define MT6359_RG_BUCK_VMODEM_VOSEL_SLEEP_SHIFT               0
#define MT6359_RG_BUCK_VMODEM_SELR2R_CTRL_ADDR                \
	MT6359_BUCK_VMODEM_SLP_CON
#define MT6359_RG_BUCK_VMODEM_SELR2R_CTRL_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_SELR2R_CTRL_SHIFT               0
#define MT6359_RG_BUCK_VMODEM_SFCHG_FRATE_ADDR                \
	MT6359_BUCK_VMODEM_CFG0
#define MT6359_RG_BUCK_VMODEM_SFCHG_FRATE_MASK                0x7F
#define MT6359_RG_BUCK_VMODEM_SFCHG_FRATE_SHIFT               0
#define MT6359_RG_BUCK_VMODEM_SFCHG_FEN_ADDR                  \
	MT6359_BUCK_VMODEM_CFG0
#define MT6359_RG_BUCK_VMODEM_SFCHG_FEN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_SFCHG_FEN_SHIFT                 7
#define MT6359_RG_BUCK_VMODEM_SFCHG_RRATE_ADDR                \
	MT6359_BUCK_VMODEM_CFG0
#define MT6359_RG_BUCK_VMODEM_SFCHG_RRATE_MASK                0x7F
#define MT6359_RG_BUCK_VMODEM_SFCHG_RRATE_SHIFT               8
#define MT6359_RG_BUCK_VMODEM_SFCHG_REN_ADDR                  \
	MT6359_BUCK_VMODEM_CFG0
#define MT6359_RG_BUCK_VMODEM_SFCHG_REN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_SFCHG_REN_SHIFT                 15
#define MT6359_RG_BUCK_VMODEM_HW0_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_BUCK_VMODEM_HW1_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_BUCK_VMODEM_HW3_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_BUCK_VMODEM_HW4_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_BUCK_VMODEM_HW5_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_BUCK_VMODEM_HW6_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_BUCK_VMODEM_HW7_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_BUCK_VMODEM_HW8_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_BUCK_VMODEM_HW9_OP_EN_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VMODEM_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_BUCK_VMODEM_HW10_OP_EN_ADDR                 \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW10_OP_EN_SHIFT                10
#define MT6359_RG_BUCK_VMODEM_HW11_OP_EN_ADDR                 \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW11_OP_EN_SHIFT                11
#define MT6359_RG_BUCK_VMODEM_HW12_OP_EN_ADDR                 \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW12_OP_EN_SHIFT                12
#define MT6359_RG_BUCK_VMODEM_HW13_OP_EN_ADDR                 \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW13_OP_EN_SHIFT                13
#define MT6359_RG_BUCK_VMODEM_HW14_OP_EN_ADDR                 \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW14_OP_EN_SHIFT                14
#define MT6359_RG_BUCK_VMODEM_SW_OP_EN_ADDR                   \
	MT6359_BUCK_VMODEM_OP_EN
#define MT6359_RG_BUCK_VMODEM_SW_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VMODEM_SW_OP_EN_SHIFT                  15
#define MT6359_RG_BUCK_VMODEM_OP_EN_SET_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN_SET
#define MT6359_RG_BUCK_VMODEM_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VMODEM_OP_EN_CLR_ADDR                  \
	MT6359_BUCK_VMODEM_OP_EN_CLR
#define MT6359_RG_BUCK_VMODEM_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VMODEM_HW0_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_BUCK_VMODEM_HW1_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_BUCK_VMODEM_HW3_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_BUCK_VMODEM_HW4_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_BUCK_VMODEM_HW5_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_BUCK_VMODEM_HW6_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_BUCK_VMODEM_HW7_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_BUCK_VMODEM_HW8_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_BUCK_VMODEM_HW9_OP_CFG_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_BUCK_VMODEM_HW10_OP_CFG_ADDR                \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_BUCK_VMODEM_HW11_OP_CFG_ADDR                \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_BUCK_VMODEM_HW12_OP_CFG_ADDR                \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_BUCK_VMODEM_HW13_OP_CFG_ADDR                \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_BUCK_VMODEM_HW14_OP_CFG_ADDR                \
	MT6359_BUCK_VMODEM_OP_CFG
#define MT6359_RG_BUCK_VMODEM_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_BUCK_VMODEM_OP_CFG_SET_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG_SET
#define MT6359_RG_BUCK_VMODEM_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_CFG_SET_SHIFT                0
#define MT6359_RG_BUCK_VMODEM_OP_CFG_CLR_ADDR                 \
	MT6359_BUCK_VMODEM_OP_CFG_CLR
#define MT6359_RG_BUCK_VMODEM_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_BUCK_VMODEM_HW0_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW0_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW0_OP_MODE_SHIFT               0
#define MT6359_RG_BUCK_VMODEM_HW1_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW1_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW1_OP_MODE_SHIFT               1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW2_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW2_OP_MODE_SHIFT               2
#define MT6359_RG_BUCK_VMODEM_HW3_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW3_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW3_OP_MODE_SHIFT               3
#define MT6359_RG_BUCK_VMODEM_HW4_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW4_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW4_OP_MODE_SHIFT               4
#define MT6359_RG_BUCK_VMODEM_HW5_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW5_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW5_OP_MODE_SHIFT               5
#define MT6359_RG_BUCK_VMODEM_HW6_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW6_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW6_OP_MODE_SHIFT               6
#define MT6359_RG_BUCK_VMODEM_HW7_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW7_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW7_OP_MODE_SHIFT               7
#define MT6359_RG_BUCK_VMODEM_HW8_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW8_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW8_OP_MODE_SHIFT               8
#define MT6359_RG_BUCK_VMODEM_HW9_OP_MODE_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW9_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VMODEM_HW9_OP_MODE_SHIFT               9
#define MT6359_RG_BUCK_VMODEM_HW10_OP_MODE_ADDR               \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW10_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VMODEM_HW10_OP_MODE_SHIFT              10
#define MT6359_RG_BUCK_VMODEM_HW11_OP_MODE_ADDR               \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW11_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VMODEM_HW11_OP_MODE_SHIFT              11
#define MT6359_RG_BUCK_VMODEM_HW12_OP_MODE_ADDR               \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW12_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VMODEM_HW12_OP_MODE_SHIFT              12
#define MT6359_RG_BUCK_VMODEM_HW13_OP_MODE_ADDR               \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW13_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VMODEM_HW13_OP_MODE_SHIFT              13
#define MT6359_RG_BUCK_VMODEM_HW14_OP_MODE_ADDR               \
	MT6359_BUCK_VMODEM_OP_MODE
#define MT6359_RG_BUCK_VMODEM_HW14_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VMODEM_HW14_OP_MODE_SHIFT              14
#define MT6359_RG_BUCK_VMODEM_OP_MODE_SET_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE_SET
#define MT6359_RG_BUCK_VMODEM_OP_MODE_SET_MASK                0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_MODE_SET_SHIFT               0
#define MT6359_RG_BUCK_VMODEM_OP_MODE_CLR_ADDR                \
	MT6359_BUCK_VMODEM_OP_MODE_CLR
#define MT6359_RG_BUCK_VMODEM_OP_MODE_CLR_MASK                0xFFFF
#define MT6359_RG_BUCK_VMODEM_OP_MODE_CLR_SHIFT               0
#define MT6359_DA_VMODEM_VOSEL_ADDR                           \
	MT6359_BUCK_VMODEM_DBG0
#define MT6359_DA_VMODEM_VOSEL_MASK                           0x7F
#define MT6359_DA_VMODEM_VOSEL_SHIFT                          0
#define MT6359_DA_VMODEM_VOSEL_GRAY_ADDR                      \
	MT6359_BUCK_VMODEM_DBG0
#define MT6359_DA_VMODEM_VOSEL_GRAY_MASK                      0x7F
#define MT6359_DA_VMODEM_VOSEL_GRAY_SHIFT                     8
#define MT6359_DA_VMODEM_EN_ADDR                              \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_EN_MASK                              0x1
#define MT6359_DA_VMODEM_EN_SHIFT                             0
#define MT6359_DA_VMODEM_STB_ADDR                             \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_STB_MASK                             0x1
#define MT6359_DA_VMODEM_STB_SHIFT                            1
#define MT6359_DA_VMODEM_LOOP_SEL_ADDR                        \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_LOOP_SEL_MASK                        0x1
#define MT6359_DA_VMODEM_LOOP_SEL_SHIFT                       2
#define MT6359_DA_VMODEM_R2R_PDN_ADDR                         \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_R2R_PDN_MASK                         0x1
#define MT6359_DA_VMODEM_R2R_PDN_SHIFT                        3
#define MT6359_DA_VMODEM_DVS_EN_ADDR                          \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_DVS_EN_MASK                          0x1
#define MT6359_DA_VMODEM_DVS_EN_SHIFT                         4
#define MT6359_DA_VMODEM_DVS_DOWN_ADDR                        \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_DVS_DOWN_MASK                        0x1
#define MT6359_DA_VMODEM_DVS_DOWN_SHIFT                       5
#define MT6359_DA_VMODEM_SSH_ADDR                             \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_SSH_MASK                             0x1
#define MT6359_DA_VMODEM_SSH_SHIFT                            6
#define MT6359_DA_VMODEM_MINFREQ_DISCHARGE_ADDR               \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_DA_VMODEM_MINFREQ_DISCHARGE_MASK               0x1
#define MT6359_DA_VMODEM_MINFREQ_DISCHARGE_SHIFT              8
#define MT6359_RG_BUCK_VMODEM_CK_SW_MODE_ADDR                 \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_RG_BUCK_VMODEM_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VMODEM_CK_SW_MODE_SHIFT                12
#define MT6359_RG_BUCK_VMODEM_CK_SW_EN_ADDR                   \
	MT6359_BUCK_VMODEM_DBG1
#define MT6359_RG_BUCK_VMODEM_CK_SW_EN_MASK                   0x1
#define MT6359_RG_BUCK_VMODEM_CK_SW_EN_SHIFT                  13
#define MT6359_RG_BUCK_VMODEM_TRACK_STALL_BYPASS_ADDR         \
	MT6359_BUCK_VMODEM_STALL_TRACK0
#define MT6359_RG_BUCK_VMODEM_TRACK_STALL_BYPASS_MASK         0x1
#define MT6359_RG_BUCK_VMODEM_TRACK_STALL_BYPASS_SHIFT        0
#define MT6359_BUCK_VMODEM_ELR_LEN_ADDR                       \
	MT6359_BUCK_VMODEM_ELR_NUM
#define MT6359_BUCK_VMODEM_ELR_LEN_MASK                       0xFF
#define MT6359_BUCK_VMODEM_ELR_LEN_SHIFT                      0
#define MT6359_RG_BUCK_VMODEM_VOSEL_ADDR                      \
	MT6359_BUCK_VMODEM_ELR0
#define MT6359_RG_BUCK_VMODEM_VOSEL_MASK                      0x7F
#define MT6359_RG_BUCK_VMODEM_VOSEL_SHIFT                     0
#define MT6359_BUCK_VPROC1_ANA_ID_ADDR                        \
	MT6359_BUCK_VPROC1_DSN_ID
#define MT6359_BUCK_VPROC1_ANA_ID_MASK                        0xFF
#define MT6359_BUCK_VPROC1_ANA_ID_SHIFT                       0
#define MT6359_BUCK_VPROC1_DIG_ID_ADDR                        \
	MT6359_BUCK_VPROC1_DSN_ID
#define MT6359_BUCK_VPROC1_DIG_ID_MASK                        0xFF
#define MT6359_BUCK_VPROC1_DIG_ID_SHIFT                       8
#define MT6359_BUCK_VPROC1_ANA_MINOR_REV_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_REV0
#define MT6359_BUCK_VPROC1_ANA_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC1_ANA_MINOR_REV_SHIFT                0
#define MT6359_BUCK_VPROC1_ANA_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_REV0
#define MT6359_BUCK_VPROC1_ANA_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC1_ANA_MAJOR_REV_SHIFT                4
#define MT6359_BUCK_VPROC1_DIG_MINOR_REV_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_REV0
#define MT6359_BUCK_VPROC1_DIG_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC1_DIG_MINOR_REV_SHIFT                8
#define MT6359_BUCK_VPROC1_DIG_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_REV0
#define MT6359_BUCK_VPROC1_DIG_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC1_DIG_MAJOR_REV_SHIFT                12
#define MT6359_BUCK_VPROC1_DSN_CBS_ADDR                       \
	MT6359_BUCK_VPROC1_DSN_DBI
#define MT6359_BUCK_VPROC1_DSN_CBS_MASK                       0x3
#define MT6359_BUCK_VPROC1_DSN_CBS_SHIFT                      0
#define MT6359_BUCK_VPROC1_DSN_BIX_ADDR                       \
	MT6359_BUCK_VPROC1_DSN_DBI
#define MT6359_BUCK_VPROC1_DSN_BIX_MASK                       0x3
#define MT6359_BUCK_VPROC1_DSN_BIX_SHIFT                      2
#define MT6359_BUCK_VPROC1_DSN_ESP_ADDR                       \
	MT6359_BUCK_VPROC1_DSN_DBI
#define MT6359_BUCK_VPROC1_DSN_ESP_MASK                       0xFF
#define MT6359_BUCK_VPROC1_DSN_ESP_SHIFT                      8
#define MT6359_BUCK_VPROC1_DSN_FPI_SSHUB_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_SSHUB_MASK                 0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_SSHUB_SHIFT                0
#define MT6359_BUCK_VPROC1_DSN_FPI_TRACKING_ADDR              \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_TRACKING_MASK              0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_TRACKING_SHIFT             1
#define MT6359_BUCK_VPROC1_DSN_FPI_PREOC_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_PREOC_MASK                 0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_PREOC_SHIFT                2
#define MT6359_BUCK_VPROC1_DSN_FPI_VOTER_ADDR                 \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_VOTER_MASK                 0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_VOTER_SHIFT                3
#define MT6359_BUCK_VPROC1_DSN_FPI_ULTRASONIC_ADDR            \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_ULTRASONIC_MASK            0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_ULTRASONIC_SHIFT           4
#define MT6359_BUCK_VPROC1_DSN_FPI_DLC_ADDR                   \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_DLC_MASK                   0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_DLC_SHIFT                  5
#define MT6359_BUCK_VPROC1_DSN_FPI_TRAP_ADDR                  \
	MT6359_BUCK_VPROC1_DSN_DXI
#define MT6359_BUCK_VPROC1_DSN_FPI_TRAP_MASK                  0x1
#define MT6359_BUCK_VPROC1_DSN_FPI_TRAP_SHIFT                 6
#define MT6359_RG_BUCK_VPROC1_EN_ADDR                         \
	MT6359_BUCK_VPROC1_CON0
#define MT6359_RG_BUCK_VPROC1_EN_MASK                         0x1
#define MT6359_RG_BUCK_VPROC1_EN_SHIFT                        0
#define MT6359_RG_BUCK_VPROC1_LP_ADDR                         \
	MT6359_BUCK_VPROC1_CON0
#define MT6359_RG_BUCK_VPROC1_LP_MASK                         0x1
#define MT6359_RG_BUCK_VPROC1_LP_SHIFT                        1
#define MT6359_RG_BUCK_VPROC1_CON0_SET_ADDR                   \
	MT6359_BUCK_VPROC1_CON0_SET
#define MT6359_RG_BUCK_VPROC1_CON0_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPROC1_CON0_SET_SHIFT                  0
#define MT6359_RG_BUCK_VPROC1_CON0_CLR_ADDR                   \
	MT6359_BUCK_VPROC1_CON0_CLR
#define MT6359_RG_BUCK_VPROC1_CON0_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPROC1_CON0_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VPROC1_VOSEL_SLEEP_ADDR                \
	MT6359_BUCK_VPROC1_CON1
#define MT6359_RG_BUCK_VPROC1_VOSEL_SLEEP_MASK                0x7F
#define MT6359_RG_BUCK_VPROC1_VOSEL_SLEEP_SHIFT               0
#define MT6359_RG_BUCK_VPROC1_SELR2R_CTRL_ADDR                \
	MT6359_BUCK_VPROC1_SLP_CON
#define MT6359_RG_BUCK_VPROC1_SELR2R_CTRL_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_SELR2R_CTRL_SHIFT               0
#define MT6359_RG_BUCK_VPROC1_SFCHG_FRATE_ADDR                \
	MT6359_BUCK_VPROC1_CFG0
#define MT6359_RG_BUCK_VPROC1_SFCHG_FRATE_MASK                0x7F
#define MT6359_RG_BUCK_VPROC1_SFCHG_FRATE_SHIFT               0
#define MT6359_RG_BUCK_VPROC1_SFCHG_FEN_ADDR                  \
	MT6359_BUCK_VPROC1_CFG0
#define MT6359_RG_BUCK_VPROC1_SFCHG_FEN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_SFCHG_FEN_SHIFT                 7
#define MT6359_RG_BUCK_VPROC1_SFCHG_RRATE_ADDR                \
	MT6359_BUCK_VPROC1_CFG0
#define MT6359_RG_BUCK_VPROC1_SFCHG_RRATE_MASK                0x7F
#define MT6359_RG_BUCK_VPROC1_SFCHG_RRATE_SHIFT               8
#define MT6359_RG_BUCK_VPROC1_SFCHG_REN_ADDR                  \
	MT6359_BUCK_VPROC1_CFG0
#define MT6359_RG_BUCK_VPROC1_SFCHG_REN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_SFCHG_REN_SHIFT                 15
#define MT6359_RG_BUCK_VPROC1_HW0_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_BUCK_VPROC1_HW1_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_BUCK_VPROC1_HW3_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_BUCK_VPROC1_HW4_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_BUCK_VPROC1_HW5_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_BUCK_VPROC1_HW6_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_BUCK_VPROC1_HW7_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_BUCK_VPROC1_HW8_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_BUCK_VPROC1_HW9_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC1_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_BUCK_VPROC1_HW10_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW10_OP_EN_SHIFT                10
#define MT6359_RG_BUCK_VPROC1_HW11_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW11_OP_EN_SHIFT                11
#define MT6359_RG_BUCK_VPROC1_HW12_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW12_OP_EN_SHIFT                12
#define MT6359_RG_BUCK_VPROC1_HW13_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW13_OP_EN_SHIFT                13
#define MT6359_RG_BUCK_VPROC1_HW14_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW14_OP_EN_SHIFT                14
#define MT6359_RG_BUCK_VPROC1_SW_OP_EN_ADDR                   \
	MT6359_BUCK_VPROC1_OP_EN
#define MT6359_RG_BUCK_VPROC1_SW_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VPROC1_SW_OP_EN_SHIFT                  15
#define MT6359_RG_BUCK_VPROC1_OP_EN_SET_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN_SET
#define MT6359_RG_BUCK_VPROC1_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VPROC1_OP_EN_CLR_ADDR                  \
	MT6359_BUCK_VPROC1_OP_EN_CLR
#define MT6359_RG_BUCK_VPROC1_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VPROC1_HW0_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_BUCK_VPROC1_HW1_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_BUCK_VPROC1_HW3_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_BUCK_VPROC1_HW4_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_BUCK_VPROC1_HW5_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_BUCK_VPROC1_HW6_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_BUCK_VPROC1_HW7_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_BUCK_VPROC1_HW8_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_BUCK_VPROC1_HW9_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_BUCK_VPROC1_HW10_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_BUCK_VPROC1_HW11_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_BUCK_VPROC1_HW12_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_BUCK_VPROC1_HW13_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_BUCK_VPROC1_HW14_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC1_OP_CFG
#define MT6359_RG_BUCK_VPROC1_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_BUCK_VPROC1_OP_CFG_SET_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG_SET
#define MT6359_RG_BUCK_VPROC1_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_CFG_SET_SHIFT                0
#define MT6359_RG_BUCK_VPROC1_OP_CFG_CLR_ADDR                 \
	MT6359_BUCK_VPROC1_OP_CFG_CLR
#define MT6359_RG_BUCK_VPROC1_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_BUCK_VPROC1_HW0_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW0_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW0_OP_MODE_SHIFT               0
#define MT6359_RG_BUCK_VPROC1_HW1_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW1_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW1_OP_MODE_SHIFT               1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW2_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW2_OP_MODE_SHIFT               2
#define MT6359_RG_BUCK_VPROC1_HW3_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW3_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW3_OP_MODE_SHIFT               3
#define MT6359_RG_BUCK_VPROC1_HW4_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW4_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW4_OP_MODE_SHIFT               4
#define MT6359_RG_BUCK_VPROC1_HW5_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW5_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW5_OP_MODE_SHIFT               5
#define MT6359_RG_BUCK_VPROC1_HW6_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW6_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW6_OP_MODE_SHIFT               6
#define MT6359_RG_BUCK_VPROC1_HW7_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW7_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW7_OP_MODE_SHIFT               7
#define MT6359_RG_BUCK_VPROC1_HW8_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW8_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW8_OP_MODE_SHIFT               8
#define MT6359_RG_BUCK_VPROC1_HW9_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW9_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC1_HW9_OP_MODE_SHIFT               9
#define MT6359_RG_BUCK_VPROC1_HW10_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW10_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC1_HW10_OP_MODE_SHIFT              10
#define MT6359_RG_BUCK_VPROC1_HW11_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW11_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC1_HW11_OP_MODE_SHIFT              11
#define MT6359_RG_BUCK_VPROC1_HW12_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW12_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC1_HW12_OP_MODE_SHIFT              12
#define MT6359_RG_BUCK_VPROC1_HW13_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW13_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC1_HW13_OP_MODE_SHIFT              13
#define MT6359_RG_BUCK_VPROC1_HW14_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC1_OP_MODE
#define MT6359_RG_BUCK_VPROC1_HW14_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC1_HW14_OP_MODE_SHIFT              14
#define MT6359_RG_BUCK_VPROC1_OP_MODE_SET_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE_SET
#define MT6359_RG_BUCK_VPROC1_OP_MODE_SET_MASK                0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_MODE_SET_SHIFT               0
#define MT6359_RG_BUCK_VPROC1_OP_MODE_CLR_ADDR                \
	MT6359_BUCK_VPROC1_OP_MODE_CLR
#define MT6359_RG_BUCK_VPROC1_OP_MODE_CLR_MASK                0xFFFF
#define MT6359_RG_BUCK_VPROC1_OP_MODE_CLR_SHIFT               0
#define MT6359_DA_VPROC1_VOSEL_ADDR                           \
	MT6359_BUCK_VPROC1_DBG0
#define MT6359_DA_VPROC1_VOSEL_MASK                           0x7F
#define MT6359_DA_VPROC1_VOSEL_SHIFT                          0
#define MT6359_DA_VPROC1_VOSEL_GRAY_ADDR                      \
	MT6359_BUCK_VPROC1_DBG0
#define MT6359_DA_VPROC1_VOSEL_GRAY_MASK                      0x7F
#define MT6359_DA_VPROC1_VOSEL_GRAY_SHIFT                     8
#define MT6359_DA_VPROC1_EN_ADDR                              \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_EN_MASK                              0x1
#define MT6359_DA_VPROC1_EN_SHIFT                             0
#define MT6359_DA_VPROC1_STB_ADDR                             \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_STB_MASK                             0x1
#define MT6359_DA_VPROC1_STB_SHIFT                            1
#define MT6359_DA_VPROC1_LOOP_SEL_ADDR                        \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_LOOP_SEL_MASK                        0x1
#define MT6359_DA_VPROC1_LOOP_SEL_SHIFT                       2
#define MT6359_DA_VPROC1_R2R_PDN_ADDR                         \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_R2R_PDN_MASK                         0x1
#define MT6359_DA_VPROC1_R2R_PDN_SHIFT                        3
#define MT6359_DA_VPROC1_DVS_EN_ADDR                          \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_DVS_EN_MASK                          0x1
#define MT6359_DA_VPROC1_DVS_EN_SHIFT                         4
#define MT6359_DA_VPROC1_DVS_DOWN_ADDR                        \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_DVS_DOWN_MASK                        0x1
#define MT6359_DA_VPROC1_DVS_DOWN_SHIFT                       5
#define MT6359_DA_VPROC1_SSH_ADDR                             \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_SSH_MASK                             0x1
#define MT6359_DA_VPROC1_SSH_SHIFT                            6
#define MT6359_DA_VPROC1_MINFREQ_DISCHARGE_ADDR               \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_DA_VPROC1_MINFREQ_DISCHARGE_MASK               0x1
#define MT6359_DA_VPROC1_MINFREQ_DISCHARGE_SHIFT              8
#define MT6359_RG_BUCK_VPROC1_CK_SW_MODE_ADDR                 \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_RG_BUCK_VPROC1_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VPROC1_CK_SW_MODE_SHIFT                12
#define MT6359_RG_BUCK_VPROC1_CK_SW_EN_ADDR                   \
	MT6359_BUCK_VPROC1_DBG1
#define MT6359_RG_BUCK_VPROC1_CK_SW_EN_MASK                   0x1
#define MT6359_RG_BUCK_VPROC1_CK_SW_EN_SHIFT                  13
#define MT6359_RG_BUCK_VPROC1_TRACK_STALL_BYPASS_ADDR         \
	MT6359_BUCK_VPROC1_STALL_TRACK0
#define MT6359_RG_BUCK_VPROC1_TRACK_STALL_BYPASS_MASK         0x1
#define MT6359_RG_BUCK_VPROC1_TRACK_STALL_BYPASS_SHIFT        0
#define MT6359_BUCK_VPROC1_ELR_LEN_ADDR                       \
	MT6359_BUCK_VPROC1_ELR_NUM
#define MT6359_BUCK_VPROC1_ELR_LEN_MASK                       0xFF
#define MT6359_BUCK_VPROC1_ELR_LEN_SHIFT                      0
#define MT6359_RG_BUCK_VPROC1_VOSEL_ADDR                      \
	MT6359_BUCK_VPROC1_ELR0
#define MT6359_RG_BUCK_VPROC1_VOSEL_MASK                      0x7F
#define MT6359_RG_BUCK_VPROC1_VOSEL_SHIFT                     0
#define MT6359_BUCK_VPROC2_ANA_ID_ADDR                        \
	MT6359_BUCK_VPROC2_DSN_ID
#define MT6359_BUCK_VPROC2_ANA_ID_MASK                        0xFF
#define MT6359_BUCK_VPROC2_ANA_ID_SHIFT                       0
#define MT6359_BUCK_VPROC2_DIG_ID_ADDR                        \
	MT6359_BUCK_VPROC2_DSN_ID
#define MT6359_BUCK_VPROC2_DIG_ID_MASK                        0xFF
#define MT6359_BUCK_VPROC2_DIG_ID_SHIFT                       8
#define MT6359_BUCK_VPROC2_ANA_MINOR_REV_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_REV0
#define MT6359_BUCK_VPROC2_ANA_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC2_ANA_MINOR_REV_SHIFT                0
#define MT6359_BUCK_VPROC2_ANA_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_REV0
#define MT6359_BUCK_VPROC2_ANA_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC2_ANA_MAJOR_REV_SHIFT                4
#define MT6359_BUCK_VPROC2_DIG_MINOR_REV_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_REV0
#define MT6359_BUCK_VPROC2_DIG_MINOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC2_DIG_MINOR_REV_SHIFT                8
#define MT6359_BUCK_VPROC2_DIG_MAJOR_REV_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_REV0
#define MT6359_BUCK_VPROC2_DIG_MAJOR_REV_MASK                 0xF
#define MT6359_BUCK_VPROC2_DIG_MAJOR_REV_SHIFT                12
#define MT6359_BUCK_VPROC2_DSN_CBS_ADDR                       \
	MT6359_BUCK_VPROC2_DSN_DBI
#define MT6359_BUCK_VPROC2_DSN_CBS_MASK                       0x3
#define MT6359_BUCK_VPROC2_DSN_CBS_SHIFT                      0
#define MT6359_BUCK_VPROC2_DSN_BIX_ADDR                       \
	MT6359_BUCK_VPROC2_DSN_DBI
#define MT6359_BUCK_VPROC2_DSN_BIX_MASK                       0x3
#define MT6359_BUCK_VPROC2_DSN_BIX_SHIFT                      2
#define MT6359_BUCK_VPROC2_DSN_ESP_ADDR                       \
	MT6359_BUCK_VPROC2_DSN_DBI
#define MT6359_BUCK_VPROC2_DSN_ESP_MASK                       0xFF
#define MT6359_BUCK_VPROC2_DSN_ESP_SHIFT                      8
#define MT6359_BUCK_VPROC2_DSN_FPI_SSHUB_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_SSHUB_MASK                 0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_SSHUB_SHIFT                0
#define MT6359_BUCK_VPROC2_DSN_FPI_TRACKING_ADDR              \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_TRACKING_MASK              0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_TRACKING_SHIFT             1
#define MT6359_BUCK_VPROC2_DSN_FPI_PREOC_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_PREOC_MASK                 0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_PREOC_SHIFT                2
#define MT6359_BUCK_VPROC2_DSN_FPI_VOTER_ADDR                 \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_VOTER_MASK                 0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_VOTER_SHIFT                3
#define MT6359_BUCK_VPROC2_DSN_FPI_ULTRASONIC_ADDR            \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_ULTRASONIC_MASK            0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_ULTRASONIC_SHIFT           4
#define MT6359_BUCK_VPROC2_DSN_FPI_DLC_ADDR                   \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_DLC_MASK                   0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_DLC_SHIFT                  5
#define MT6359_BUCK_VPROC2_DSN_FPI_TRAP_ADDR                  \
	MT6359_BUCK_VPROC2_DSN_DXI
#define MT6359_BUCK_VPROC2_DSN_FPI_TRAP_MASK                  0x1
#define MT6359_BUCK_VPROC2_DSN_FPI_TRAP_SHIFT                 6
#define MT6359_RG_BUCK_VPROC2_EN_ADDR                         \
	MT6359_BUCK_VPROC2_CON0
#define MT6359_RG_BUCK_VPROC2_EN_MASK                         0x1
#define MT6359_RG_BUCK_VPROC2_EN_SHIFT                        0
#define MT6359_RG_BUCK_VPROC2_LP_ADDR                         \
	MT6359_BUCK_VPROC2_CON0
#define MT6359_RG_BUCK_VPROC2_LP_MASK                         0x1
#define MT6359_RG_BUCK_VPROC2_LP_SHIFT                        1
#define MT6359_RG_BUCK_VPROC2_CON0_SET_ADDR                   \
	MT6359_BUCK_VPROC2_CON0_SET
#define MT6359_RG_BUCK_VPROC2_CON0_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPROC2_CON0_SET_SHIFT                  0
#define MT6359_RG_BUCK_VPROC2_CON0_CLR_ADDR                   \
	MT6359_BUCK_VPROC2_CON0_CLR
#define MT6359_RG_BUCK_VPROC2_CON0_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VPROC2_CON0_CLR_SHIFT                  0
#define MT6359_RG_BUCK_VPROC2_VOSEL_SLEEP_ADDR                \
	MT6359_BUCK_VPROC2_CON1
#define MT6359_RG_BUCK_VPROC2_VOSEL_SLEEP_MASK                0x7F
#define MT6359_RG_BUCK_VPROC2_VOSEL_SLEEP_SHIFT               0
#define MT6359_RG_BUCK_VPROC2_SELR2R_CTRL_ADDR                \
	MT6359_BUCK_VPROC2_SLP_CON
#define MT6359_RG_BUCK_VPROC2_SELR2R_CTRL_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_SELR2R_CTRL_SHIFT               0
#define MT6359_RG_BUCK_VPROC2_SFCHG_FRATE_ADDR                \
	MT6359_BUCK_VPROC2_CFG0
#define MT6359_RG_BUCK_VPROC2_SFCHG_FRATE_MASK                0x7F
#define MT6359_RG_BUCK_VPROC2_SFCHG_FRATE_SHIFT               0
#define MT6359_RG_BUCK_VPROC2_SFCHG_FEN_ADDR                  \
	MT6359_BUCK_VPROC2_CFG0
#define MT6359_RG_BUCK_VPROC2_SFCHG_FEN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_SFCHG_FEN_SHIFT                 7
#define MT6359_RG_BUCK_VPROC2_SFCHG_RRATE_ADDR                \
	MT6359_BUCK_VPROC2_CFG0
#define MT6359_RG_BUCK_VPROC2_SFCHG_RRATE_MASK                0x7F
#define MT6359_RG_BUCK_VPROC2_SFCHG_RRATE_SHIFT               8
#define MT6359_RG_BUCK_VPROC2_SFCHG_REN_ADDR                  \
	MT6359_BUCK_VPROC2_CFG0
#define MT6359_RG_BUCK_VPROC2_SFCHG_REN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_SFCHG_REN_SHIFT                 15
#define MT6359_RG_BUCK_VPROC2_HW0_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_BUCK_VPROC2_HW1_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_BUCK_VPROC2_HW3_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_BUCK_VPROC2_HW4_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_BUCK_VPROC2_HW5_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_BUCK_VPROC2_HW6_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_BUCK_VPROC2_HW7_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_BUCK_VPROC2_HW8_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_BUCK_VPROC2_HW9_OP_EN_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_BUCK_VPROC2_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_BUCK_VPROC2_HW10_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW10_OP_EN_SHIFT                10
#define MT6359_RG_BUCK_VPROC2_HW11_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW11_OP_EN_SHIFT                11
#define MT6359_RG_BUCK_VPROC2_HW12_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW12_OP_EN_SHIFT                12
#define MT6359_RG_BUCK_VPROC2_HW13_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW13_OP_EN_SHIFT                13
#define MT6359_RG_BUCK_VPROC2_HW14_OP_EN_ADDR                 \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW14_OP_EN_SHIFT                14
#define MT6359_RG_BUCK_VPROC2_SW_OP_EN_ADDR                   \
	MT6359_BUCK_VPROC2_OP_EN
#define MT6359_RG_BUCK_VPROC2_SW_OP_EN_MASK                   0x1
#define MT6359_RG_BUCK_VPROC2_SW_OP_EN_SHIFT                  15
#define MT6359_RG_BUCK_VPROC2_OP_EN_SET_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN_SET
#define MT6359_RG_BUCK_VPROC2_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VPROC2_OP_EN_CLR_ADDR                  \
	MT6359_BUCK_VPROC2_OP_EN_CLR
#define MT6359_RG_BUCK_VPROC2_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VPROC2_HW0_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_BUCK_VPROC2_HW1_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_BUCK_VPROC2_HW3_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_BUCK_VPROC2_HW4_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_BUCK_VPROC2_HW5_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_BUCK_VPROC2_HW6_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_BUCK_VPROC2_HW7_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_BUCK_VPROC2_HW8_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_BUCK_VPROC2_HW9_OP_CFG_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_BUCK_VPROC2_HW10_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_BUCK_VPROC2_HW11_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_BUCK_VPROC2_HW12_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_BUCK_VPROC2_HW13_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_BUCK_VPROC2_HW14_OP_CFG_ADDR                \
	MT6359_BUCK_VPROC2_OP_CFG
#define MT6359_RG_BUCK_VPROC2_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_BUCK_VPROC2_OP_CFG_SET_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG_SET
#define MT6359_RG_BUCK_VPROC2_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_CFG_SET_SHIFT                0
#define MT6359_RG_BUCK_VPROC2_OP_CFG_CLR_ADDR                 \
	MT6359_BUCK_VPROC2_OP_CFG_CLR
#define MT6359_RG_BUCK_VPROC2_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_BUCK_VPROC2_HW0_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW0_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW0_OP_MODE_SHIFT               0
#define MT6359_RG_BUCK_VPROC2_HW1_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW1_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW1_OP_MODE_SHIFT               1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW2_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW2_OP_MODE_SHIFT               2
#define MT6359_RG_BUCK_VPROC2_HW3_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW3_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW3_OP_MODE_SHIFT               3
#define MT6359_RG_BUCK_VPROC2_HW4_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW4_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW4_OP_MODE_SHIFT               4
#define MT6359_RG_BUCK_VPROC2_HW5_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW5_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW5_OP_MODE_SHIFT               5
#define MT6359_RG_BUCK_VPROC2_HW6_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW6_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW6_OP_MODE_SHIFT               6
#define MT6359_RG_BUCK_VPROC2_HW7_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW7_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW7_OP_MODE_SHIFT               7
#define MT6359_RG_BUCK_VPROC2_HW8_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW8_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW8_OP_MODE_SHIFT               8
#define MT6359_RG_BUCK_VPROC2_HW9_OP_MODE_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW9_OP_MODE_MASK                0x1
#define MT6359_RG_BUCK_VPROC2_HW9_OP_MODE_SHIFT               9
#define MT6359_RG_BUCK_VPROC2_HW10_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW10_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC2_HW10_OP_MODE_SHIFT              10
#define MT6359_RG_BUCK_VPROC2_HW11_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW11_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC2_HW11_OP_MODE_SHIFT              11
#define MT6359_RG_BUCK_VPROC2_HW12_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW12_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC2_HW12_OP_MODE_SHIFT              12
#define MT6359_RG_BUCK_VPROC2_HW13_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW13_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC2_HW13_OP_MODE_SHIFT              13
#define MT6359_RG_BUCK_VPROC2_HW14_OP_MODE_ADDR               \
	MT6359_BUCK_VPROC2_OP_MODE
#define MT6359_RG_BUCK_VPROC2_HW14_OP_MODE_MASK               0x1
#define MT6359_RG_BUCK_VPROC2_HW14_OP_MODE_SHIFT              14
#define MT6359_RG_BUCK_VPROC2_OP_MODE_SET_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE_SET
#define MT6359_RG_BUCK_VPROC2_OP_MODE_SET_MASK                0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_MODE_SET_SHIFT               0
#define MT6359_RG_BUCK_VPROC2_OP_MODE_CLR_ADDR                \
	MT6359_BUCK_VPROC2_OP_MODE_CLR
#define MT6359_RG_BUCK_VPROC2_OP_MODE_CLR_MASK                0xFFFF
#define MT6359_RG_BUCK_VPROC2_OP_MODE_CLR_SHIFT               0
#define MT6359_DA_VPROC2_VOSEL_ADDR                           \
	MT6359_BUCK_VPROC2_DBG0
#define MT6359_DA_VPROC2_VOSEL_MASK                           0x7F
#define MT6359_DA_VPROC2_VOSEL_SHIFT                          0
#define MT6359_DA_VPROC2_VOSEL_GRAY_ADDR                      \
	MT6359_BUCK_VPROC2_DBG0
#define MT6359_DA_VPROC2_VOSEL_GRAY_MASK                      0x7F
#define MT6359_DA_VPROC2_VOSEL_GRAY_SHIFT                     8
#define MT6359_DA_VPROC2_EN_ADDR                              \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_EN_MASK                              0x1
#define MT6359_DA_VPROC2_EN_SHIFT                             0
#define MT6359_DA_VPROC2_STB_ADDR                             \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_STB_MASK                             0x1
#define MT6359_DA_VPROC2_STB_SHIFT                            1
#define MT6359_DA_VPROC2_LOOP_SEL_ADDR                        \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_LOOP_SEL_MASK                        0x1
#define MT6359_DA_VPROC2_LOOP_SEL_SHIFT                       2
#define MT6359_DA_VPROC2_R2R_PDN_ADDR                         \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_R2R_PDN_MASK                         0x1
#define MT6359_DA_VPROC2_R2R_PDN_SHIFT                        3
#define MT6359_DA_VPROC2_DVS_EN_ADDR                          \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_DVS_EN_MASK                          0x1
#define MT6359_DA_VPROC2_DVS_EN_SHIFT                         4
#define MT6359_DA_VPROC2_DVS_DOWN_ADDR                        \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_DVS_DOWN_MASK                        0x1
#define MT6359_DA_VPROC2_DVS_DOWN_SHIFT                       5
#define MT6359_DA_VPROC2_SSH_ADDR                             \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_SSH_MASK                             0x1
#define MT6359_DA_VPROC2_SSH_SHIFT                            6
#define MT6359_DA_VPROC2_MINFREQ_DISCHARGE_ADDR               \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_DA_VPROC2_MINFREQ_DISCHARGE_MASK               0x1
#define MT6359_DA_VPROC2_MINFREQ_DISCHARGE_SHIFT              8
#define MT6359_RG_BUCK_VPROC2_CK_SW_MODE_ADDR                 \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_RG_BUCK_VPROC2_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_CK_SW_MODE_SHIFT                12
#define MT6359_RG_BUCK_VPROC2_CK_SW_EN_ADDR                   \
	MT6359_BUCK_VPROC2_DBG1
#define MT6359_RG_BUCK_VPROC2_CK_SW_EN_MASK                   0x1
#define MT6359_RG_BUCK_VPROC2_CK_SW_EN_SHIFT                  13
#define MT6359_RG_BUCK_VPROC2_TRACK_EN_ADDR                   \
	MT6359_BUCK_VPROC2_TRACK0
#define MT6359_RG_BUCK_VPROC2_TRACK_EN_MASK                   0x1
#define MT6359_RG_BUCK_VPROC2_TRACK_EN_SHIFT                  0
#define MT6359_RG_BUCK_VPROC2_TRACK_MODE_ADDR                 \
	MT6359_BUCK_VPROC2_TRACK0
#define MT6359_RG_BUCK_VPROC2_TRACK_MODE_MASK                 0x1
#define MT6359_RG_BUCK_VPROC2_TRACK_MODE_SHIFT                1
#define MT6359_RG_BUCK_VPROC2_VOSEL_DELTA_ADDR                \
	MT6359_BUCK_VPROC2_TRACK0
#define MT6359_RG_BUCK_VPROC2_VOSEL_DELTA_MASK                0xF
#define MT6359_RG_BUCK_VPROC2_VOSEL_DELTA_SHIFT               4
#define MT6359_RG_BUCK_VPROC2_VOSEL_OFFSET_ADDR               \
	MT6359_BUCK_VPROC2_TRACK0
#define MT6359_RG_BUCK_VPROC2_VOSEL_OFFSET_MASK               0x7F
#define MT6359_RG_BUCK_VPROC2_VOSEL_OFFSET_SHIFT              8
#define MT6359_RG_BUCK_VPROC2_VOSEL_LB_ADDR                   \
	MT6359_BUCK_VPROC2_TRACK1
#define MT6359_RG_BUCK_VPROC2_VOSEL_LB_MASK                   0x7F
#define MT6359_RG_BUCK_VPROC2_VOSEL_LB_SHIFT                  0
#define MT6359_RG_BUCK_VPROC2_VOSEL_HB_ADDR                   \
	MT6359_BUCK_VPROC2_TRACK1
#define MT6359_RG_BUCK_VPROC2_VOSEL_HB_MASK                   0x7F
#define MT6359_RG_BUCK_VPROC2_VOSEL_HB_SHIFT                  8
#define MT6359_RG_BUCK_VPROC2_TRACK_STALL_BYPASS_ADDR         \
	MT6359_BUCK_VPROC2_STALL_TRACK0
#define MT6359_RG_BUCK_VPROC2_TRACK_STALL_BYPASS_MASK         0x1
#define MT6359_RG_BUCK_VPROC2_TRACK_STALL_BYPASS_SHIFT        0
#define MT6359_BUCK_VPROC2_ELR_LEN_ADDR                       \
	MT6359_BUCK_VPROC2_ELR_NUM
#define MT6359_BUCK_VPROC2_ELR_LEN_MASK                       0xFF
#define MT6359_BUCK_VPROC2_ELR_LEN_SHIFT                      0
#define MT6359_RG_BUCK_VPROC2_VOSEL_ADDR                      \
	MT6359_BUCK_VPROC2_ELR0
#define MT6359_RG_BUCK_VPROC2_VOSEL_MASK                      0x7F
#define MT6359_RG_BUCK_VPROC2_VOSEL_SHIFT                     0
#define MT6359_BUCK_VS1_ANA_ID_ADDR                           \
	MT6359_BUCK_VS1_DSN_ID
#define MT6359_BUCK_VS1_ANA_ID_MASK                           0xFF
#define MT6359_BUCK_VS1_ANA_ID_SHIFT                          0
#define MT6359_BUCK_VS1_DIG_ID_ADDR                           \
	MT6359_BUCK_VS1_DSN_ID
#define MT6359_BUCK_VS1_DIG_ID_MASK                           0xFF
#define MT6359_BUCK_VS1_DIG_ID_SHIFT                          8
#define MT6359_BUCK_VS1_ANA_MINOR_REV_ADDR                    \
	MT6359_BUCK_VS1_DSN_REV0
#define MT6359_BUCK_VS1_ANA_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VS1_ANA_MINOR_REV_SHIFT                   0
#define MT6359_BUCK_VS1_ANA_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VS1_DSN_REV0
#define MT6359_BUCK_VS1_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VS1_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_BUCK_VS1_DIG_MINOR_REV_ADDR                    \
	MT6359_BUCK_VS1_DSN_REV0
#define MT6359_BUCK_VS1_DIG_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VS1_DIG_MINOR_REV_SHIFT                   8
#define MT6359_BUCK_VS1_DIG_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VS1_DSN_REV0
#define MT6359_BUCK_VS1_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VS1_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_BUCK_VS1_DSN_CBS_ADDR                          \
	MT6359_BUCK_VS1_DSN_DBI
#define MT6359_BUCK_VS1_DSN_CBS_MASK                          0x3
#define MT6359_BUCK_VS1_DSN_CBS_SHIFT                         0
#define MT6359_BUCK_VS1_DSN_BIX_ADDR                          \
	MT6359_BUCK_VS1_DSN_DBI
#define MT6359_BUCK_VS1_DSN_BIX_MASK                          0x3
#define MT6359_BUCK_VS1_DSN_BIX_SHIFT                         2
#define MT6359_BUCK_VS1_DSN_ESP_ADDR                          \
	MT6359_BUCK_VS1_DSN_DBI
#define MT6359_BUCK_VS1_DSN_ESP_MASK                          0xFF
#define MT6359_BUCK_VS1_DSN_ESP_SHIFT                         8
#define MT6359_BUCK_VS1_DSN_FPI_SSHUB_ADDR                    \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_SSHUB_MASK                    0x1
#define MT6359_BUCK_VS1_DSN_FPI_SSHUB_SHIFT                   0
#define MT6359_BUCK_VS1_DSN_FPI_TRACKING_ADDR                 \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_TRACKING_MASK                 0x1
#define MT6359_BUCK_VS1_DSN_FPI_TRACKING_SHIFT                1
#define MT6359_BUCK_VS1_DSN_FPI_PREOC_ADDR                    \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_PREOC_MASK                    0x1
#define MT6359_BUCK_VS1_DSN_FPI_PREOC_SHIFT                   2
#define MT6359_BUCK_VS1_DSN_FPI_VOTER_ADDR                    \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_VOTER_MASK                    0x1
#define MT6359_BUCK_VS1_DSN_FPI_VOTER_SHIFT                   3
#define MT6359_BUCK_VS1_DSN_FPI_ULTRASONIC_ADDR               \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_ULTRASONIC_MASK               0x1
#define MT6359_BUCK_VS1_DSN_FPI_ULTRASONIC_SHIFT              4
#define MT6359_BUCK_VS1_DSN_FPI_DLC_ADDR                      \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_DLC_MASK                      0x1
#define MT6359_BUCK_VS1_DSN_FPI_DLC_SHIFT                     5
#define MT6359_BUCK_VS1_DSN_FPI_TRAP_ADDR                     \
	MT6359_BUCK_VS1_DSN_DXI
#define MT6359_BUCK_VS1_DSN_FPI_TRAP_MASK                     0x1
#define MT6359_BUCK_VS1_DSN_FPI_TRAP_SHIFT                    6
#define MT6359_RG_BUCK_VS1_EN_ADDR                            \
	MT6359_BUCK_VS1_CON0
#define MT6359_RG_BUCK_VS1_EN_MASK                            0x1
#define MT6359_RG_BUCK_VS1_EN_SHIFT                           0
#define MT6359_RG_BUCK_VS1_LP_ADDR                            \
	MT6359_BUCK_VS1_CON0
#define MT6359_RG_BUCK_VS1_LP_MASK                            0x1
#define MT6359_RG_BUCK_VS1_LP_SHIFT                           1
#define MT6359_RG_BUCK_VS1_CON0_SET_ADDR                      \
	MT6359_BUCK_VS1_CON0_SET
#define MT6359_RG_BUCK_VS1_CON0_SET_MASK                      0xFFFF
#define MT6359_RG_BUCK_VS1_CON0_SET_SHIFT                     0
#define MT6359_RG_BUCK_VS1_CON0_CLR_ADDR                      \
	MT6359_BUCK_VS1_CON0_CLR
#define MT6359_RG_BUCK_VS1_CON0_CLR_MASK                      0xFFFF
#define MT6359_RG_BUCK_VS1_CON0_CLR_SHIFT                     0
#define MT6359_RG_BUCK_VS1_VOSEL_SLEEP_ADDR                   \
	MT6359_BUCK_VS1_CON1
#define MT6359_RG_BUCK_VS1_VOSEL_SLEEP_MASK                   0x7F
#define MT6359_RG_BUCK_VS1_VOSEL_SLEEP_SHIFT                  0
#define MT6359_RG_BUCK_VS1_SELR2R_CTRL_ADDR                   \
	MT6359_BUCK_VS1_SLP_CON
#define MT6359_RG_BUCK_VS1_SELR2R_CTRL_MASK                   0x1
#define MT6359_RG_BUCK_VS1_SELR2R_CTRL_SHIFT                  0
#define MT6359_RG_BUCK_VS1_SFCHG_FRATE_ADDR                   \
	MT6359_BUCK_VS1_CFG0
#define MT6359_RG_BUCK_VS1_SFCHG_FRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VS1_SFCHG_FRATE_SHIFT                  0
#define MT6359_RG_BUCK_VS1_SFCHG_FEN_ADDR                     \
	MT6359_BUCK_VS1_CFG0
#define MT6359_RG_BUCK_VS1_SFCHG_FEN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_SFCHG_FEN_SHIFT                    7
#define MT6359_RG_BUCK_VS1_SFCHG_RRATE_ADDR                   \
	MT6359_BUCK_VS1_CFG0
#define MT6359_RG_BUCK_VS1_SFCHG_RRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VS1_SFCHG_RRATE_SHIFT                  8
#define MT6359_RG_BUCK_VS1_SFCHG_REN_ADDR                     \
	MT6359_BUCK_VS1_CFG0
#define MT6359_RG_BUCK_VS1_SFCHG_REN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_SFCHG_REN_SHIFT                    15
#define MT6359_RG_BUCK_VS1_HW0_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_BUCK_VS1_HW1_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_BUCK_VS1_HW2_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_BUCK_VS1_HW3_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_BUCK_VS1_HW4_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_BUCK_VS1_HW5_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_BUCK_VS1_HW6_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_BUCK_VS1_HW7_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_BUCK_VS1_HW8_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_BUCK_VS1_HW9_OP_EN_ADDR                     \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS1_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_BUCK_VS1_HW10_OP_EN_ADDR                    \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_BUCK_VS1_HW11_OP_EN_ADDR                    \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_BUCK_VS1_HW12_OP_EN_ADDR                    \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_BUCK_VS1_HW13_OP_EN_ADDR                    \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_BUCK_VS1_HW14_OP_EN_ADDR                    \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_BUCK_VS1_SW_OP_EN_ADDR                      \
	MT6359_BUCK_VS1_OP_EN
#define MT6359_RG_BUCK_VS1_SW_OP_EN_MASK                      0x1
#define MT6359_RG_BUCK_VS1_SW_OP_EN_SHIFT                     15
#define MT6359_RG_BUCK_VS1_OP_EN_SET_ADDR                     \
	MT6359_BUCK_VS1_OP_EN_SET
#define MT6359_RG_BUCK_VS1_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_BUCK_VS1_OP_EN_SET_SHIFT                    0
#define MT6359_RG_BUCK_VS1_OP_EN_CLR_ADDR                     \
	MT6359_BUCK_VS1_OP_EN_CLR
#define MT6359_RG_BUCK_VS1_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_BUCK_VS1_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_BUCK_VS1_HW0_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_BUCK_VS1_HW1_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_BUCK_VS1_HW2_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_BUCK_VS1_HW3_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_BUCK_VS1_HW4_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_BUCK_VS1_HW5_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_BUCK_VS1_HW6_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_BUCK_VS1_HW7_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_BUCK_VS1_HW8_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_BUCK_VS1_HW9_OP_CFG_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS1_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_BUCK_VS1_HW10_OP_CFG_ADDR                   \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_BUCK_VS1_HW11_OP_CFG_ADDR                   \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_BUCK_VS1_HW12_OP_CFG_ADDR                   \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_BUCK_VS1_HW13_OP_CFG_ADDR                   \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_BUCK_VS1_HW14_OP_CFG_ADDR                   \
	MT6359_BUCK_VS1_OP_CFG
#define MT6359_RG_BUCK_VS1_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_BUCK_VS1_OP_CFG_SET_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG_SET
#define MT6359_RG_BUCK_VS1_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_BUCK_VS1_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_BUCK_VS1_OP_CFG_CLR_ADDR                    \
	MT6359_BUCK_VS1_OP_CFG_CLR
#define MT6359_RG_BUCK_VS1_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_BUCK_VS1_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_BUCK_VS1_HW0_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW0_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW0_OP_MODE_SHIFT                  0
#define MT6359_RG_BUCK_VS1_HW1_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW1_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW1_OP_MODE_SHIFT                  1
#define MT6359_RG_BUCK_VS1_HW2_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW2_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW2_OP_MODE_SHIFT                  2
#define MT6359_RG_BUCK_VS1_HW3_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW3_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW3_OP_MODE_SHIFT                  3
#define MT6359_RG_BUCK_VS1_HW4_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW4_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW4_OP_MODE_SHIFT                  4
#define MT6359_RG_BUCK_VS1_HW5_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW5_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW5_OP_MODE_SHIFT                  5
#define MT6359_RG_BUCK_VS1_HW6_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW6_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW6_OP_MODE_SHIFT                  6
#define MT6359_RG_BUCK_VS1_HW7_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW7_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW7_OP_MODE_SHIFT                  7
#define MT6359_RG_BUCK_VS1_HW8_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW8_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW8_OP_MODE_SHIFT                  8
#define MT6359_RG_BUCK_VS1_HW9_OP_MODE_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW9_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS1_HW9_OP_MODE_SHIFT                  9
#define MT6359_RG_BUCK_VS1_HW10_OP_MODE_ADDR                  \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW10_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS1_HW10_OP_MODE_SHIFT                 10
#define MT6359_RG_BUCK_VS1_HW11_OP_MODE_ADDR                  \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW11_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS1_HW11_OP_MODE_SHIFT                 11
#define MT6359_RG_BUCK_VS1_HW12_OP_MODE_ADDR                  \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW12_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS1_HW12_OP_MODE_SHIFT                 12
#define MT6359_RG_BUCK_VS1_HW13_OP_MODE_ADDR                  \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW13_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS1_HW13_OP_MODE_SHIFT                 13
#define MT6359_RG_BUCK_VS1_HW14_OP_MODE_ADDR                  \
	MT6359_BUCK_VS1_OP_MODE
#define MT6359_RG_BUCK_VS1_HW14_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS1_HW14_OP_MODE_SHIFT                 14
#define MT6359_RG_BUCK_VS1_OP_MODE_SET_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE_SET
#define MT6359_RG_BUCK_VS1_OP_MODE_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VS1_OP_MODE_SET_SHIFT                  0
#define MT6359_RG_BUCK_VS1_OP_MODE_CLR_ADDR                   \
	MT6359_BUCK_VS1_OP_MODE_CLR
#define MT6359_RG_BUCK_VS1_OP_MODE_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VS1_OP_MODE_CLR_SHIFT                  0
#define MT6359_DA_VS1_VOSEL_ADDR                              \
	MT6359_BUCK_VS1_DBG0
#define MT6359_DA_VS1_VOSEL_MASK                              0x7F
#define MT6359_DA_VS1_VOSEL_SHIFT                             0
#define MT6359_DA_VS1_VOSEL_GRAY_ADDR                         \
	MT6359_BUCK_VS1_DBG0
#define MT6359_DA_VS1_VOSEL_GRAY_MASK                         0x7F
#define MT6359_DA_VS1_VOSEL_GRAY_SHIFT                        8
#define MT6359_DA_VS1_EN_ADDR                                 \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_EN_MASK                                 0x1
#define MT6359_DA_VS1_EN_SHIFT                                0
#define MT6359_DA_VS1_STB_ADDR                                \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_STB_MASK                                0x1
#define MT6359_DA_VS1_STB_SHIFT                               1
#define MT6359_DA_VS1_LOOP_SEL_ADDR                           \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_LOOP_SEL_MASK                           0x1
#define MT6359_DA_VS1_LOOP_SEL_SHIFT                          2
#define MT6359_DA_VS1_R2R_PDN_ADDR                            \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_R2R_PDN_MASK                            0x1
#define MT6359_DA_VS1_R2R_PDN_SHIFT                           3
#define MT6359_DA_VS1_DVS_EN_ADDR                             \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_DVS_EN_MASK                             0x1
#define MT6359_DA_VS1_DVS_EN_SHIFT                            4
#define MT6359_DA_VS1_DVS_DOWN_ADDR                           \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_DVS_DOWN_MASK                           0x1
#define MT6359_DA_VS1_DVS_DOWN_SHIFT                          5
#define MT6359_DA_VS1_SSH_ADDR                                \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_SSH_MASK                                0x1
#define MT6359_DA_VS1_SSH_SHIFT                               6
#define MT6359_DA_VS1_MINFREQ_DISCHARGE_ADDR                  \
	MT6359_BUCK_VS1_DBG1
#define MT6359_DA_VS1_MINFREQ_DISCHARGE_MASK                  0x1
#define MT6359_DA_VS1_MINFREQ_DISCHARGE_SHIFT                 8
#define MT6359_RG_BUCK_VS1_CK_SW_MODE_ADDR                    \
	MT6359_BUCK_VS1_DBG1
#define MT6359_RG_BUCK_VS1_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_BUCK_VS1_CK_SW_MODE_SHIFT                   12
#define MT6359_RG_BUCK_VS1_CK_SW_EN_ADDR                      \
	MT6359_BUCK_VS1_DBG1
#define MT6359_RG_BUCK_VS1_CK_SW_EN_MASK                      0x1
#define MT6359_RG_BUCK_VS1_CK_SW_EN_SHIFT                     13
#define MT6359_RG_BUCK_VS1_VOTER_EN_ADDR                      \
	MT6359_BUCK_VS1_VOTER
#define MT6359_RG_BUCK_VS1_VOTER_EN_MASK                      0xFFF
#define MT6359_RG_BUCK_VS1_VOTER_EN_SHIFT                     0
#define MT6359_RG_BUCK_VS1_VOTER_EN_SET_ADDR                  \
	MT6359_BUCK_VS1_VOTER_SET
#define MT6359_RG_BUCK_VS1_VOTER_EN_SET_MASK                  0xFFF
#define MT6359_RG_BUCK_VS1_VOTER_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VS1_VOTER_EN_CLR_ADDR                  \
	MT6359_BUCK_VS1_VOTER_CLR
#define MT6359_RG_BUCK_VS1_VOTER_EN_CLR_MASK                  0xFFF
#define MT6359_RG_BUCK_VS1_VOTER_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VS1_VOTER_VOSEL_ADDR                   \
	MT6359_BUCK_VS1_VOTER_CFG
#define MT6359_RG_BUCK_VS1_VOTER_VOSEL_MASK                   0x7F
#define MT6359_RG_BUCK_VS1_VOTER_VOSEL_SHIFT                  0
#define MT6359_BUCK_VS1_ELR_LEN_ADDR                          \
	MT6359_BUCK_VS1_ELR_NUM
#define MT6359_BUCK_VS1_ELR_LEN_MASK                          0xFF
#define MT6359_BUCK_VS1_ELR_LEN_SHIFT                         0
#define MT6359_RG_BUCK_VS1_VOSEL_ADDR                         \
	MT6359_BUCK_VS1_ELR0
#define MT6359_RG_BUCK_VS1_VOSEL_MASK                         0x7F
#define MT6359_RG_BUCK_VS1_VOSEL_SHIFT                        0
#define MT6359_BUCK_VS2_ANA_ID_ADDR                           \
	MT6359_BUCK_VS2_DSN_ID
#define MT6359_BUCK_VS2_ANA_ID_MASK                           0xFF
#define MT6359_BUCK_VS2_ANA_ID_SHIFT                          0
#define MT6359_BUCK_VS2_DIG_ID_ADDR                           \
	MT6359_BUCK_VS2_DSN_ID
#define MT6359_BUCK_VS2_DIG_ID_MASK                           0xFF
#define MT6359_BUCK_VS2_DIG_ID_SHIFT                          8
#define MT6359_BUCK_VS2_ANA_MINOR_REV_ADDR                    \
	MT6359_BUCK_VS2_DSN_REV0
#define MT6359_BUCK_VS2_ANA_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VS2_ANA_MINOR_REV_SHIFT                   0
#define MT6359_BUCK_VS2_ANA_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VS2_DSN_REV0
#define MT6359_BUCK_VS2_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VS2_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_BUCK_VS2_DIG_MINOR_REV_ADDR                    \
	MT6359_BUCK_VS2_DSN_REV0
#define MT6359_BUCK_VS2_DIG_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VS2_DIG_MINOR_REV_SHIFT                   8
#define MT6359_BUCK_VS2_DIG_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VS2_DSN_REV0
#define MT6359_BUCK_VS2_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VS2_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_BUCK_VS2_DSN_CBS_ADDR                          \
	MT6359_BUCK_VS2_DSN_DBI
#define MT6359_BUCK_VS2_DSN_CBS_MASK                          0x3
#define MT6359_BUCK_VS2_DSN_CBS_SHIFT                         0
#define MT6359_BUCK_VS2_DSN_BIX_ADDR                          \
	MT6359_BUCK_VS2_DSN_DBI
#define MT6359_BUCK_VS2_DSN_BIX_MASK                          0x3
#define MT6359_BUCK_VS2_DSN_BIX_SHIFT                         2
#define MT6359_BUCK_VS2_DSN_ESP_ADDR                          \
	MT6359_BUCK_VS2_DSN_DBI
#define MT6359_BUCK_VS2_DSN_ESP_MASK                          0xFF
#define MT6359_BUCK_VS2_DSN_ESP_SHIFT                         8
#define MT6359_BUCK_VS2_DSN_FPI_SSHUB_ADDR                    \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_SSHUB_MASK                    0x1
#define MT6359_BUCK_VS2_DSN_FPI_SSHUB_SHIFT                   0
#define MT6359_BUCK_VS2_DSN_FPI_TRACKING_ADDR                 \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_TRACKING_MASK                 0x1
#define MT6359_BUCK_VS2_DSN_FPI_TRACKING_SHIFT                1
#define MT6359_BUCK_VS2_DSN_FPI_PREOC_ADDR                    \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_PREOC_MASK                    0x1
#define MT6359_BUCK_VS2_DSN_FPI_PREOC_SHIFT                   2
#define MT6359_BUCK_VS2_DSN_FPI_VOTER_ADDR                    \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_VOTER_MASK                    0x1
#define MT6359_BUCK_VS2_DSN_FPI_VOTER_SHIFT                   3
#define MT6359_BUCK_VS2_DSN_FPI_ULTRASONIC_ADDR               \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_ULTRASONIC_MASK               0x1
#define MT6359_BUCK_VS2_DSN_FPI_ULTRASONIC_SHIFT              4
#define MT6359_BUCK_VS2_DSN_FPI_DLC_ADDR                      \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_DLC_MASK                      0x1
#define MT6359_BUCK_VS2_DSN_FPI_DLC_SHIFT                     5
#define MT6359_BUCK_VS2_DSN_FPI_TRAP_ADDR                     \
	MT6359_BUCK_VS2_DSN_DXI
#define MT6359_BUCK_VS2_DSN_FPI_TRAP_MASK                     0x1
#define MT6359_BUCK_VS2_DSN_FPI_TRAP_SHIFT                    6
#define MT6359_RG_BUCK_VS2_EN_ADDR                            \
	MT6359_BUCK_VS2_CON0
#define MT6359_RG_BUCK_VS2_EN_MASK                            0x1
#define MT6359_RG_BUCK_VS2_EN_SHIFT                           0
#define MT6359_RG_BUCK_VS2_LP_ADDR                            \
	MT6359_BUCK_VS2_CON0
#define MT6359_RG_BUCK_VS2_LP_MASK                            0x1
#define MT6359_RG_BUCK_VS2_LP_SHIFT                           1
#define MT6359_RG_BUCK_VS2_CON0_SET_ADDR                      \
	MT6359_BUCK_VS2_CON0_SET
#define MT6359_RG_BUCK_VS2_CON0_SET_MASK                      0xFFFF
#define MT6359_RG_BUCK_VS2_CON0_SET_SHIFT                     0
#define MT6359_RG_BUCK_VS2_CON0_CLR_ADDR                      \
	MT6359_BUCK_VS2_CON0_CLR
#define MT6359_RG_BUCK_VS2_CON0_CLR_MASK                      0xFFFF
#define MT6359_RG_BUCK_VS2_CON0_CLR_SHIFT                     0
#define MT6359_RG_BUCK_VS2_VOSEL_SLEEP_ADDR                   \
	MT6359_BUCK_VS2_CON1
#define MT6359_RG_BUCK_VS2_VOSEL_SLEEP_MASK                   0x7F
#define MT6359_RG_BUCK_VS2_VOSEL_SLEEP_SHIFT                  0
#define MT6359_RG_BUCK_VS2_SELR2R_CTRL_ADDR                   \
	MT6359_BUCK_VS2_SLP_CON
#define MT6359_RG_BUCK_VS2_SELR2R_CTRL_MASK                   0x1
#define MT6359_RG_BUCK_VS2_SELR2R_CTRL_SHIFT                  0
#define MT6359_RG_BUCK_VS2_SFCHG_FRATE_ADDR                   \
	MT6359_BUCK_VS2_CFG0
#define MT6359_RG_BUCK_VS2_SFCHG_FRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VS2_SFCHG_FRATE_SHIFT                  0
#define MT6359_RG_BUCK_VS2_SFCHG_FEN_ADDR                     \
	MT6359_BUCK_VS2_CFG0
#define MT6359_RG_BUCK_VS2_SFCHG_FEN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_SFCHG_FEN_SHIFT                    7
#define MT6359_RG_BUCK_VS2_SFCHG_RRATE_ADDR                   \
	MT6359_BUCK_VS2_CFG0
#define MT6359_RG_BUCK_VS2_SFCHG_RRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VS2_SFCHG_RRATE_SHIFT                  8
#define MT6359_RG_BUCK_VS2_SFCHG_REN_ADDR                     \
	MT6359_BUCK_VS2_CFG0
#define MT6359_RG_BUCK_VS2_SFCHG_REN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_SFCHG_REN_SHIFT                    15
#define MT6359_RG_BUCK_VS2_HW0_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_BUCK_VS2_HW1_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_BUCK_VS2_HW2_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_BUCK_VS2_HW3_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_BUCK_VS2_HW4_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_BUCK_VS2_HW5_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_BUCK_VS2_HW6_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_BUCK_VS2_HW7_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_BUCK_VS2_HW8_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_BUCK_VS2_HW9_OP_EN_ADDR                     \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_BUCK_VS2_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_BUCK_VS2_HW10_OP_EN_ADDR                    \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_BUCK_VS2_HW11_OP_EN_ADDR                    \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_BUCK_VS2_HW12_OP_EN_ADDR                    \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_BUCK_VS2_HW13_OP_EN_ADDR                    \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_BUCK_VS2_HW14_OP_EN_ADDR                    \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_BUCK_VS2_SW_OP_EN_ADDR                      \
	MT6359_BUCK_VS2_OP_EN
#define MT6359_RG_BUCK_VS2_SW_OP_EN_MASK                      0x1
#define MT6359_RG_BUCK_VS2_SW_OP_EN_SHIFT                     15
#define MT6359_RG_BUCK_VS2_OP_EN_SET_ADDR                     \
	MT6359_BUCK_VS2_OP_EN_SET
#define MT6359_RG_BUCK_VS2_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_BUCK_VS2_OP_EN_SET_SHIFT                    0
#define MT6359_RG_BUCK_VS2_OP_EN_CLR_ADDR                     \
	MT6359_BUCK_VS2_OP_EN_CLR
#define MT6359_RG_BUCK_VS2_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_BUCK_VS2_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_BUCK_VS2_HW0_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_BUCK_VS2_HW1_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_BUCK_VS2_HW2_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_BUCK_VS2_HW3_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_BUCK_VS2_HW4_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_BUCK_VS2_HW5_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_BUCK_VS2_HW6_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_BUCK_VS2_HW7_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_BUCK_VS2_HW8_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_BUCK_VS2_HW9_OP_CFG_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_BUCK_VS2_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_BUCK_VS2_HW10_OP_CFG_ADDR                   \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_BUCK_VS2_HW11_OP_CFG_ADDR                   \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_BUCK_VS2_HW12_OP_CFG_ADDR                   \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_BUCK_VS2_HW13_OP_CFG_ADDR                   \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_BUCK_VS2_HW14_OP_CFG_ADDR                   \
	MT6359_BUCK_VS2_OP_CFG
#define MT6359_RG_BUCK_VS2_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_BUCK_VS2_OP_CFG_SET_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG_SET
#define MT6359_RG_BUCK_VS2_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_BUCK_VS2_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_BUCK_VS2_OP_CFG_CLR_ADDR                    \
	MT6359_BUCK_VS2_OP_CFG_CLR
#define MT6359_RG_BUCK_VS2_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_BUCK_VS2_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_BUCK_VS2_HW0_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW0_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW0_OP_MODE_SHIFT                  0
#define MT6359_RG_BUCK_VS2_HW1_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW1_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW1_OP_MODE_SHIFT                  1
#define MT6359_RG_BUCK_VS2_HW2_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW2_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW2_OP_MODE_SHIFT                  2
#define MT6359_RG_BUCK_VS2_HW3_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW3_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW3_OP_MODE_SHIFT                  3
#define MT6359_RG_BUCK_VS2_HW4_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW4_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW4_OP_MODE_SHIFT                  4
#define MT6359_RG_BUCK_VS2_HW5_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW5_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW5_OP_MODE_SHIFT                  5
#define MT6359_RG_BUCK_VS2_HW6_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW6_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW6_OP_MODE_SHIFT                  6
#define MT6359_RG_BUCK_VS2_HW7_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW7_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW7_OP_MODE_SHIFT                  7
#define MT6359_RG_BUCK_VS2_HW8_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW8_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW8_OP_MODE_SHIFT                  8
#define MT6359_RG_BUCK_VS2_HW9_OP_MODE_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW9_OP_MODE_MASK                   0x1
#define MT6359_RG_BUCK_VS2_HW9_OP_MODE_SHIFT                  9
#define MT6359_RG_BUCK_VS2_HW10_OP_MODE_ADDR                  \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW10_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS2_HW10_OP_MODE_SHIFT                 10
#define MT6359_RG_BUCK_VS2_HW11_OP_MODE_ADDR                  \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW11_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS2_HW11_OP_MODE_SHIFT                 11
#define MT6359_RG_BUCK_VS2_HW12_OP_MODE_ADDR                  \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW12_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS2_HW12_OP_MODE_SHIFT                 12
#define MT6359_RG_BUCK_VS2_HW13_OP_MODE_ADDR                  \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW13_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS2_HW13_OP_MODE_SHIFT                 13
#define MT6359_RG_BUCK_VS2_HW14_OP_MODE_ADDR                  \
	MT6359_BUCK_VS2_OP_MODE
#define MT6359_RG_BUCK_VS2_HW14_OP_MODE_MASK                  0x1
#define MT6359_RG_BUCK_VS2_HW14_OP_MODE_SHIFT                 14
#define MT6359_RG_BUCK_VS2_OP_MODE_SET_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE_SET
#define MT6359_RG_BUCK_VS2_OP_MODE_SET_MASK                   0xFFFF
#define MT6359_RG_BUCK_VS2_OP_MODE_SET_SHIFT                  0
#define MT6359_RG_BUCK_VS2_OP_MODE_CLR_ADDR                   \
	MT6359_BUCK_VS2_OP_MODE_CLR
#define MT6359_RG_BUCK_VS2_OP_MODE_CLR_MASK                   0xFFFF
#define MT6359_RG_BUCK_VS2_OP_MODE_CLR_SHIFT                  0
#define MT6359_DA_VS2_VOSEL_ADDR                              \
	MT6359_BUCK_VS2_DBG0
#define MT6359_DA_VS2_VOSEL_MASK                              0x7F
#define MT6359_DA_VS2_VOSEL_SHIFT                             0
#define MT6359_DA_VS2_VOSEL_GRAY_ADDR                         \
	MT6359_BUCK_VS2_DBG0
#define MT6359_DA_VS2_VOSEL_GRAY_MASK                         0x7F
#define MT6359_DA_VS2_VOSEL_GRAY_SHIFT                        8
#define MT6359_DA_VS2_EN_ADDR                                 \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_EN_MASK                                 0x1
#define MT6359_DA_VS2_EN_SHIFT                                0
#define MT6359_DA_VS2_STB_ADDR                                \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_STB_MASK                                0x1
#define MT6359_DA_VS2_STB_SHIFT                               1
#define MT6359_DA_VS2_LOOP_SEL_ADDR                           \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_LOOP_SEL_MASK                           0x1
#define MT6359_DA_VS2_LOOP_SEL_SHIFT                          2
#define MT6359_DA_VS2_R2R_PDN_ADDR                            \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_R2R_PDN_MASK                            0x1
#define MT6359_DA_VS2_R2R_PDN_SHIFT                           3
#define MT6359_DA_VS2_DVS_EN_ADDR                             \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_DVS_EN_MASK                             0x1
#define MT6359_DA_VS2_DVS_EN_SHIFT                            4
#define MT6359_DA_VS2_DVS_DOWN_ADDR                           \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_DVS_DOWN_MASK                           0x1
#define MT6359_DA_VS2_DVS_DOWN_SHIFT                          5
#define MT6359_DA_VS2_SSH_ADDR                                \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_SSH_MASK                                0x1
#define MT6359_DA_VS2_SSH_SHIFT                               6
#define MT6359_DA_VS2_MINFREQ_DISCHARGE_ADDR                  \
	MT6359_BUCK_VS2_DBG1
#define MT6359_DA_VS2_MINFREQ_DISCHARGE_MASK                  0x1
#define MT6359_DA_VS2_MINFREQ_DISCHARGE_SHIFT                 8
#define MT6359_RG_BUCK_VS2_CK_SW_MODE_ADDR                    \
	MT6359_BUCK_VS2_DBG1
#define MT6359_RG_BUCK_VS2_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_BUCK_VS2_CK_SW_MODE_SHIFT                   12
#define MT6359_RG_BUCK_VS2_CK_SW_EN_ADDR                      \
	MT6359_BUCK_VS2_DBG1
#define MT6359_RG_BUCK_VS2_CK_SW_EN_MASK                      0x1
#define MT6359_RG_BUCK_VS2_CK_SW_EN_SHIFT                     13
#define MT6359_RG_BUCK_VS2_VOTER_EN_ADDR                      \
	MT6359_BUCK_VS2_VOTER
#define MT6359_RG_BUCK_VS2_VOTER_EN_MASK                      0xFFF
#define MT6359_RG_BUCK_VS2_VOTER_EN_SHIFT                     0
#define MT6359_RG_BUCK_VS2_VOTER_EN_SET_ADDR                  \
	MT6359_BUCK_VS2_VOTER_SET
#define MT6359_RG_BUCK_VS2_VOTER_EN_SET_MASK                  0xFFF
#define MT6359_RG_BUCK_VS2_VOTER_EN_SET_SHIFT                 0
#define MT6359_RG_BUCK_VS2_VOTER_EN_CLR_ADDR                  \
	MT6359_BUCK_VS2_VOTER_CLR
#define MT6359_RG_BUCK_VS2_VOTER_EN_CLR_MASK                  0xFFF
#define MT6359_RG_BUCK_VS2_VOTER_EN_CLR_SHIFT                 0
#define MT6359_RG_BUCK_VS2_VOTER_VOSEL_ADDR                   \
	MT6359_BUCK_VS2_VOTER_CFG
#define MT6359_RG_BUCK_VS2_VOTER_VOSEL_MASK                   0x7F
#define MT6359_RG_BUCK_VS2_VOTER_VOSEL_SHIFT                  0
#define MT6359_BUCK_VS2_ELR_LEN_ADDR                          \
	MT6359_BUCK_VS2_ELR_NUM
#define MT6359_BUCK_VS2_ELR_LEN_MASK                          0xFF
#define MT6359_BUCK_VS2_ELR_LEN_SHIFT                         0
#define MT6359_RG_BUCK_VS2_VOSEL_ADDR                         \
	MT6359_BUCK_VS2_ELR0
#define MT6359_RG_BUCK_VS2_VOSEL_MASK                         0x7F
#define MT6359_RG_BUCK_VS2_VOSEL_SHIFT                        0
#define MT6359_BUCK_VPA_ANA_ID_ADDR                           \
	MT6359_BUCK_VPA_DSN_ID
#define MT6359_BUCK_VPA_ANA_ID_MASK                           0xFF
#define MT6359_BUCK_VPA_ANA_ID_SHIFT                          0
#define MT6359_BUCK_VPA_DIG_ID_ADDR                           \
	MT6359_BUCK_VPA_DSN_ID
#define MT6359_BUCK_VPA_DIG_ID_MASK                           0xFF
#define MT6359_BUCK_VPA_DIG_ID_SHIFT                          8
#define MT6359_BUCK_VPA_ANA_MINOR_REV_ADDR                    \
	MT6359_BUCK_VPA_DSN_REV0
#define MT6359_BUCK_VPA_ANA_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VPA_ANA_MINOR_REV_SHIFT                   0
#define MT6359_BUCK_VPA_ANA_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VPA_DSN_REV0
#define MT6359_BUCK_VPA_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VPA_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_BUCK_VPA_DIG_MINOR_REV_ADDR                    \
	MT6359_BUCK_VPA_DSN_REV0
#define MT6359_BUCK_VPA_DIG_MINOR_REV_MASK                    0xF
#define MT6359_BUCK_VPA_DIG_MINOR_REV_SHIFT                   8
#define MT6359_BUCK_VPA_DIG_MAJOR_REV_ADDR                    \
	MT6359_BUCK_VPA_DSN_REV0
#define MT6359_BUCK_VPA_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_BUCK_VPA_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_BUCK_VPA_DSN_CBS_ADDR                          \
	MT6359_BUCK_VPA_DSN_DBI
#define MT6359_BUCK_VPA_DSN_CBS_MASK                          0x3
#define MT6359_BUCK_VPA_DSN_CBS_SHIFT                         0
#define MT6359_BUCK_VPA_DSN_BIX_ADDR                          \
	MT6359_BUCK_VPA_DSN_DBI
#define MT6359_BUCK_VPA_DSN_BIX_MASK                          0x3
#define MT6359_BUCK_VPA_DSN_BIX_SHIFT                         2
#define MT6359_BUCK_VPA_DSN_ESP_ADDR                          \
	MT6359_BUCK_VPA_DSN_DBI
#define MT6359_BUCK_VPA_DSN_ESP_MASK                          0xFF
#define MT6359_BUCK_VPA_DSN_ESP_SHIFT                         8
#define MT6359_BUCK_VPA_DSN_FPI_SSHUB_ADDR                    \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_SSHUB_MASK                    0x1
#define MT6359_BUCK_VPA_DSN_FPI_SSHUB_SHIFT                   0
#define MT6359_BUCK_VPA_DSN_FPI_TRACKING_ADDR                 \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_TRACKING_MASK                 0x1
#define MT6359_BUCK_VPA_DSN_FPI_TRACKING_SHIFT                1
#define MT6359_BUCK_VPA_DSN_FPI_PREOC_ADDR                    \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_PREOC_MASK                    0x1
#define MT6359_BUCK_VPA_DSN_FPI_PREOC_SHIFT                   2
#define MT6359_BUCK_VPA_DSN_FPI_VOTER_ADDR                    \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_VOTER_MASK                    0x1
#define MT6359_BUCK_VPA_DSN_FPI_VOTER_SHIFT                   3
#define MT6359_BUCK_VPA_DSN_FPI_ULTRASONIC_ADDR               \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_ULTRASONIC_MASK               0x1
#define MT6359_BUCK_VPA_DSN_FPI_ULTRASONIC_SHIFT              4
#define MT6359_BUCK_VPA_DSN_FPI_DLC_ADDR                      \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_DLC_MASK                      0x1
#define MT6359_BUCK_VPA_DSN_FPI_DLC_SHIFT                     5
#define MT6359_BUCK_VPA_DSN_FPI_TRAP_ADDR                     \
	MT6359_BUCK_VPA_DSN_DXI
#define MT6359_BUCK_VPA_DSN_FPI_TRAP_MASK                     0x1
#define MT6359_BUCK_VPA_DSN_FPI_TRAP_SHIFT                    6
#define MT6359_RG_BUCK_VPA_EN_ADDR                            \
	MT6359_BUCK_VPA_CON0
#define MT6359_RG_BUCK_VPA_EN_MASK                            0x1
#define MT6359_RG_BUCK_VPA_EN_SHIFT                           0
#define MT6359_RG_BUCK_VPA_LP_ADDR                            \
	MT6359_BUCK_VPA_CON0
#define MT6359_RG_BUCK_VPA_LP_MASK                            0x1
#define MT6359_RG_BUCK_VPA_LP_SHIFT                           1
#define MT6359_RG_BUCK_VPA_CON0_SET_ADDR                      \
	MT6359_BUCK_VPA_CON0_SET
#define MT6359_RG_BUCK_VPA_CON0_SET_MASK                      0xFFFF
#define MT6359_RG_BUCK_VPA_CON0_SET_SHIFT                     0
#define MT6359_RG_BUCK_VPA_CON0_CLR_ADDR                      \
	MT6359_BUCK_VPA_CON0_CLR
#define MT6359_RG_BUCK_VPA_CON0_CLR_MASK                      0xFFFF
#define MT6359_RG_BUCK_VPA_CON0_CLR_SHIFT                     0
#define MT6359_RG_BUCK_VPA_VOSEL_ADDR                         \
	MT6359_BUCK_VPA_CON1
#define MT6359_RG_BUCK_VPA_VOSEL_MASK                         0x3F
#define MT6359_RG_BUCK_VPA_VOSEL_SHIFT                        0
#define MT6359_RG_BUCK_VPA_SFCHG_FRATE_ADDR                   \
	MT6359_BUCK_VPA_CFG0
#define MT6359_RG_BUCK_VPA_SFCHG_FRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VPA_SFCHG_FRATE_SHIFT                  0
#define MT6359_RG_BUCK_VPA_SFCHG_FEN_ADDR                     \
	MT6359_BUCK_VPA_CFG0
#define MT6359_RG_BUCK_VPA_SFCHG_FEN_MASK                     0x1
#define MT6359_RG_BUCK_VPA_SFCHG_FEN_SHIFT                    7
#define MT6359_RG_BUCK_VPA_SFCHG_RRATE_ADDR                   \
	MT6359_BUCK_VPA_CFG0
#define MT6359_RG_BUCK_VPA_SFCHG_RRATE_MASK                   0x7F
#define MT6359_RG_BUCK_VPA_SFCHG_RRATE_SHIFT                  8
#define MT6359_RG_BUCK_VPA_SFCHG_REN_ADDR                     \
	MT6359_BUCK_VPA_CFG0
#define MT6359_RG_BUCK_VPA_SFCHG_REN_MASK                     0x1
#define MT6359_RG_BUCK_VPA_SFCHG_REN_SHIFT                    15
#define MT6359_RG_BUCK_VPA_DVS_DOWN_CTRL_ADDR                 \
	MT6359_BUCK_VPA_CFG1
#define MT6359_RG_BUCK_VPA_DVS_DOWN_CTRL_MASK                 0x1
#define MT6359_RG_BUCK_VPA_DVS_DOWN_CTRL_SHIFT                0
#define MT6359_DA_VPA_VOSEL_ADDR                              \
	MT6359_BUCK_VPA_DBG0
#define MT6359_DA_VPA_VOSEL_MASK                              0x3F
#define MT6359_DA_VPA_VOSEL_SHIFT                             0
#define MT6359_DA_VPA_VOSEL_GRAY_ADDR                         \
	MT6359_BUCK_VPA_DBG0
#define MT6359_DA_VPA_VOSEL_GRAY_MASK                         0x3F
#define MT6359_DA_VPA_VOSEL_GRAY_SHIFT                        8
#define MT6359_DA_VPA_EN_ADDR                                 \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_EN_MASK                                 0x1
#define MT6359_DA_VPA_EN_SHIFT                                0
#define MT6359_DA_VPA_STB_ADDR                                \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_STB_MASK                                0x1
#define MT6359_DA_VPA_STB_SHIFT                               1
#define MT6359_DA_VPA_DVS_TRANST_ADDR                         \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_DVS_TRANST_MASK                         0x1
#define MT6359_DA_VPA_DVS_TRANST_SHIFT                        5
#define MT6359_DA_VPA_DVS_BW_ADDR                             \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_DVS_BW_MASK                             0x1
#define MT6359_DA_VPA_DVS_BW_SHIFT                            6
#define MT6359_DA_VPA_DVS_DOWN_ADDR                           \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_DVS_DOWN_MASK                           0x1
#define MT6359_DA_VPA_DVS_DOWN_SHIFT                          7
#define MT6359_DA_VPA_MINFREQ_DISCHARGE_ADDR                  \
	MT6359_BUCK_VPA_DBG1
#define MT6359_DA_VPA_MINFREQ_DISCHARGE_MASK                  0x1
#define MT6359_DA_VPA_MINFREQ_DISCHARGE_SHIFT                 8
#define MT6359_RG_BUCK_VPA_CK_SW_MODE_ADDR                    \
	MT6359_BUCK_VPA_DBG1
#define MT6359_RG_BUCK_VPA_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_BUCK_VPA_CK_SW_MODE_SHIFT                   12
#define MT6359_RG_BUCK_VPA_CK_SW_EN_ADDR                      \
	MT6359_BUCK_VPA_DBG1
#define MT6359_RG_BUCK_VPA_CK_SW_EN_MASK                      0x1
#define MT6359_RG_BUCK_VPA_CK_SW_EN_SHIFT                     13
#define MT6359_RG_BUCK_VPA_VOSEL_DLC011_ADDR                  \
	MT6359_BUCK_VPA_DLC_CON0
#define MT6359_RG_BUCK_VPA_VOSEL_DLC011_MASK                  0x3F
#define MT6359_RG_BUCK_VPA_VOSEL_DLC011_SHIFT                 0
#define MT6359_RG_BUCK_VPA_VOSEL_DLC111_ADDR                  \
	MT6359_BUCK_VPA_DLC_CON0
#define MT6359_RG_BUCK_VPA_VOSEL_DLC111_MASK                  0x3F
#define MT6359_RG_BUCK_VPA_VOSEL_DLC111_SHIFT                 8
#define MT6359_RG_BUCK_VPA_VOSEL_DLC001_ADDR                  \
	MT6359_BUCK_VPA_DLC_CON1
#define MT6359_RG_BUCK_VPA_VOSEL_DLC001_MASK                  0x3F
#define MT6359_RG_BUCK_VPA_VOSEL_DLC001_SHIFT                 8
#define MT6359_RG_BUCK_VPA_DLC_MAP_EN_ADDR                    \
	MT6359_BUCK_VPA_DLC_CON2
#define MT6359_RG_BUCK_VPA_DLC_MAP_EN_MASK                    0x1
#define MT6359_RG_BUCK_VPA_DLC_MAP_EN_SHIFT                   0
#define MT6359_RG_BUCK_VPA_DLC_ADDR                           \
	MT6359_BUCK_VPA_DLC_CON2
#define MT6359_RG_BUCK_VPA_DLC_MASK                           0x7
#define MT6359_RG_BUCK_VPA_DLC_SHIFT                          8
#define MT6359_DA_VPA_DLC_ADDR                                \
	MT6359_BUCK_VPA_DLC_CON2
#define MT6359_DA_VPA_DLC_MASK                                0x7
#define MT6359_DA_VPA_DLC_SHIFT                               12
#define MT6359_RG_BUCK_VPA_MSFG_EN_ADDR                       \
	MT6359_BUCK_VPA_MSFG_CON0
#define MT6359_RG_BUCK_VPA_MSFG_EN_MASK                       0x1
#define MT6359_RG_BUCK_VPA_MSFG_EN_SHIFT                      0
#define MT6359_RG_BUCK_VPA_MSFG_RDELTA2GO_ADDR                \
	MT6359_BUCK_VPA_MSFG_CON1
#define MT6359_RG_BUCK_VPA_MSFG_RDELTA2GO_MASK                0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RDELTA2GO_SHIFT               0
#define MT6359_RG_BUCK_VPA_MSFG_FDELTA2GO_ADDR                \
	MT6359_BUCK_VPA_MSFG_CON1
#define MT6359_RG_BUCK_VPA_MSFG_FDELTA2GO_MASK                0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FDELTA2GO_SHIFT               8
#define MT6359_RG_BUCK_VPA_MSFG_RRATE0_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE0
#define MT6359_RG_BUCK_VPA_MSFG_RRATE0_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE0_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_RRATE1_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE0
#define MT6359_RG_BUCK_VPA_MSFG_RRATE1_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE1_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_RRATE2_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE1
#define MT6359_RG_BUCK_VPA_MSFG_RRATE2_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE2_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_RRATE3_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE1
#define MT6359_RG_BUCK_VPA_MSFG_RRATE3_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE3_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_RRATE4_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE2
#define MT6359_RG_BUCK_VPA_MSFG_RRATE4_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE4_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_RRATE5_ADDR                   \
	MT6359_BUCK_VPA_MSFG_RRATE2
#define MT6359_RG_BUCK_VPA_MSFG_RRATE5_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RRATE5_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_RTHD0_ADDR                    \
	MT6359_BUCK_VPA_MSFG_RTHD0
#define MT6359_RG_BUCK_VPA_MSFG_RTHD0_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RTHD0_SHIFT                   0
#define MT6359_RG_BUCK_VPA_MSFG_RTHD1_ADDR                    \
	MT6359_BUCK_VPA_MSFG_RTHD0
#define MT6359_RG_BUCK_VPA_MSFG_RTHD1_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RTHD1_SHIFT                   8
#define MT6359_RG_BUCK_VPA_MSFG_RTHD2_ADDR                    \
	MT6359_BUCK_VPA_MSFG_RTHD1
#define MT6359_RG_BUCK_VPA_MSFG_RTHD2_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RTHD2_SHIFT                   0
#define MT6359_RG_BUCK_VPA_MSFG_RTHD3_ADDR                    \
	MT6359_BUCK_VPA_MSFG_RTHD1
#define MT6359_RG_BUCK_VPA_MSFG_RTHD3_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RTHD3_SHIFT                   8
#define MT6359_RG_BUCK_VPA_MSFG_RTHD4_ADDR                    \
	MT6359_BUCK_VPA_MSFG_RTHD2
#define MT6359_RG_BUCK_VPA_MSFG_RTHD4_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_RTHD4_SHIFT                   0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE0_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE0_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE0_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE1_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE1_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE1_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_FRATE2_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE1
#define MT6359_RG_BUCK_VPA_MSFG_FRATE2_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE2_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE3_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE1
#define MT6359_RG_BUCK_VPA_MSFG_FRATE3_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE3_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_FRATE4_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE2
#define MT6359_RG_BUCK_VPA_MSFG_FRATE4_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE4_SHIFT                  0
#define MT6359_RG_BUCK_VPA_MSFG_FRATE5_ADDR                   \
	MT6359_BUCK_VPA_MSFG_FRATE2
#define MT6359_RG_BUCK_VPA_MSFG_FRATE5_MASK                   0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FRATE5_SHIFT                  8
#define MT6359_RG_BUCK_VPA_MSFG_FTHD0_ADDR                    \
	MT6359_BUCK_VPA_MSFG_FTHD0
#define MT6359_RG_BUCK_VPA_MSFG_FTHD0_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FTHD0_SHIFT                   0
#define MT6359_RG_BUCK_VPA_MSFG_FTHD1_ADDR                    \
	MT6359_BUCK_VPA_MSFG_FTHD0
#define MT6359_RG_BUCK_VPA_MSFG_FTHD1_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FTHD1_SHIFT                   8
#define MT6359_RG_BUCK_VPA_MSFG_FTHD2_ADDR                    \
	MT6359_BUCK_VPA_MSFG_FTHD1
#define MT6359_RG_BUCK_VPA_MSFG_FTHD2_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FTHD2_SHIFT                   0
#define MT6359_RG_BUCK_VPA_MSFG_FTHD3_ADDR                    \
	MT6359_BUCK_VPA_MSFG_FTHD1
#define MT6359_RG_BUCK_VPA_MSFG_FTHD3_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FTHD3_SHIFT                   8
#define MT6359_RG_BUCK_VPA_MSFG_FTHD4_ADDR                    \
	MT6359_BUCK_VPA_MSFG_FTHD2
#define MT6359_RG_BUCK_VPA_MSFG_FTHD4_MASK                    0x3F
#define MT6359_RG_BUCK_VPA_MSFG_FTHD4_SHIFT                   0
#define MT6359_BUCK_ANA0_ANA_ID_ADDR                          \
	MT6359_BUCK_ANA0_DSN_ID
#define MT6359_BUCK_ANA0_ANA_ID_MASK                          0xFF
#define MT6359_BUCK_ANA0_ANA_ID_SHIFT                         0
#define MT6359_BUCK_ANA0_DIG_ID_ADDR                          \
	MT6359_BUCK_ANA0_DSN_ID
#define MT6359_BUCK_ANA0_DIG_ID_MASK                          0xFF
#define MT6359_BUCK_ANA0_DIG_ID_SHIFT                         8
#define MT6359_BUCK_ANA0_ANA_MINOR_REV_ADDR                   \
	MT6359_BUCK_ANA0_DSN_REV0
#define MT6359_BUCK_ANA0_ANA_MINOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA0_ANA_MINOR_REV_SHIFT                  0
#define MT6359_BUCK_ANA0_ANA_MAJOR_REV_ADDR                   \
	MT6359_BUCK_ANA0_DSN_REV0
#define MT6359_BUCK_ANA0_ANA_MAJOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA0_ANA_MAJOR_REV_SHIFT                  4
#define MT6359_BUCK_ANA0_DIG_MINOR_REV_ADDR                   \
	MT6359_BUCK_ANA0_DSN_REV0
#define MT6359_BUCK_ANA0_DIG_MINOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA0_DIG_MINOR_REV_SHIFT                  8
#define MT6359_BUCK_ANA0_DIG_MAJOR_REV_ADDR                   \
	MT6359_BUCK_ANA0_DSN_REV0
#define MT6359_BUCK_ANA0_DIG_MAJOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA0_DIG_MAJOR_REV_SHIFT                  12
#define MT6359_BUCK_ANA0_DSN_CBS_ADDR                         \
	MT6359_BUCK_ANA0_DSN_DBI
#define MT6359_BUCK_ANA0_DSN_CBS_MASK                         0x3
#define MT6359_BUCK_ANA0_DSN_CBS_SHIFT                        0
#define MT6359_BUCK_ANA0_DSN_BIX_ADDR                         \
	MT6359_BUCK_ANA0_DSN_DBI
#define MT6359_BUCK_ANA0_DSN_BIX_MASK                         0x3
#define MT6359_BUCK_ANA0_DSN_BIX_SHIFT                        2
#define MT6359_BUCK_ANA0_DSN_ESP_ADDR                         \
	MT6359_BUCK_ANA0_DSN_DBI
#define MT6359_BUCK_ANA0_DSN_ESP_MASK                         0xFF
#define MT6359_BUCK_ANA0_DSN_ESP_SHIFT                        8
#define MT6359_BUCK_ANA0_DSN_FPI_ADDR                         \
	MT6359_BUCK_ANA0_DSN_FPI
#define MT6359_BUCK_ANA0_DSN_FPI_MASK                         0xFF
#define MT6359_BUCK_ANA0_DSN_FPI_SHIFT                        0
#define MT6359_RG_SMPS_TESTMODE_B_ADDR                        \
	MT6359_SMPS_ANA_CON0
#define MT6359_RG_SMPS_TESTMODE_B_MASK                        0x3F
#define MT6359_RG_SMPS_TESTMODE_B_SHIFT                       0
#define MT6359_RG_AUTOK_RST_ADDR                              \
	MT6359_SMPS_ANA_CON0
#define MT6359_RG_AUTOK_RST_MASK                              0x1
#define MT6359_RG_AUTOK_RST_SHIFT                             6
#define MT6359_RG_SMPS_DISAUTOK_ADDR                          \
	MT6359_SMPS_ANA_CON0
#define MT6359_RG_SMPS_DISAUTOK_MASK                          0x1
#define MT6359_RG_SMPS_DISAUTOK_SHIFT                         7
#define MT6359_RG_VGPU11_NDIS_EN_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_NDIS_EN_MASK                         0x1
#define MT6359_RG_VGPU11_NDIS_EN_SHIFT                        0
#define MT6359_RG_VGPU11_PWM_RSTRAMP_EN_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_PWM_RSTRAMP_EN_MASK                  0x1
#define MT6359_RG_VGPU11_PWM_RSTRAMP_EN_SHIFT                 1
#define MT6359_RG_VGPU11_SLEEP_TIME_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_SLEEP_TIME_MASK                      0x3
#define MT6359_RG_VGPU11_SLEEP_TIME_SHIFT                     2
#define MT6359_RG_VGPU11_LOOPSEL_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_LOOPSEL_DIS_MASK                     0x1
#define MT6359_RG_VGPU11_LOOPSEL_DIS_SHIFT                    4
#define MT6359_RG_VGPU11_TB_DIS_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_TB_DIS_MASK                          0x1
#define MT6359_RG_VGPU11_TB_DIS_SHIFT                         5
#define MT6359_RG_VGPU11_TB_PFM_OFF_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_TB_PFM_OFF_MASK                      0x1
#define MT6359_RG_VGPU11_TB_PFM_OFF_SHIFT                     6
#define MT6359_RG_VGPU11_DUMMY_LOAD_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_DUMMY_LOAD_EN_MASK                   0x1
#define MT6359_RG_VGPU11_DUMMY_LOAD_EN_SHIFT                  7
#define MT6359_RG_VGPU11_TB_VREFSEL_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_TB_VREFSEL_MASK                      0x3
#define MT6359_RG_VGPU11_TB_VREFSEL_SHIFT                     8
#define MT6359_RG_VGPU11_TON_EXTEND_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_TON_EXTEND_EN_MASK                   0x1
#define MT6359_RG_VGPU11_TON_EXTEND_EN_SHIFT                  10
#define MT6359_RG_VGPU11_URT_EN_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_URT_EN_MASK                          0x1
#define MT6359_RG_VGPU11_URT_EN_SHIFT                         11
#define MT6359_RG_VGPU11_OVP_EN_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_OVP_EN_MASK                          0x1
#define MT6359_RG_VGPU11_OVP_EN_SHIFT                         12
#define MT6359_RG_VGPU11_OVP_VREFSEL_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_OVP_VREFSEL_MASK                     0x1
#define MT6359_RG_VGPU11_OVP_VREFSEL_SHIFT                    13
#define MT6359_RG_VGPU11_RAMP_AC_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON0
#define MT6359_RG_VGPU11_RAMP_AC_MASK                         0x1
#define MT6359_RG_VGPU11_RAMP_AC_SHIFT                        14
#define MT6359_RG_VGPU11_OCP_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON1
#define MT6359_RG_VGPU11_OCP_MASK                             0x7
#define MT6359_RG_VGPU11_OCP_SHIFT                            0
#define MT6359_RG_VGPU11_OCN_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON1
#define MT6359_RG_VGPU11_OCN_MASK                             0x7
#define MT6359_RG_VGPU11_OCN_SHIFT                            3
#define MT6359_RG_VGPU11_FUGON_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON1
#define MT6359_RG_VGPU11_FUGON_MASK                           0x1
#define MT6359_RG_VGPU11_FUGON_SHIFT                          6
#define MT6359_RG_VGPU11_FLGON_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON1
#define MT6359_RG_VGPU11_FLGON_MASK                           0x1
#define MT6359_RG_VGPU11_FLGON_SHIFT                          7
#define MT6359_RG_VGPU11_PFM_PEAK_ADDR                        \
	MT6359_VGPUVCORE_ANA_CON1
#define MT6359_RG_VGPU11_PFM_PEAK_MASK                        0xF
#define MT6359_RG_VGPU11_PFM_PEAK_SHIFT                       8
#define MT6359_RG_VGPU11_SONIC_PFM_PEAK_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_SONIC_PFM_PEAK_MASK                  0xF
#define MT6359_RG_VGPU11_SONIC_PFM_PEAK_SHIFT                 0
#define MT6359_RG_VGPU11_VDIFF_GROUNDSEL_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_VDIFF_GROUNDSEL_MASK                 0x1
#define MT6359_RG_VGPU11_VDIFF_GROUNDSEL_SHIFT                4
#define MT6359_RG_VGPU11_UG_SR_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_UG_SR_MASK                           0x3
#define MT6359_RG_VGPU11_UG_SR_SHIFT                          5
#define MT6359_RG_VGPU11_LG_SR_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_LG_SR_MASK                           0x3
#define MT6359_RG_VGPU11_LG_SR_SHIFT                          7
#define MT6359_RG_VGPU11_FCCM_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_FCCM_MASK                            0x1
#define MT6359_RG_VGPU11_FCCM_SHIFT                           9
#define MT6359_RG_VGPU11_RETENTION_EN_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_RETENTION_EN_MASK                    0x1
#define MT6359_RG_VGPU11_RETENTION_EN_SHIFT                   10
#define MT6359_RG_VGPU11_NONAUDIBLE_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON2
#define MT6359_RG_VGPU11_NONAUDIBLE_EN_MASK                   0x1
#define MT6359_RG_VGPU11_NONAUDIBLE_EN_SHIFT                  11
#define MT6359_RG_VGPU11_RSVH_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON3
#define MT6359_RG_VGPU11_RSVH_MASK                            0xFF
#define MT6359_RG_VGPU11_RSVH_SHIFT                           0
#define MT6359_RG_VGPU11_RSVL_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON3
#define MT6359_RG_VGPU11_RSVL_MASK                            0xFF
#define MT6359_RG_VGPU11_RSVL_SHIFT                           8
#define MT6359_RGS_VGPU11_OC_STATUS_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON4
#define MT6359_RGS_VGPU11_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VGPU11_OC_STATUS_SHIFT                     0
#define MT6359_RGS_VGPU11_DIG_MON_ADDR                        \
	MT6359_VGPUVCORE_ANA_CON4
#define MT6359_RGS_VGPU11_DIG_MON_MASK                        0x1
#define MT6359_RGS_VGPU11_DIG_MON_SHIFT                       1
#define MT6359_RG_VGPU11_DIGMON_SEL_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON4
#define MT6359_RG_VGPU11_DIGMON_SEL_MASK                      0x7
#define MT6359_RG_VGPU11_DIGMON_SEL_SHIFT                     2
#define MT6359_RG_VGPU11_VBAT_LOW_DIS_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_VBAT_LOW_DIS_MASK                    0x1
#define MT6359_RG_VGPU11_VBAT_LOW_DIS_SHIFT                   0
#define MT6359_RG_VGPU11_VBAT_HI_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_VBAT_HI_DIS_MASK                     0x1
#define MT6359_RG_VGPU11_VBAT_HI_DIS_SHIFT                    1
#define MT6359_RG_VGPU11_VOUT_HI_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_VOUT_HI_DIS_MASK                     0x1
#define MT6359_RG_VGPU11_VOUT_HI_DIS_SHIFT                    2
#define MT6359_RG_VGPU11_RCB_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_RCB_MASK                             0x7
#define MT6359_RG_VGPU11_RCB_SHIFT                            3
#define MT6359_RG_VGPU11_VDIFF_OFF_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_VDIFF_OFF_MASK                       0x1
#define MT6359_RG_VGPU11_VDIFF_OFF_SHIFT                      6
#define MT6359_RG_VGPU11_VDIFFCAP_EN_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_VDIFFCAP_EN_MASK                     0x1
#define MT6359_RG_VGPU11_VDIFFCAP_EN_SHIFT                    7
#define MT6359_RG_VGPU11_DAC_VREF_1P1V_EN_ADDR                \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_DAC_VREF_1P1V_EN_MASK                0x1
#define MT6359_RG_VGPU11_DAC_VREF_1P1V_EN_SHIFT               8
#define MT6359_RG_VGPU11_DAC_VREF_1P2V_EN_ADDR                \
	MT6359_VGPUVCORE_ANA_CON5
#define MT6359_RG_VGPU11_DAC_VREF_1P2V_EN_MASK                0x1
#define MT6359_RG_VGPU11_DAC_VREF_1P2V_EN_SHIFT               9
#define MT6359_RG_VGPU12_NDIS_EN_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_NDIS_EN_MASK                         0x1
#define MT6359_RG_VGPU12_NDIS_EN_SHIFT                        0
#define MT6359_RG_VGPU12_PWM_RSTRAMP_EN_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_PWM_RSTRAMP_EN_MASK                  0x1
#define MT6359_RG_VGPU12_PWM_RSTRAMP_EN_SHIFT                 1
#define MT6359_RG_VGPU12_SLEEP_TIME_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_SLEEP_TIME_MASK                      0x3
#define MT6359_RG_VGPU12_SLEEP_TIME_SHIFT                     2
#define MT6359_RG_VGPU12_LOOPSEL_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_LOOPSEL_DIS_MASK                     0x1
#define MT6359_RG_VGPU12_LOOPSEL_DIS_SHIFT                    4
#define MT6359_RG_VGPU12_TB_DIS_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_TB_DIS_MASK                          0x1
#define MT6359_RG_VGPU12_TB_DIS_SHIFT                         5
#define MT6359_RG_VGPU12_TB_PFM_OFF_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_TB_PFM_OFF_MASK                      0x1
#define MT6359_RG_VGPU12_TB_PFM_OFF_SHIFT                     6
#define MT6359_RG_VGPU12_TB_VREFSEL_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_TB_VREFSEL_MASK                      0x3
#define MT6359_RG_VGPU12_TB_VREFSEL_SHIFT                     8
#define MT6359_RG_VGPU12_TON_EXTEND_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_TON_EXTEND_EN_MASK                   0x1
#define MT6359_RG_VGPU12_TON_EXTEND_EN_SHIFT                  10
#define MT6359_RG_VGPU12_URT_EN_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_URT_EN_MASK                          0x1
#define MT6359_RG_VGPU12_URT_EN_SHIFT                         11
#define MT6359_RG_VGPU12_DUMMY_LOAD_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_DUMMY_LOAD_EN_MASK                   0x1
#define MT6359_RG_VGPU12_DUMMY_LOAD_EN_SHIFT                  12
#define MT6359_RG_VGPU12_OVP_EN_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_OVP_EN_MASK                          0x1
#define MT6359_RG_VGPU12_OVP_EN_SHIFT                         13
#define MT6359_RG_VGPU12_OVP_VREFSEL_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_OVP_VREFSEL_MASK                     0x1
#define MT6359_RG_VGPU12_OVP_VREFSEL_SHIFT                    14
#define MT6359_RG_VGPU12_RAMP_AC_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON6
#define MT6359_RG_VGPU12_RAMP_AC_MASK                         0x1
#define MT6359_RG_VGPU12_RAMP_AC_SHIFT                        15
#define MT6359_RG_VGPU12_OCP_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON7
#define MT6359_RG_VGPU12_OCP_MASK                             0x7
#define MT6359_RG_VGPU12_OCP_SHIFT                            0
#define MT6359_RG_VGPU12_OCN_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON7
#define MT6359_RG_VGPU12_OCN_MASK                             0x7
#define MT6359_RG_VGPU12_OCN_SHIFT                            3
#define MT6359_RG_VGPU12_PFM_PEAK_ADDR                        \
	MT6359_VGPUVCORE_ANA_CON7
#define MT6359_RG_VGPU12_PFM_PEAK_MASK                        0xF
#define MT6359_RG_VGPU12_PFM_PEAK_SHIFT                       8
#define MT6359_RG_VGPU12_SONIC_PFM_PEAK_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON7
#define MT6359_RG_VGPU12_SONIC_PFM_PEAK_MASK                  0xF
#define MT6359_RG_VGPU12_SONIC_PFM_PEAK_SHIFT                 12
#define MT6359_RG_VGPU12_FLGON_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_FLGON_MASK                           0x1
#define MT6359_RG_VGPU12_FLGON_SHIFT                          0
#define MT6359_RG_VGPU12_FUGON_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_FUGON_MASK                           0x1
#define MT6359_RG_VGPU12_FUGON_SHIFT                          1
#define MT6359_RG_VGPU12_VDIFF_GROUNDSEL_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_VDIFF_GROUNDSEL_MASK                 0x1
#define MT6359_RG_VGPU12_VDIFF_GROUNDSEL_SHIFT                2
#define MT6359_RG_VGPU12_UG_SR_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_UG_SR_MASK                           0x3
#define MT6359_RG_VGPU12_UG_SR_SHIFT                          3
#define MT6359_RG_VGPU12_LG_SR_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_LG_SR_MASK                           0x3
#define MT6359_RG_VGPU12_LG_SR_SHIFT                          5
#define MT6359_RG_VGPU12_FCCM_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_FCCM_MASK                            0x1
#define MT6359_RG_VGPU12_FCCM_SHIFT                           7
#define MT6359_RG_VGPU12_RSVH_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON8
#define MT6359_RG_VGPU12_RSVH_MASK                            0xFF
#define MT6359_RG_VGPU12_RSVH_SHIFT                           8
#define MT6359_RG_VGPU12_RSVL_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RG_VGPU12_RSVL_MASK                            0xFF
#define MT6359_RG_VGPU12_RSVL_SHIFT                           0
#define MT6359_RG_VGPU12_NONAUDIBLE_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RG_VGPU12_NONAUDIBLE_EN_MASK                   0x1
#define MT6359_RG_VGPU12_NONAUDIBLE_EN_SHIFT                  8
#define MT6359_RG_VGPU12_RETENTION_EN_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RG_VGPU12_RETENTION_EN_MASK                    0x1
#define MT6359_RG_VGPU12_RETENTION_EN_SHIFT                   9
#define MT6359_RGS_VGPU12_OC_STATUS_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RGS_VGPU12_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VGPU12_OC_STATUS_SHIFT                     10
#define MT6359_RGS_VGPU12_DIG_MON_ADDR                        \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RGS_VGPU12_DIG_MON_MASK                        0x1
#define MT6359_RGS_VGPU12_DIG_MON_SHIFT                       11
#define MT6359_RG_VGPU12_DIGMON_SEL_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON9
#define MT6359_RG_VGPU12_DIGMON_SEL_MASK                      0x7
#define MT6359_RG_VGPU12_DIGMON_SEL_SHIFT                     12
#define MT6359_RG_VGPU12_RCB_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_RCB_MASK                             0x7
#define MT6359_RG_VGPU12_RCB_SHIFT                            0
#define MT6359_RG_VGPU12_VBAT_HI_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_VBAT_HI_DIS_MASK                     0x1
#define MT6359_RG_VGPU12_VBAT_HI_DIS_SHIFT                    3
#define MT6359_RG_VGPU12_VBAT_LOW_DIS_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_VBAT_LOW_DIS_MASK                    0x1
#define MT6359_RG_VGPU12_VBAT_LOW_DIS_SHIFT                   4
#define MT6359_RG_VGPU12_VOUT_HI_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_VOUT_HI_DIS_MASK                     0x1
#define MT6359_RG_VGPU12_VOUT_HI_DIS_SHIFT                    5
#define MT6359_RG_VGPU12_VDIFF_OFF_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_VDIFF_OFF_MASK                       0x1
#define MT6359_RG_VGPU12_VDIFF_OFF_SHIFT                      6
#define MT6359_RG_VGPU12_VDIFFCAP_EN_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_VDIFFCAP_EN_MASK                     0x1
#define MT6359_RG_VGPU12_VDIFFCAP_EN_SHIFT                    7
#define MT6359_RG_VGPU12_DAC_VREF_1P1V_EN_ADDR                \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_DAC_VREF_1P1V_EN_MASK                0x1
#define MT6359_RG_VGPU12_DAC_VREF_1P1V_EN_SHIFT               8
#define MT6359_RG_VGPU12_DAC_VREF_1P2V_EN_ADDR                \
	MT6359_VGPUVCORE_ANA_CON10
#define MT6359_RG_VGPU12_DAC_VREF_1P2V_EN_MASK                0x1
#define MT6359_RG_VGPU12_DAC_VREF_1P2V_EN_SHIFT               9
#define MT6359_RG_VCORE_TB_DIS_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_TB_DIS_MASK                           0x1
#define MT6359_RG_VCORE_TB_DIS_SHIFT                          0
#define MT6359_RG_VCORE_NDIS_EN_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_NDIS_EN_MASK                          0x1
#define MT6359_RG_VCORE_NDIS_EN_SHIFT                         1
#define MT6359_RG_VCORE_LOOPSEL_DIS_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_LOOPSEL_DIS_MASK                      0x1
#define MT6359_RG_VCORE_LOOPSEL_DIS_SHIFT                     2
#define MT6359_RG_VCORE_PWM_RSTRAMP_EN_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_PWM_RSTRAMP_EN_MASK                   0x1
#define MT6359_RG_VCORE_PWM_RSTRAMP_EN_SHIFT                  3
#define MT6359_RG_VCORE_SLEEP_TIME_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_SLEEP_TIME_MASK                       0x3
#define MT6359_RG_VCORE_SLEEP_TIME_SHIFT                      4
#define MT6359_RG_VCORE_TB_VREFSEL_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_TB_VREFSEL_MASK                       0x3
#define MT6359_RG_VCORE_TB_VREFSEL_SHIFT                      6
#define MT6359_RG_VCORE_TB_PFM_OFF_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_TB_PFM_OFF_MASK                       0x1
#define MT6359_RG_VCORE_TB_PFM_OFF_SHIFT                      8
#define MT6359_RG_VCORE_TON_EXTEND_EN_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_TON_EXTEND_EN_MASK                    0x1
#define MT6359_RG_VCORE_TON_EXTEND_EN_SHIFT                   9
#define MT6359_RG_VCORE_URT_EN_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_URT_EN_MASK                           0x1
#define MT6359_RG_VCORE_URT_EN_SHIFT                          10
#define MT6359_RG_VCORE_DUMMY_LOAD_EN_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_DUMMY_LOAD_EN_MASK                    0x1
#define MT6359_RG_VCORE_DUMMY_LOAD_EN_SHIFT                   11
#define MT6359_RG_VCORE_OVP_EN_ADDR                           \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_OVP_EN_MASK                           0x1
#define MT6359_RG_VCORE_OVP_EN_SHIFT                          12
#define MT6359_RG_VCORE_OVP_VREFSEL_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_OVP_VREFSEL_MASK                      0x1
#define MT6359_RG_VCORE_OVP_VREFSEL_SHIFT                     13
#define MT6359_RG_VCORE_RAMP_AC_ADDR                          \
	MT6359_VGPUVCORE_ANA_CON11
#define MT6359_RG_VCORE_RAMP_AC_MASK                          0x1
#define MT6359_RG_VCORE_RAMP_AC_SHIFT                         14
#define MT6359_RG_VCORE_OCP_ADDR                              \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_OCP_MASK                              0x7
#define MT6359_RG_VCORE_OCP_SHIFT                             0
#define MT6359_RG_VCORE_OCN_ADDR                              \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_OCN_MASK                              0x7
#define MT6359_RG_VCORE_OCN_SHIFT                             3
#define MT6359_RG_VCORE_FUGON_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_FUGON_MASK                            0x1
#define MT6359_RG_VCORE_FUGON_SHIFT                           6
#define MT6359_RG_VCORE_FLGON_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_FLGON_MASK                            0x1
#define MT6359_RG_VCORE_FLGON_SHIFT                           7
#define MT6359_RG_VCORE_PFM_PEAK_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_PFM_PEAK_MASK                         0xF
#define MT6359_RG_VCORE_PFM_PEAK_SHIFT                        8
#define MT6359_RG_VCORE_SONIC_PFM_PEAK_ADDR                   \
	MT6359_VGPUVCORE_ANA_CON12
#define MT6359_RG_VCORE_SONIC_PFM_PEAK_MASK                   0xF
#define MT6359_RG_VCORE_SONIC_PFM_PEAK_SHIFT                  12
#define MT6359_RG_VCORE_UG_SR_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_UG_SR_MASK                            0x3
#define MT6359_RG_VCORE_UG_SR_SHIFT                           0
#define MT6359_RG_VCORE_LG_SR_ADDR                            \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_LG_SR_MASK                            0x3
#define MT6359_RG_VCORE_LG_SR_SHIFT                           2
#define MT6359_RG_VCORE_VDIFF_GROUNDSEL_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_VDIFF_GROUNDSEL_MASK                  0x1
#define MT6359_RG_VCORE_VDIFF_GROUNDSEL_SHIFT                 4
#define MT6359_RG_VCORE_FCCM_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_FCCM_MASK                             0x1
#define MT6359_RG_VCORE_FCCM_SHIFT                            5
#define MT6359_RG_VCORE_NONAUDIBLE_EN_ADDR                    \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_NONAUDIBLE_EN_MASK                    0x1
#define MT6359_RG_VCORE_NONAUDIBLE_EN_SHIFT                   6
#define MT6359_RG_VCORE_RETENTION_EN_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_RETENTION_EN_MASK                     0x1
#define MT6359_RG_VCORE_RETENTION_EN_SHIFT                    7
#define MT6359_RG_VCORE_RSVH_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON13
#define MT6359_RG_VCORE_RSVH_MASK                             0xFF
#define MT6359_RG_VCORE_RSVH_SHIFT                            8
#define MT6359_RG_VCORE_RSVL_ADDR                             \
	MT6359_VGPUVCORE_ANA_CON14
#define MT6359_RG_VCORE_RSVL_MASK                             0xFF
#define MT6359_RG_VCORE_RSVL_SHIFT                            0
#define MT6359_RGS_VCORE_OC_STATUS_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON14
#define MT6359_RGS_VCORE_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VCORE_OC_STATUS_SHIFT                      8
#define MT6359_RGS_VCORE_DIG_MON_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON14
#define MT6359_RGS_VCORE_DIG_MON_MASK                         0x1
#define MT6359_RGS_VCORE_DIG_MON_SHIFT                        9
#define MT6359_RG_VCORE_DIGMON_SEL_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON14
#define MT6359_RG_VCORE_DIGMON_SEL_MASK                       0x7
#define MT6359_RG_VCORE_DIGMON_SEL_SHIFT                      10
#define MT6359_RG_VGPUVCORE_TMDL_ADDR                         \
	MT6359_VGPUVCORE_ANA_CON14
#define MT6359_RG_VGPUVCORE_TMDL_MASK                         0x1
#define MT6359_RG_VGPUVCORE_TMDL_SHIFT                        15
#define MT6359_RG_VCORE_RCB_ADDR                              \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_RCB_MASK                              0x7
#define MT6359_RG_VCORE_RCB_SHIFT                             0
#define MT6359_RG_VCORE_VBAT_LOW_DIS_ADDR                     \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_VBAT_LOW_DIS_MASK                     0x1
#define MT6359_RG_VCORE_VBAT_LOW_DIS_SHIFT                    3
#define MT6359_RG_VCORE_VBAT_HI_DIS_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_VBAT_HI_DIS_MASK                      0x1
#define MT6359_RG_VCORE_VBAT_HI_DIS_SHIFT                     4
#define MT6359_RG_VCORE_VOUT_HI_DIS_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_VOUT_HI_DIS_MASK                      0x1
#define MT6359_RG_VCORE_VOUT_HI_DIS_SHIFT                     5
#define MT6359_RG_VCORE_VDIFF_OFF_ADDR                        \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_VDIFF_OFF_MASK                        0x1
#define MT6359_RG_VCORE_VDIFF_OFF_SHIFT                       6
#define MT6359_RG_VCORE_VDIFFCAP_EN_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_VDIFFCAP_EN_MASK                      0x1
#define MT6359_RG_VCORE_VDIFFCAP_EN_SHIFT                     7
#define MT6359_RG_VCORE_DAC_VREF_1P1V_EN_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_DAC_VREF_1P1V_EN_MASK                 0x1
#define MT6359_RG_VCORE_DAC_VREF_1P1V_EN_SHIFT                8
#define MT6359_RG_VCORE_DAC_VREF_1P2V_EN_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VCORE_DAC_VREF_1P2V_EN_MASK                 0x1
#define MT6359_RG_VCORE_DAC_VREF_1P2V_EN_SHIFT                9
#define MT6359_RG_VGPUVCORE_DIFF_L_ADDR                       \
	MT6359_VGPUVCORE_ANA_CON15
#define MT6359_RG_VGPUVCORE_DIFF_L_MASK                       0x1
#define MT6359_RG_VGPUVCORE_DIFF_L_SHIFT                      10
#define MT6359_RG_VGPUVCORE_SR_VBAT_ADDR                      \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RG_VGPUVCORE_SR_VBAT_MASK                      0x1
#define MT6359_RG_VGPUVCORE_SR_VBAT_SHIFT                     0
#define MT6359_RG_VGPUVCORE_CONFIG_LAT_RSVH_ADDR              \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RG_VGPUVCORE_CONFIG_LAT_RSVH_MASK              0x1
#define MT6359_RG_VGPUVCORE_CONFIG_LAT_RSVH_SHIFT             1
#define MT6359_RG_VGPUVCORE_RECONFIG_RSVH_ADDR                \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RG_VGPUVCORE_RECONFIG_RSVH_MASK                0x1
#define MT6359_RG_VGPUVCORE_RECONFIG_RSVH_SHIFT               2
#define MT6359_RG_VGPUVCORE_RECONFIG_EN_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RG_VGPUVCORE_RECONFIG_EN_MASK                  0x1
#define MT6359_RG_VGPUVCORE_RECONFIG_EN_SHIFT                 3
#define MT6359_RGS_3PH1_VGPU11_DIGCFG_EN_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RGS_3PH1_VGPU11_DIGCFG_EN_MASK                 0x1
#define MT6359_RGS_3PH1_VGPU11_DIGCFG_EN_SHIFT                4
#define MT6359_RGS_3PH2_VCORE_DIGCFG_EN_ADDR                  \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RGS_3PH2_VCORE_DIGCFG_EN_MASK                  0x1
#define MT6359_RGS_3PH2_VCORE_DIGCFG_EN_SHIFT                 5
#define MT6359_RGS_3PH3_VGPU12_DIGCFG_EN_ADDR                 \
	MT6359_VGPUVCORE_ANA_CON16
#define MT6359_RGS_3PH3_VGPU12_DIGCFG_EN_MASK                 0x1
#define MT6359_RGS_3PH3_VGPU12_DIGCFG_EN_SHIFT                6
#define MT6359_RG_VPROC1_SR_VBAT_ADDR                         \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_SR_VBAT_MASK                         0x1
#define MT6359_RG_VPROC1_SR_VBAT_SHIFT                        0
#define MT6359_RG_VPROC1_NDIS_EN_ADDR                         \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_NDIS_EN_MASK                         0x1
#define MT6359_RG_VPROC1_NDIS_EN_SHIFT                        1
#define MT6359_RG_VPROC1_PWM_RSTRAMP_EN_ADDR                  \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_PWM_RSTRAMP_EN_MASK                  0x1
#define MT6359_RG_VPROC1_PWM_RSTRAMP_EN_SHIFT                 2
#define MT6359_RG_VPROC1_SLEEP_TIME_ADDR                      \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_SLEEP_TIME_MASK                      0x3
#define MT6359_RG_VPROC1_SLEEP_TIME_SHIFT                     3
#define MT6359_RG_VPROC1_LOOPSEL_DIS_ADDR                     \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_LOOPSEL_DIS_MASK                     0x1
#define MT6359_RG_VPROC1_LOOPSEL_DIS_SHIFT                    5
#define MT6359_RG_VPROC1_RAMP_AC_ADDR                         \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_RAMP_AC_MASK                         0x1
#define MT6359_RG_VPROC1_RAMP_AC_SHIFT                        6
#define MT6359_RG_VPROC1_TB_DIS_ADDR                          \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_TB_DIS_MASK                          0x1
#define MT6359_RG_VPROC1_TB_DIS_SHIFT                         7
#define MT6359_RG_VPROC1_TB_PFM_OFF_ADDR                      \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_TB_PFM_OFF_MASK                      0x1
#define MT6359_RG_VPROC1_TB_PFM_OFF_SHIFT                     8
#define MT6359_RG_VPROC1_TB_VREFSEL_ADDR                      \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_TB_VREFSEL_MASK                      0x3
#define MT6359_RG_VPROC1_TB_VREFSEL_SHIFT                     9
#define MT6359_RG_VPROC1_TON_EXTEND_EN_ADDR                   \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_TON_EXTEND_EN_MASK                   0x1
#define MT6359_RG_VPROC1_TON_EXTEND_EN_SHIFT                  11
#define MT6359_RG_VPROC1_URT_EN_ADDR                          \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_URT_EN_MASK                          0x1
#define MT6359_RG_VPROC1_URT_EN_SHIFT                         12
#define MT6359_RG_VPROC1_DUMMY_LOAD_EN_ADDR                   \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_DUMMY_LOAD_EN_MASK                   0x1
#define MT6359_RG_VPROC1_DUMMY_LOAD_EN_SHIFT                  13
#define MT6359_RG_VPROC1_OVP_EN_ADDR                          \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_OVP_EN_MASK                          0x1
#define MT6359_RG_VPROC1_OVP_EN_SHIFT                         14
#define MT6359_RG_VPROC1_OVP_VREFSEL_ADDR                     \
	MT6359_VPROC1_ANA_CON0
#define MT6359_RG_VPROC1_OVP_VREFSEL_MASK                     0x1
#define MT6359_RG_VPROC1_OVP_VREFSEL_SHIFT                    15
#define MT6359_RG_VPROC1_OCN_ADDR                             \
	MT6359_VPROC1_ANA_CON1
#define MT6359_RG_VPROC1_OCN_MASK                             0x7
#define MT6359_RG_VPROC1_OCN_SHIFT                            1
#define MT6359_RG_VPROC1_OCP_ADDR                             \
	MT6359_VPROC1_ANA_CON1
#define MT6359_RG_VPROC1_OCP_MASK                             0x7
#define MT6359_RG_VPROC1_OCP_SHIFT                            4
#define MT6359_RG_VPROC1_PFM_PEAK_ADDR                        \
	MT6359_VPROC1_ANA_CON1
#define MT6359_RG_VPROC1_PFM_PEAK_MASK                        0xF
#define MT6359_RG_VPROC1_PFM_PEAK_SHIFT                       7
#define MT6359_RG_VPROC1_SONIC_PFM_PEAK_ADDR                  \
	MT6359_VPROC1_ANA_CON1
#define MT6359_RG_VPROC1_SONIC_PFM_PEAK_MASK                  0xF
#define MT6359_RG_VPROC1_SONIC_PFM_PEAK_SHIFT                 11
#define MT6359_RGS_VPROC1_OC_STATUS_ADDR                      \
	MT6359_VPROC1_ANA_CON1
#define MT6359_RGS_VPROC1_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VPROC1_OC_STATUS_SHIFT                     15
#define MT6359_RGS_VPROC1_DIG_MON_ADDR                        \
	MT6359_VPROC1_ANA_CON2
#define MT6359_RGS_VPROC1_DIG_MON_MASK                        0x1
#define MT6359_RGS_VPROC1_DIG_MON_SHIFT                       9
#define MT6359_RG_VPROC1_UG_SR_ADDR                           \
	MT6359_VPROC1_ANA_CON2
#define MT6359_RG_VPROC1_UG_SR_MASK                           0x3
#define MT6359_RG_VPROC1_UG_SR_SHIFT                          10
#define MT6359_RG_VPROC1_LG_SR_ADDR                           \
	MT6359_VPROC1_ANA_CON2
#define MT6359_RG_VPROC1_LG_SR_MASK                           0x3
#define MT6359_RG_VPROC1_LG_SR_SHIFT                          12
#define MT6359_RG_VPROC1_TMDL_ADDR                            \
	MT6359_VPROC1_ANA_CON2
#define MT6359_RG_VPROC1_TMDL_MASK                            0x1
#define MT6359_RG_VPROC1_TMDL_SHIFT                           14
#define MT6359_RG_VPROC1_FUGON_ADDR                           \
	MT6359_VPROC1_ANA_CON2
#define MT6359_RG_VPROC1_FUGON_MASK                           0x1
#define MT6359_RG_VPROC1_FUGON_SHIFT                          15
#define MT6359_RG_VPROC1_FLGON_ADDR                           \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_FLGON_MASK                           0x1
#define MT6359_RG_VPROC1_FLGON_SHIFT                          0
#define MT6359_RG_VPROC1_FCCM_ADDR                            \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_FCCM_MASK                            0x1
#define MT6359_RG_VPROC1_FCCM_SHIFT                           1
#define MT6359_RG_VPROC1_NONAUDIBLE_EN_ADDR                   \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_NONAUDIBLE_EN_MASK                   0x1
#define MT6359_RG_VPROC1_NONAUDIBLE_EN_SHIFT                  2
#define MT6359_RG_VPROC1_RETENTION_EN_ADDR                    \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_RETENTION_EN_MASK                    0x1
#define MT6359_RG_VPROC1_RETENTION_EN_SHIFT                   3
#define MT6359_RG_VPROC1_VDIFF_GROUNDSEL_ADDR                 \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_VDIFF_GROUNDSEL_MASK                 0x1
#define MT6359_RG_VPROC1_VDIFF_GROUNDSEL_SHIFT                4
#define MT6359_RG_VPROC1_DIGMON_SEL_ADDR                      \
	MT6359_VPROC1_ANA_CON3
#define MT6359_RG_VPROC1_DIGMON_SEL_MASK                      0x7
#define MT6359_RG_VPROC1_DIGMON_SEL_SHIFT                     5
#define MT6359_RG_VPROC1_RSVH_ADDR                            \
	MT6359_VPROC1_ANA_CON4
#define MT6359_RG_VPROC1_RSVH_MASK                            0xFF
#define MT6359_RG_VPROC1_RSVH_SHIFT                           0
#define MT6359_RG_VPROC1_RSVL_ADDR                            \
	MT6359_VPROC1_ANA_CON4
#define MT6359_RG_VPROC1_RSVL_MASK                            0xFF
#define MT6359_RG_VPROC1_RSVL_SHIFT                           8
#define MT6359_RG_VPROC1_RCB_ADDR                             \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_RCB_MASK                             0x7
#define MT6359_RG_VPROC1_RCB_SHIFT                            1
#define MT6359_RG_VPROC1_VDIFFCAP_EN_ADDR                     \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_VDIFFCAP_EN_MASK                     0x1
#define MT6359_RG_VPROC1_VDIFFCAP_EN_SHIFT                    4
#define MT6359_RG_VPROC1_VBAT_HI_DIS_ADDR                     \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_VBAT_HI_DIS_MASK                     0x1
#define MT6359_RG_VPROC1_VBAT_HI_DIS_SHIFT                    5
#define MT6359_RG_VPROC1_VBAT_LOW_DIS_ADDR                    \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_VBAT_LOW_DIS_MASK                    0x1
#define MT6359_RG_VPROC1_VBAT_LOW_DIS_SHIFT                   6
#define MT6359_RG_VPROC1_VOUT_HI_DIS_ADDR                     \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_VOUT_HI_DIS_MASK                     0x1
#define MT6359_RG_VPROC1_VOUT_HI_DIS_SHIFT                    7
#define MT6359_RG_VPROC1_DAC_VREF_1P1V_EN_ADDR                \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_DAC_VREF_1P1V_EN_MASK                0x1
#define MT6359_RG_VPROC1_DAC_VREF_1P1V_EN_SHIFT               8
#define MT6359_RG_VPROC1_DAC_VREF_1P2V_EN_ADDR                \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_DAC_VREF_1P2V_EN_MASK                0x1
#define MT6359_RG_VPROC1_DAC_VREF_1P2V_EN_SHIFT               9
#define MT6359_RG_VPROC1_VDIFF_OFF_ADDR                       \
	MT6359_VPROC1_ANA_CON5
#define MT6359_RG_VPROC1_VDIFF_OFF_MASK                       0x1
#define MT6359_RG_VPROC1_VDIFF_OFF_SHIFT                      10
#define MT6359_BUCK_ANA0_ELR_LEN_ADDR                         \
	MT6359_BUCK_ANA0_ELR_NUM
#define MT6359_BUCK_ANA0_ELR_LEN_MASK                         0xFF
#define MT6359_BUCK_ANA0_ELR_LEN_SHIFT                        0
#define MT6359_RG_VGPU11_DRIVER_SR_TRIM_ADDR                  \
	MT6359_SMPS_ELR_0
#define MT6359_RG_VGPU11_DRIVER_SR_TRIM_MASK                  0x7
#define MT6359_RG_VGPU11_DRIVER_SR_TRIM_SHIFT                 0
#define MT6359_RG_VGPU11_CCOMP_ADDR                           \
	MT6359_SMPS_ELR_0
#define MT6359_RG_VGPU11_CCOMP_MASK                           0x3
#define MT6359_RG_VGPU11_CCOMP_SHIFT                          3
#define MT6359_RG_VGPU11_RCOMP_ADDR                           \
	MT6359_SMPS_ELR_0
#define MT6359_RG_VGPU11_RCOMP_MASK                           0xF
#define MT6359_RG_VGPU11_RCOMP_SHIFT                          5
#define MT6359_RG_VGPU11_RAMP_SLP_ADDR                        \
	MT6359_SMPS_ELR_0
#define MT6359_RG_VGPU11_RAMP_SLP_MASK                        0x7
#define MT6359_RG_VGPU11_RAMP_SLP_SHIFT                       9
#define MT6359_RG_VGPU11_NLIM_TRIM_ADDR                       \
	MT6359_SMPS_ELR_0
#define MT6359_RG_VGPU11_NLIM_TRIM_MASK                       0xF
#define MT6359_RG_VGPU11_NLIM_TRIM_SHIFT                      12
#define MT6359_RG_VGPU12_DRIVER_SR_TRIM_ADDR                  \
	MT6359_SMPS_ELR_1
#define MT6359_RG_VGPU12_DRIVER_SR_TRIM_MASK                  0x7
#define MT6359_RG_VGPU12_DRIVER_SR_TRIM_SHIFT                 0
#define MT6359_RG_VGPU12_CCOMP_ADDR                           \
	MT6359_SMPS_ELR_1
#define MT6359_RG_VGPU12_CCOMP_MASK                           0x3
#define MT6359_RG_VGPU12_CCOMP_SHIFT                          3
#define MT6359_RG_VGPU12_RCOMP_ADDR                           \
	MT6359_SMPS_ELR_1
#define MT6359_RG_VGPU12_RCOMP_MASK                           0xF
#define MT6359_RG_VGPU12_RCOMP_SHIFT                          5
#define MT6359_RG_VGPU12_RAMP_SLP_ADDR                        \
	MT6359_SMPS_ELR_1
#define MT6359_RG_VGPU12_RAMP_SLP_MASK                        0x7
#define MT6359_RG_VGPU12_RAMP_SLP_SHIFT                       9
#define MT6359_RG_VGPU12_NLIM_TRIM_ADDR                       \
	MT6359_SMPS_ELR_1
#define MT6359_RG_VGPU12_NLIM_TRIM_MASK                       0xF
#define MT6359_RG_VGPU12_NLIM_TRIM_SHIFT                      12
#define MT6359_RG_VCORE_DRIVER_SR_TRIM_ADDR                   \
	MT6359_SMPS_ELR_2
#define MT6359_RG_VCORE_DRIVER_SR_TRIM_MASK                   0x7
#define MT6359_RG_VCORE_DRIVER_SR_TRIM_SHIFT                  0
#define MT6359_RG_VCORE_CCOMP_ADDR                            \
	MT6359_SMPS_ELR_2
#define MT6359_RG_VCORE_CCOMP_MASK                            0x3
#define MT6359_RG_VCORE_CCOMP_SHIFT                           3
#define MT6359_RG_VCORE_RCOMP_ADDR                            \
	MT6359_SMPS_ELR_2
#define MT6359_RG_VCORE_RCOMP_MASK                            0xF
#define MT6359_RG_VCORE_RCOMP_SHIFT                           5
#define MT6359_RG_VCORE_RAMP_SLP_ADDR                         \
	MT6359_SMPS_ELR_2
#define MT6359_RG_VCORE_RAMP_SLP_MASK                         0x7
#define MT6359_RG_VCORE_RAMP_SLP_SHIFT                        9
#define MT6359_RG_VCORE_NLIM_TRIM_ADDR                        \
	MT6359_SMPS_ELR_2
#define MT6359_RG_VCORE_NLIM_TRIM_MASK                        0xF
#define MT6359_RG_VCORE_NLIM_TRIM_SHIFT                       12
#define MT6359_RG_VGPU11_CSNSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_3
#define MT6359_RG_VGPU11_CSNSLP_TRIM_MASK                     0xF
#define MT6359_RG_VGPU11_CSNSLP_TRIM_SHIFT                    0
#define MT6359_RG_VGPU11_ZC_TRIM_ADDR                         \
	MT6359_SMPS_ELR_3
#define MT6359_RG_VGPU11_ZC_TRIM_MASK                         0x3
#define MT6359_RG_VGPU11_ZC_TRIM_SHIFT                        4
#define MT6359_RG_VGPU12_CSNSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_4
#define MT6359_RG_VGPU12_CSNSLP_TRIM_MASK                     0xF
#define MT6359_RG_VGPU12_CSNSLP_TRIM_SHIFT                    0
#define MT6359_RG_VGPU12_ZC_TRIM_ADDR                         \
	MT6359_SMPS_ELR_4
#define MT6359_RG_VGPU12_ZC_TRIM_MASK                         0x3
#define MT6359_RG_VGPU12_ZC_TRIM_SHIFT                        4
#define MT6359_RG_VCORE_CSNSLP_TRIM_ADDR                      \
	MT6359_SMPS_ELR_5
#define MT6359_RG_VCORE_CSNSLP_TRIM_MASK                      0xF
#define MT6359_RG_VCORE_CSNSLP_TRIM_SHIFT                     0
#define MT6359_RG_VCORE_ZC_TRIM_ADDR                          \
	MT6359_SMPS_ELR_5
#define MT6359_RG_VCORE_ZC_TRIM_MASK                          0x3
#define MT6359_RG_VCORE_ZC_TRIM_SHIFT                         4
#define MT6359_RG_VGPU11_CSPSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_6
#define MT6359_RG_VGPU11_CSPSLP_TRIM_MASK                     0xF
#define MT6359_RG_VGPU11_CSPSLP_TRIM_SHIFT                    0
#define MT6359_RG_VGPU12_CSPSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_7
#define MT6359_RG_VGPU12_CSPSLP_TRIM_MASK                     0xF
#define MT6359_RG_VGPU12_CSPSLP_TRIM_SHIFT                    0
#define MT6359_RG_VCORE_CSPSLP_TRIM_ADDR                      \
	MT6359_SMPS_ELR_8
#define MT6359_RG_VCORE_CSPSLP_TRIM_MASK                      0xF
#define MT6359_RG_VCORE_CSPSLP_TRIM_SHIFT                     0
#define MT6359_RG_VGPUVCORE_PHIN_TRIM_ADDR                    \
	MT6359_SMPS_ELR_9
#define MT6359_RG_VGPUVCORE_PHIN_TRIM_MASK                    0xF
#define MT6359_RG_VGPUVCORE_PHIN_TRIM_SHIFT                   0
#define MT6359_RG_VPROC1_DRIVER_SR_TRIM_ADDR                  \
	MT6359_SMPS_ELR_10
#define MT6359_RG_VPROC1_DRIVER_SR_TRIM_MASK                  0x7
#define MT6359_RG_VPROC1_DRIVER_SR_TRIM_SHIFT                 0
#define MT6359_RG_VPROC1_CCOMP_ADDR                           \
	MT6359_SMPS_ELR_10
#define MT6359_RG_VPROC1_CCOMP_MASK                           0x3
#define MT6359_RG_VPROC1_CCOMP_SHIFT                          3
#define MT6359_RG_VPROC1_RCOMP_ADDR                           \
	MT6359_SMPS_ELR_10
#define MT6359_RG_VPROC1_RCOMP_MASK                           0xF
#define MT6359_RG_VPROC1_RCOMP_SHIFT                          5
#define MT6359_RG_VPROC1_RAMP_SLP_ADDR                        \
	MT6359_SMPS_ELR_10
#define MT6359_RG_VPROC1_RAMP_SLP_MASK                        0x7
#define MT6359_RG_VPROC1_RAMP_SLP_SHIFT                       9
#define MT6359_RG_VPROC1_NLIM_TRIM_ADDR                       \
	MT6359_SMPS_ELR_10
#define MT6359_RG_VPROC1_NLIM_TRIM_MASK                       0xF
#define MT6359_RG_VPROC1_NLIM_TRIM_SHIFT                      12
#define MT6359_RG_VPROC1_CSNSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_11
#define MT6359_RG_VPROC1_CSNSLP_TRIM_MASK                     0xF
#define MT6359_RG_VPROC1_CSNSLP_TRIM_SHIFT                    0
#define MT6359_RG_VPROC1_ZC_TRIM_ADDR                         \
	MT6359_SMPS_ELR_11
#define MT6359_RG_VPROC1_ZC_TRIM_MASK                         0x3
#define MT6359_RG_VPROC1_ZC_TRIM_SHIFT                        4
#define MT6359_RG_VPROC1_CSPSLP_TRIM_ADDR                     \
	MT6359_SMPS_ELR_12
#define MT6359_RG_VPROC1_CSPSLP_TRIM_MASK                     0xF
#define MT6359_RG_VPROC1_CSPSLP_TRIM_SHIFT                    0
#define MT6359_RG_VS1_TRIMH_ADDR                              \
	MT6359_SMPS_ELR_13
#define MT6359_RG_VS1_TRIMH_MASK                              0xF
#define MT6359_RG_VS1_TRIMH_SHIFT                             0
#define MT6359_RG_VS2_TRIMH_ADDR                              \
	MT6359_SMPS_ELR_13
#define MT6359_RG_VS2_TRIMH_MASK                              0xF
#define MT6359_RG_VS2_TRIMH_SHIFT                             4
#define MT6359_RG_VGPU11_TRIMH_ADDR                           \
	MT6359_SMPS_ELR_13
#define MT6359_RG_VGPU11_TRIMH_MASK                           0xF
#define MT6359_RG_VGPU11_TRIMH_SHIFT                          8
#define MT6359_RG_VGPU12_TRIMH_ADDR                           \
	MT6359_SMPS_ELR_13
#define MT6359_RG_VGPU12_TRIMH_MASK                           0xF
#define MT6359_RG_VGPU12_TRIMH_SHIFT                          12
#define MT6359_RG_VPROC2_TRIMH_ADDR                           \
	MT6359_SMPS_ELR_14
#define MT6359_RG_VPROC2_TRIMH_MASK                           0xF
#define MT6359_RG_VPROC2_TRIMH_SHIFT                          0
#define MT6359_RG_VPROC1_TRIMH_ADDR                           \
	MT6359_SMPS_ELR_14
#define MT6359_RG_VPROC1_TRIMH_MASK                           0xF
#define MT6359_RG_VPROC1_TRIMH_SHIFT                          4
#define MT6359_RG_VCORE_TRIMH_ADDR                            \
	MT6359_SMPS_ELR_14
#define MT6359_RG_VCORE_TRIMH_MASK                            0xF
#define MT6359_RG_VCORE_TRIMH_SHIFT                           8
#define MT6359_RG_VMODEM_TRIMH_ADDR                           \
	MT6359_SMPS_ELR_14
#define MT6359_RG_VMODEM_TRIMH_MASK                           0xF
#define MT6359_RG_VMODEM_TRIMH_SHIFT                          12
#define MT6359_RG_VPA_TRIMH_ADDR                              \
	MT6359_SMPS_ELR_15
#define MT6359_RG_VPA_TRIMH_MASK                              0xF
#define MT6359_RG_VPA_TRIMH_SHIFT                             0
#define MT6359_RG_VPU_TRIMH_ADDR                              \
	MT6359_SMPS_ELR_15
#define MT6359_RG_VPU_TRIMH_MASK                              0xF
#define MT6359_RG_VPU_TRIMH_SHIFT                             4
#define MT6359_RG_VSRAM_PROC1_TRIMH_ADDR                      \
	MT6359_SMPS_ELR_15
#define MT6359_RG_VSRAM_PROC1_TRIMH_MASK                      0x1F
#define MT6359_RG_VSRAM_PROC1_TRIMH_SHIFT                     8
#define MT6359_RG_VSRAM_PROC2_TRIMH_ADDR                      \
	MT6359_SMPS_ELR_16
#define MT6359_RG_VSRAM_PROC2_TRIMH_MASK                      0x1F
#define MT6359_RG_VSRAM_PROC2_TRIMH_SHIFT                     0
#define MT6359_RG_VSRAM_MD_TRIMH_ADDR                         \
	MT6359_SMPS_ELR_16
#define MT6359_RG_VSRAM_MD_TRIMH_MASK                         0x1F
#define MT6359_RG_VSRAM_MD_TRIMH_SHIFT                        5
#define MT6359_RG_VSRAM_OTHERS_TRIMH_ADDR                     \
	MT6359_SMPS_ELR_16
#define MT6359_RG_VSRAM_OTHERS_TRIMH_MASK                     0x1F
#define MT6359_RG_VSRAM_OTHERS_TRIMH_SHIFT                    10
#define MT6359_RG_VGPU11_TON_TRIM_ADDR                        \
	MT6359_SMPS_ELR_17
#define MT6359_RG_VGPU11_TON_TRIM_MASK                        0x3F
#define MT6359_RG_VGPU11_TON_TRIM_SHIFT                       0
#define MT6359_RG_VGPU12_TON_TRIM_ADDR                        \
	MT6359_SMPS_ELR_17
#define MT6359_RG_VGPU12_TON_TRIM_MASK                        0x3F
#define MT6359_RG_VGPU12_TON_TRIM_SHIFT                       6
#define MT6359_RG_VCORE_TON_TRIM_ADDR                         \
	MT6359_SMPS_ELR_18
#define MT6359_RG_VCORE_TON_TRIM_MASK                         0x3F
#define MT6359_RG_VCORE_TON_TRIM_SHIFT                        0
#define MT6359_RG_VGPUVCORE_PH2_OFF_ADDR                      \
	MT6359_SMPS_ELR_18
#define MT6359_RG_VGPUVCORE_PH2_OFF_MASK                      0x1
#define MT6359_RG_VGPUVCORE_PH2_OFF_SHIFT                     6
#define MT6359_RG_VGPUVCORE_PH3_OFF_ADDR                      \
	MT6359_SMPS_ELR_18
#define MT6359_RG_VGPUVCORE_PH3_OFF_MASK                      0x1
#define MT6359_RG_VGPUVCORE_PH3_OFF_SHIFT                     7
#define MT6359_RG_VGPUVCORE_PG_FB3_ADDR                       \
	MT6359_SMPS_ELR_18
#define MT6359_RG_VGPUVCORE_PG_FB3_MASK                       0x1
#define MT6359_RG_VGPUVCORE_PG_FB3_SHIFT                      8
#define MT6359_RG_VPROC1_TON_TRIM_ADDR                        \
	MT6359_SMPS_ELR_18
#define MT6359_RG_VPROC1_TON_TRIM_MASK                        0x3F
#define MT6359_RG_VPROC1_TON_TRIM_SHIFT                       9
#define MT6359_BUCK_ANA1_ANA_ID_ADDR                          \
	MT6359_BUCK_ANA1_DSN_ID
#define MT6359_BUCK_ANA1_ANA_ID_MASK                          0xFF
#define MT6359_BUCK_ANA1_ANA_ID_SHIFT                         0
#define MT6359_BUCK_ANA1_DIG_ID_ADDR                          \
	MT6359_BUCK_ANA1_DSN_ID
#define MT6359_BUCK_ANA1_DIG_ID_MASK                          0xFF
#define MT6359_BUCK_ANA1_DIG_ID_SHIFT                         8
#define MT6359_BUCK_ANA1_ANA_MINOR_REV_ADDR                   \
	MT6359_BUCK_ANA1_DSN_REV0
#define MT6359_BUCK_ANA1_ANA_MINOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA1_ANA_MINOR_REV_SHIFT                  0
#define MT6359_BUCK_ANA1_ANA_MAJOR_REV_ADDR                   \
	MT6359_BUCK_ANA1_DSN_REV0
#define MT6359_BUCK_ANA1_ANA_MAJOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA1_ANA_MAJOR_REV_SHIFT                  4
#define MT6359_BUCK_ANA1_DIG_MINOR_REV_ADDR                   \
	MT6359_BUCK_ANA1_DSN_REV0
#define MT6359_BUCK_ANA1_DIG_MINOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA1_DIG_MINOR_REV_SHIFT                  8
#define MT6359_BUCK_ANA1_DIG_MAJOR_REV_ADDR                   \
	MT6359_BUCK_ANA1_DSN_REV0
#define MT6359_BUCK_ANA1_DIG_MAJOR_REV_MASK                   0xF
#define MT6359_BUCK_ANA1_DIG_MAJOR_REV_SHIFT                  12
#define MT6359_BUCK_ANA1_DSN_CBS_ADDR                         \
	MT6359_BUCK_ANA1_DSN_DBI
#define MT6359_BUCK_ANA1_DSN_CBS_MASK                         0x3
#define MT6359_BUCK_ANA1_DSN_CBS_SHIFT                        0
#define MT6359_BUCK_ANA1_DSN_BIX_ADDR                         \
	MT6359_BUCK_ANA1_DSN_DBI
#define MT6359_BUCK_ANA1_DSN_BIX_MASK                         0x3
#define MT6359_BUCK_ANA1_DSN_BIX_SHIFT                        2
#define MT6359_BUCK_ANA1_DSN_ESP_ADDR                         \
	MT6359_BUCK_ANA1_DSN_DBI
#define MT6359_BUCK_ANA1_DSN_ESP_MASK                         0xFF
#define MT6359_BUCK_ANA1_DSN_ESP_SHIFT                        8
#define MT6359_BUCK_ANA1_DSN_FPI_ADDR                         \
	MT6359_BUCK_ANA1_DSN_FPI
#define MT6359_BUCK_ANA1_DSN_FPI_MASK                         0xFF
#define MT6359_BUCK_ANA1_DSN_FPI_SHIFT                        0
#define MT6359_RG_VPROC2_SR_VBAT_ADDR                         \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_SR_VBAT_MASK                         0x1
#define MT6359_RG_VPROC2_SR_VBAT_SHIFT                        0
#define MT6359_RG_VPROC2_NDIS_EN_ADDR                         \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_NDIS_EN_MASK                         0x1
#define MT6359_RG_VPROC2_NDIS_EN_SHIFT                        1
#define MT6359_RG_VPROC2_PWM_RSTRAMP_EN_ADDR                  \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_PWM_RSTRAMP_EN_MASK                  0x1
#define MT6359_RG_VPROC2_PWM_RSTRAMP_EN_SHIFT                 2
#define MT6359_RG_VPROC2_SLEEP_TIME_ADDR                      \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_SLEEP_TIME_MASK                      0x3
#define MT6359_RG_VPROC2_SLEEP_TIME_SHIFT                     3
#define MT6359_RG_VPROC2_LOOPSEL_DIS_ADDR                     \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_LOOPSEL_DIS_MASK                     0x1
#define MT6359_RG_VPROC2_LOOPSEL_DIS_SHIFT                    5
#define MT6359_RG_VPROC2_RAMP_AC_ADDR                         \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_RAMP_AC_MASK                         0x1
#define MT6359_RG_VPROC2_RAMP_AC_SHIFT                        6
#define MT6359_RG_VPROC2_TB_DIS_ADDR                          \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_TB_DIS_MASK                          0x1
#define MT6359_RG_VPROC2_TB_DIS_SHIFT                         7
#define MT6359_RG_VPROC2_TB_PFM_OFF_ADDR                      \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_TB_PFM_OFF_MASK                      0x1
#define MT6359_RG_VPROC2_TB_PFM_OFF_SHIFT                     8
#define MT6359_RG_VPROC2_TB_VREFSEL_ADDR                      \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_TB_VREFSEL_MASK                      0x3
#define MT6359_RG_VPROC2_TB_VREFSEL_SHIFT                     9
#define MT6359_RG_VPROC2_TON_EXTEND_EN_ADDR                   \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_TON_EXTEND_EN_MASK                   0x1
#define MT6359_RG_VPROC2_TON_EXTEND_EN_SHIFT                  11
#define MT6359_RG_VPROC2_URT_EN_ADDR                          \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_URT_EN_MASK                          0x1
#define MT6359_RG_VPROC2_URT_EN_SHIFT                         12
#define MT6359_RG_VPROC2_DUMMY_LOAD_EN_ADDR                   \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_DUMMY_LOAD_EN_MASK                   0x1
#define MT6359_RG_VPROC2_DUMMY_LOAD_EN_SHIFT                  13
#define MT6359_RG_VPROC2_OVP_EN_ADDR                          \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_OVP_EN_MASK                          0x1
#define MT6359_RG_VPROC2_OVP_EN_SHIFT                         14
#define MT6359_RG_VPROC2_OVP_VREFSEL_ADDR                     \
	MT6359_VPROC2_ANA_CON0
#define MT6359_RG_VPROC2_OVP_VREFSEL_MASK                     0x1
#define MT6359_RG_VPROC2_OVP_VREFSEL_SHIFT                    15
#define MT6359_RG_VPROC2_OCN_ADDR                             \
	MT6359_VPROC2_ANA_CON1
#define MT6359_RG_VPROC2_OCN_MASK                             0x7
#define MT6359_RG_VPROC2_OCN_SHIFT                            1
#define MT6359_RG_VPROC2_OCP_ADDR                             \
	MT6359_VPROC2_ANA_CON1
#define MT6359_RG_VPROC2_OCP_MASK                             0x7
#define MT6359_RG_VPROC2_OCP_SHIFT                            4
#define MT6359_RG_VPROC2_PFM_PEAK_ADDR                        \
	MT6359_VPROC2_ANA_CON1
#define MT6359_RG_VPROC2_PFM_PEAK_MASK                        0xF
#define MT6359_RG_VPROC2_PFM_PEAK_SHIFT                       7
#define MT6359_RG_VPROC2_SONIC_PFM_PEAK_ADDR                  \
	MT6359_VPROC2_ANA_CON1
#define MT6359_RG_VPROC2_SONIC_PFM_PEAK_MASK                  0xF
#define MT6359_RG_VPROC2_SONIC_PFM_PEAK_SHIFT                 11
#define MT6359_RGS_VPROC2_OC_STATUS_ADDR                      \
	MT6359_VPROC2_ANA_CON1
#define MT6359_RGS_VPROC2_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VPROC2_OC_STATUS_SHIFT                     15
#define MT6359_RGS_VPROC2_DIG_MON_ADDR                        \
	MT6359_VPROC2_ANA_CON2
#define MT6359_RGS_VPROC2_DIG_MON_MASK                        0x1
#define MT6359_RGS_VPROC2_DIG_MON_SHIFT                       9
#define MT6359_RG_VPROC2_UG_SR_ADDR                           \
	MT6359_VPROC2_ANA_CON2
#define MT6359_RG_VPROC2_UG_SR_MASK                           0x3
#define MT6359_RG_VPROC2_UG_SR_SHIFT                          10
#define MT6359_RG_VPROC2_LG_SR_ADDR                           \
	MT6359_VPROC2_ANA_CON2
#define MT6359_RG_VPROC2_LG_SR_MASK                           0x3
#define MT6359_RG_VPROC2_LG_SR_SHIFT                          12
#define MT6359_RG_VPROC2_TMDL_ADDR                            \
	MT6359_VPROC2_ANA_CON2
#define MT6359_RG_VPROC2_TMDL_MASK                            0x1
#define MT6359_RG_VPROC2_TMDL_SHIFT                           14
#define MT6359_RG_VPROC2_FUGON_ADDR                           \
	MT6359_VPROC2_ANA_CON2
#define MT6359_RG_VPROC2_FUGON_MASK                           0x1
#define MT6359_RG_VPROC2_FUGON_SHIFT                          15
#define MT6359_RG_VPROC2_FLGON_ADDR                           \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_FLGON_MASK                           0x1
#define MT6359_RG_VPROC2_FLGON_SHIFT                          0
#define MT6359_RG_VPROC2_FCCM_ADDR                            \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_FCCM_MASK                            0x1
#define MT6359_RG_VPROC2_FCCM_SHIFT                           1
#define MT6359_RG_VPROC2_NONAUDIBLE_EN_ADDR                   \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_NONAUDIBLE_EN_MASK                   0x1
#define MT6359_RG_VPROC2_NONAUDIBLE_EN_SHIFT                  2
#define MT6359_RG_VPROC2_RETENTION_EN_ADDR                    \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_RETENTION_EN_MASK                    0x1
#define MT6359_RG_VPROC2_RETENTION_EN_SHIFT                   3
#define MT6359_RG_VPROC2_VDIFF_GROUNDSEL_ADDR                 \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_VDIFF_GROUNDSEL_MASK                 0x1
#define MT6359_RG_VPROC2_VDIFF_GROUNDSEL_SHIFT                4
#define MT6359_RG_VPROC2_DIGMON_SEL_ADDR                      \
	MT6359_VPROC2_ANA_CON3
#define MT6359_RG_VPROC2_DIGMON_SEL_MASK                      0x7
#define MT6359_RG_VPROC2_DIGMON_SEL_SHIFT                     5
#define MT6359_RG_VPROC2_RSVH_ADDR                            \
	MT6359_VPROC2_ANA_CON4
#define MT6359_RG_VPROC2_RSVH_MASK                            0xFF
#define MT6359_RG_VPROC2_RSVH_SHIFT                           0
#define MT6359_RG_VPROC2_RSVL_ADDR                            \
	MT6359_VPROC2_ANA_CON4
#define MT6359_RG_VPROC2_RSVL_MASK                            0xFF
#define MT6359_RG_VPROC2_RSVL_SHIFT                           8
#define MT6359_RG_VPROC2_RCB_ADDR                             \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_RCB_MASK                             0x7
#define MT6359_RG_VPROC2_RCB_SHIFT                            1
#define MT6359_RG_VPROC2_VDIFFCAP_EN_ADDR                     \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_VDIFFCAP_EN_MASK                     0x1
#define MT6359_RG_VPROC2_VDIFFCAP_EN_SHIFT                    4
#define MT6359_RG_VPROC2_VBAT_HI_DIS_ADDR                     \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_VBAT_HI_DIS_MASK                     0x1
#define MT6359_RG_VPROC2_VBAT_HI_DIS_SHIFT                    5
#define MT6359_RG_VPROC2_VBAT_LOW_DIS_ADDR                    \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_VBAT_LOW_DIS_MASK                    0x1
#define MT6359_RG_VPROC2_VBAT_LOW_DIS_SHIFT                   6
#define MT6359_RG_VPROC2_VOUT_HI_DIS_ADDR                     \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_VOUT_HI_DIS_MASK                     0x1
#define MT6359_RG_VPROC2_VOUT_HI_DIS_SHIFT                    7
#define MT6359_RG_VPROC2_DAC_VREF_1P1V_EN_ADDR                \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_DAC_VREF_1P1V_EN_MASK                0x1
#define MT6359_RG_VPROC2_DAC_VREF_1P1V_EN_SHIFT               8
#define MT6359_RG_VPROC2_DAC_VREF_1P2V_EN_ADDR                \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_DAC_VREF_1P2V_EN_MASK                0x1
#define MT6359_RG_VPROC2_DAC_VREF_1P2V_EN_SHIFT               9
#define MT6359_RG_VPROC2_VDIFF_OFF_ADDR                       \
	MT6359_VPROC2_ANA_CON5
#define MT6359_RG_VPROC2_VDIFF_OFF_MASK                       0x1
#define MT6359_RG_VPROC2_VDIFF_OFF_SHIFT                      10
#define MT6359_RG_VMODEM_SR_VBAT_ADDR                         \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_SR_VBAT_MASK                         0x1
#define MT6359_RG_VMODEM_SR_VBAT_SHIFT                        0
#define MT6359_RG_VMODEM_NDIS_EN_ADDR                         \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_NDIS_EN_MASK                         0x1
#define MT6359_RG_VMODEM_NDIS_EN_SHIFT                        1
#define MT6359_RG_VMODEM_PWM_RSTRAMP_EN_ADDR                  \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_PWM_RSTRAMP_EN_MASK                  0x1
#define MT6359_RG_VMODEM_PWM_RSTRAMP_EN_SHIFT                 2
#define MT6359_RG_VMODEM_SLEEP_TIME_ADDR                      \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_SLEEP_TIME_MASK                      0x3
#define MT6359_RG_VMODEM_SLEEP_TIME_SHIFT                     3
#define MT6359_RG_VMODEM_LOOPSEL_DIS_ADDR                     \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_LOOPSEL_DIS_MASK                     0x1
#define MT6359_RG_VMODEM_LOOPSEL_DIS_SHIFT                    5
#define MT6359_RG_VMODEM_RAMP_AC_ADDR                         \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_RAMP_AC_MASK                         0x1
#define MT6359_RG_VMODEM_RAMP_AC_SHIFT                        6
#define MT6359_RG_VMODEM_TB_DIS_ADDR                          \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_TB_DIS_MASK                          0x1
#define MT6359_RG_VMODEM_TB_DIS_SHIFT                         7
#define MT6359_RG_VMODEM_TB_PFM_OFF_ADDR                      \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_TB_PFM_OFF_MASK                      0x1
#define MT6359_RG_VMODEM_TB_PFM_OFF_SHIFT                     8
#define MT6359_RG_VMODEM_TB_VREFSEL_ADDR                      \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_TB_VREFSEL_MASK                      0x3
#define MT6359_RG_VMODEM_TB_VREFSEL_SHIFT                     9
#define MT6359_RG_VMODEM_TON_EXTEND_EN_ADDR                   \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_TON_EXTEND_EN_MASK                   0x1
#define MT6359_RG_VMODEM_TON_EXTEND_EN_SHIFT                  11
#define MT6359_RG_VMODEM_URT_EN_ADDR                          \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_URT_EN_MASK                          0x1
#define MT6359_RG_VMODEM_URT_EN_SHIFT                         12
#define MT6359_RG_VMODEM_DUMMY_LOAD_EN_ADDR                   \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_DUMMY_LOAD_EN_MASK                   0x1
#define MT6359_RG_VMODEM_DUMMY_LOAD_EN_SHIFT                  13
#define MT6359_RG_VMODEM_OVP_EN_ADDR                          \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_OVP_EN_MASK                          0x1
#define MT6359_RG_VMODEM_OVP_EN_SHIFT                         14
#define MT6359_RG_VMODEM_OVP_VREFSEL_ADDR                     \
	MT6359_VMODEM_ANA_CON0
#define MT6359_RG_VMODEM_OVP_VREFSEL_MASK                     0x1
#define MT6359_RG_VMODEM_OVP_VREFSEL_SHIFT                    15
#define MT6359_RG_VMODEM_OCN_ADDR                             \
	MT6359_VMODEM_ANA_CON1
#define MT6359_RG_VMODEM_OCN_MASK                             0x7
#define MT6359_RG_VMODEM_OCN_SHIFT                            1
#define MT6359_RG_VMODEM_OCP_ADDR                             \
	MT6359_VMODEM_ANA_CON1
#define MT6359_RG_VMODEM_OCP_MASK                             0x7
#define MT6359_RG_VMODEM_OCP_SHIFT                            4
#define MT6359_RG_VMODEM_PFM_PEAK_ADDR                        \
	MT6359_VMODEM_ANA_CON1
#define MT6359_RG_VMODEM_PFM_PEAK_MASK                        0xF
#define MT6359_RG_VMODEM_PFM_PEAK_SHIFT                       7
#define MT6359_RG_VMODEM_SONIC_PFM_PEAK_ADDR                  \
	MT6359_VMODEM_ANA_CON1
#define MT6359_RG_VMODEM_SONIC_PFM_PEAK_MASK                  0xF
#define MT6359_RG_VMODEM_SONIC_PFM_PEAK_SHIFT                 11
#define MT6359_RGS_VMODEM_OC_STATUS_ADDR                      \
	MT6359_VMODEM_ANA_CON1
#define MT6359_RGS_VMODEM_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VMODEM_OC_STATUS_SHIFT                     15
#define MT6359_RGS_VMODEM_DIG_MON_ADDR                        \
	MT6359_VMODEM_ANA_CON2
#define MT6359_RGS_VMODEM_DIG_MON_MASK                        0x1
#define MT6359_RGS_VMODEM_DIG_MON_SHIFT                       9
#define MT6359_RG_VMODEM_UG_SR_ADDR                           \
	MT6359_VMODEM_ANA_CON2
#define MT6359_RG_VMODEM_UG_SR_MASK                           0x3
#define MT6359_RG_VMODEM_UG_SR_SHIFT                          10
#define MT6359_RG_VMODEM_LG_SR_ADDR                           \
	MT6359_VMODEM_ANA_CON2
#define MT6359_RG_VMODEM_LG_SR_MASK                           0x3
#define MT6359_RG_VMODEM_LG_SR_SHIFT                          12
#define MT6359_RG_VMODEM_TMDL_ADDR                            \
	MT6359_VMODEM_ANA_CON2
#define MT6359_RG_VMODEM_TMDL_MASK                            0x1
#define MT6359_RG_VMODEM_TMDL_SHIFT                           14
#define MT6359_RG_VMODEM_FUGON_ADDR                           \
	MT6359_VMODEM_ANA_CON2
#define MT6359_RG_VMODEM_FUGON_MASK                           0x1
#define MT6359_RG_VMODEM_FUGON_SHIFT                          15
#define MT6359_RG_VMODEM_FLGON_ADDR                           \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_FLGON_MASK                           0x1
#define MT6359_RG_VMODEM_FLGON_SHIFT                          0
#define MT6359_RG_VMODEM_FCCM_ADDR                            \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_FCCM_MASK                            0x1
#define MT6359_RG_VMODEM_FCCM_SHIFT                           1
#define MT6359_RG_VMODEM_NONAUDIBLE_EN_ADDR                   \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_NONAUDIBLE_EN_MASK                   0x1
#define MT6359_RG_VMODEM_NONAUDIBLE_EN_SHIFT                  2
#define MT6359_RG_VMODEM_RETENTION_EN_ADDR                    \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_RETENTION_EN_MASK                    0x1
#define MT6359_RG_VMODEM_RETENTION_EN_SHIFT                   3
#define MT6359_RG_VMODEM_VDIFF_GROUNDSEL_ADDR                 \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_VDIFF_GROUNDSEL_MASK                 0x1
#define MT6359_RG_VMODEM_VDIFF_GROUNDSEL_SHIFT                4
#define MT6359_RG_VMODEM_DIGMON_SEL_ADDR                      \
	MT6359_VMODEM_ANA_CON3
#define MT6359_RG_VMODEM_DIGMON_SEL_MASK                      0x7
#define MT6359_RG_VMODEM_DIGMON_SEL_SHIFT                     5
#define MT6359_RG_VMODEM_RSVH_ADDR                            \
	MT6359_VMODEM_ANA_CON4
#define MT6359_RG_VMODEM_RSVH_MASK                            0xFF
#define MT6359_RG_VMODEM_RSVH_SHIFT                           0
#define MT6359_RG_VMODEM_RSVL_ADDR                            \
	MT6359_VMODEM_ANA_CON4
#define MT6359_RG_VMODEM_RSVL_MASK                            0xFF
#define MT6359_RG_VMODEM_RSVL_SHIFT                           8
#define MT6359_RG_VMODEM_RCB_ADDR                             \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_RCB_MASK                             0x7
#define MT6359_RG_VMODEM_RCB_SHIFT                            1
#define MT6359_RG_VMODEM_VDIFFCAP_EN_ADDR                     \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_VDIFFCAP_EN_MASK                     0x1
#define MT6359_RG_VMODEM_VDIFFCAP_EN_SHIFT                    4
#define MT6359_RG_VMODEM_VBAT_HI_DIS_ADDR                     \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_VBAT_HI_DIS_MASK                     0x1
#define MT6359_RG_VMODEM_VBAT_HI_DIS_SHIFT                    5
#define MT6359_RG_VMODEM_VBAT_LOW_DIS_ADDR                    \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_VBAT_LOW_DIS_MASK                    0x1
#define MT6359_RG_VMODEM_VBAT_LOW_DIS_SHIFT                   6
#define MT6359_RG_VMODEM_VOUT_HI_DIS_ADDR                     \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_VOUT_HI_DIS_MASK                     0x1
#define MT6359_RG_VMODEM_VOUT_HI_DIS_SHIFT                    7
#define MT6359_RG_VMODEM_DAC_VREF_1P1V_EN_ADDR                \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_DAC_VREF_1P1V_EN_MASK                0x1
#define MT6359_RG_VMODEM_DAC_VREF_1P1V_EN_SHIFT               8
#define MT6359_RG_VMODEM_DAC_VREF_1P2V_EN_ADDR                \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_DAC_VREF_1P2V_EN_MASK                0x1
#define MT6359_RG_VMODEM_DAC_VREF_1P2V_EN_SHIFT               9
#define MT6359_RG_VMODEM_VDIFF_OFF_ADDR                       \
	MT6359_VMODEM_ANA_CON5
#define MT6359_RG_VMODEM_VDIFF_OFF_MASK                       0x1
#define MT6359_RG_VMODEM_VDIFF_OFF_SHIFT                      10
#define MT6359_RG_VPU_SR_VBAT_ADDR                            \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_SR_VBAT_MASK                            0x1
#define MT6359_RG_VPU_SR_VBAT_SHIFT                           0
#define MT6359_RG_VPU_NDIS_EN_ADDR                            \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_NDIS_EN_MASK                            0x1
#define MT6359_RG_VPU_NDIS_EN_SHIFT                           1
#define MT6359_RG_VPU_PWM_RSTRAMP_EN_ADDR                     \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_PWM_RSTRAMP_EN_MASK                     0x1
#define MT6359_RG_VPU_PWM_RSTRAMP_EN_SHIFT                    2
#define MT6359_RG_VPU_SLEEP_TIME_ADDR                         \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_SLEEP_TIME_MASK                         0x3
#define MT6359_RG_VPU_SLEEP_TIME_SHIFT                        3
#define MT6359_RG_VPU_LOOPSEL_DIS_ADDR                        \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_LOOPSEL_DIS_MASK                        0x1
#define MT6359_RG_VPU_LOOPSEL_DIS_SHIFT                       5
#define MT6359_RG_VPU_RAMP_AC_ADDR                            \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_RAMP_AC_MASK                            0x1
#define MT6359_RG_VPU_RAMP_AC_SHIFT                           6
#define MT6359_RG_VPU_TB_DIS_ADDR                             \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_TB_DIS_MASK                             0x1
#define MT6359_RG_VPU_TB_DIS_SHIFT                            7
#define MT6359_RG_VPU_TB_PFM_OFF_ADDR                         \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_TB_PFM_OFF_MASK                         0x1
#define MT6359_RG_VPU_TB_PFM_OFF_SHIFT                        8
#define MT6359_RG_VPU_TB_VREFSEL_ADDR                         \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_TB_VREFSEL_MASK                         0x3
#define MT6359_RG_VPU_TB_VREFSEL_SHIFT                        9
#define MT6359_RG_VPU_TON_EXTEND_EN_ADDR                      \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_TON_EXTEND_EN_MASK                      0x1
#define MT6359_RG_VPU_TON_EXTEND_EN_SHIFT                     11
#define MT6359_RG_VPU_URT_EN_ADDR                             \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_URT_EN_MASK                             0x1
#define MT6359_RG_VPU_URT_EN_SHIFT                            12
#define MT6359_RG_VPU_DUMMY_LOAD_EN_ADDR                      \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_DUMMY_LOAD_EN_MASK                      0x1
#define MT6359_RG_VPU_DUMMY_LOAD_EN_SHIFT                     13
#define MT6359_RG_VPU_OVP_EN_ADDR                             \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_OVP_EN_MASK                             0x1
#define MT6359_RG_VPU_OVP_EN_SHIFT                            14
#define MT6359_RG_VPU_OVP_VREFSEL_ADDR                        \
	MT6359_VPU_ANA_CON0
#define MT6359_RG_VPU_OVP_VREFSEL_MASK                        0x1
#define MT6359_RG_VPU_OVP_VREFSEL_SHIFT                       15
#define MT6359_RG_VPU_OCN_ADDR                                \
	MT6359_VPU_ANA_CON1
#define MT6359_RG_VPU_OCN_MASK                                0x7
#define MT6359_RG_VPU_OCN_SHIFT                               1
#define MT6359_RG_VPU_OCP_ADDR                                \
	MT6359_VPU_ANA_CON1
#define MT6359_RG_VPU_OCP_MASK                                0x7
#define MT6359_RG_VPU_OCP_SHIFT                               4
#define MT6359_RG_VPU_PFM_PEAK_ADDR                           \
	MT6359_VPU_ANA_CON1
#define MT6359_RG_VPU_PFM_PEAK_MASK                           0xF
#define MT6359_RG_VPU_PFM_PEAK_SHIFT                          7
#define MT6359_RG_VPU_SONIC_PFM_PEAK_ADDR                     \
	MT6359_VPU_ANA_CON1
#define MT6359_RG_VPU_SONIC_PFM_PEAK_MASK                     0xF
#define MT6359_RG_VPU_SONIC_PFM_PEAK_SHIFT                    11
#define MT6359_RGS_VPU_OC_STATUS_ADDR                         \
	MT6359_VPU_ANA_CON1
#define MT6359_RGS_VPU_OC_STATUS_MASK                         0x1
#define MT6359_RGS_VPU_OC_STATUS_SHIFT                        15
#define MT6359_RGS_VPU_DIG_MON_ADDR                           \
	MT6359_VPU_ANA_CON2
#define MT6359_RGS_VPU_DIG_MON_MASK                           0x1
#define MT6359_RGS_VPU_DIG_MON_SHIFT                          9
#define MT6359_RG_VPU_UG_SR_ADDR                              \
	MT6359_VPU_ANA_CON2
#define MT6359_RG_VPU_UG_SR_MASK                              0x3
#define MT6359_RG_VPU_UG_SR_SHIFT                             10
#define MT6359_RG_VPU_LG_SR_ADDR                              \
	MT6359_VPU_ANA_CON2
#define MT6359_RG_VPU_LG_SR_MASK                              0x3
#define MT6359_RG_VPU_LG_SR_SHIFT                             12
#define MT6359_RG_VPU_TMDL_ADDR                               \
	MT6359_VPU_ANA_CON2
#define MT6359_RG_VPU_TMDL_MASK                               0x1
#define MT6359_RG_VPU_TMDL_SHIFT                              14
#define MT6359_RG_VPU_FUGON_ADDR                              \
	MT6359_VPU_ANA_CON2
#define MT6359_RG_VPU_FUGON_MASK                              0x1
#define MT6359_RG_VPU_FUGON_SHIFT                             15
#define MT6359_RG_VPU_FLGON_ADDR                              \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_FLGON_MASK                              0x1
#define MT6359_RG_VPU_FLGON_SHIFT                             0
#define MT6359_RG_VPU_FCCM_ADDR                               \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_FCCM_MASK                               0x1
#define MT6359_RG_VPU_FCCM_SHIFT                              1
#define MT6359_RG_VPU_NONAUDIBLE_EN_ADDR                      \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_NONAUDIBLE_EN_MASK                      0x1
#define MT6359_RG_VPU_NONAUDIBLE_EN_SHIFT                     2
#define MT6359_RG_VPU_RETENTION_EN_ADDR                       \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_RETENTION_EN_MASK                       0x1
#define MT6359_RG_VPU_RETENTION_EN_SHIFT                      3
#define MT6359_RG_VPU_VDIFF_GROUNDSEL_ADDR                    \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_VDIFF_GROUNDSEL_MASK                    0x1
#define MT6359_RG_VPU_VDIFF_GROUNDSEL_SHIFT                   4
#define MT6359_RG_VPU_DIGMON_SEL_ADDR                         \
	MT6359_VPU_ANA_CON3
#define MT6359_RG_VPU_DIGMON_SEL_MASK                         0x7
#define MT6359_RG_VPU_DIGMON_SEL_SHIFT                        5
#define MT6359_RG_VPU_RSVH_ADDR                               \
	MT6359_VPU_ANA_CON4
#define MT6359_RG_VPU_RSVH_MASK                               0xFF
#define MT6359_RG_VPU_RSVH_SHIFT                              0
#define MT6359_RG_VPU_RSVL_ADDR                               \
	MT6359_VPU_ANA_CON4
#define MT6359_RG_VPU_RSVL_MASK                               0xFF
#define MT6359_RG_VPU_RSVL_SHIFT                              8
#define MT6359_RG_VPU_RCB_ADDR                                \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_RCB_MASK                                0x7
#define MT6359_RG_VPU_RCB_SHIFT                               1
#define MT6359_RG_VPU_VDIFFCAP_EN_ADDR                        \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_VDIFFCAP_EN_MASK                        0x1
#define MT6359_RG_VPU_VDIFFCAP_EN_SHIFT                       4
#define MT6359_RG_VPU_VBAT_HI_DIS_ADDR                        \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_VBAT_HI_DIS_MASK                        0x1
#define MT6359_RG_VPU_VBAT_HI_DIS_SHIFT                       5
#define MT6359_RG_VPU_VBAT_LOW_DIS_ADDR                       \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_VBAT_LOW_DIS_MASK                       0x1
#define MT6359_RG_VPU_VBAT_LOW_DIS_SHIFT                      6
#define MT6359_RG_VPU_VOUT_HI_DIS_ADDR                        \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_VOUT_HI_DIS_MASK                        0x1
#define MT6359_RG_VPU_VOUT_HI_DIS_SHIFT                       7
#define MT6359_RG_VPU_DAC_VREF_1P1V_EN_ADDR                   \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_DAC_VREF_1P1V_EN_MASK                   0x1
#define MT6359_RG_VPU_DAC_VREF_1P1V_EN_SHIFT                  8
#define MT6359_RG_VPU_DAC_VREF_1P2V_EN_ADDR                   \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_DAC_VREF_1P2V_EN_MASK                   0x1
#define MT6359_RG_VPU_DAC_VREF_1P2V_EN_SHIFT                  9
#define MT6359_RG_VPU_VDIFF_OFF_ADDR                          \
	MT6359_VPU_ANA_CON5
#define MT6359_RG_VPU_VDIFF_OFF_MASK                          0x1
#define MT6359_RG_VPU_VDIFF_OFF_SHIFT                         10
#define MT6359_RG_VS1_TON_TRIM_EN_ADDR                        \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_TON_TRIM_EN_MASK                        0x1
#define MT6359_RG_VS1_TON_TRIM_EN_SHIFT                       1
#define MT6359_RG_VS1_TB_DIS_ADDR                             \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_TB_DIS_MASK                             0x1
#define MT6359_RG_VS1_TB_DIS_SHIFT                            2
#define MT6359_RG_VS1_FPWM_ADDR                               \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_FPWM_MASK                               0x1
#define MT6359_RG_VS1_FPWM_SHIFT                              3
#define MT6359_RG_VS1_PFM_TON_ADDR                            \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_PFM_TON_MASK                            0x7
#define MT6359_RG_VS1_PFM_TON_SHIFT                           4
#define MT6359_RG_VS1_VREF_TRIM_EN_ADDR                       \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_VREF_TRIM_EN_MASK                       0x1
#define MT6359_RG_VS1_VREF_TRIM_EN_SHIFT                      7
#define MT6359_RG_VS1_SLEEP_TIME_ADDR                         \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_SLEEP_TIME_MASK                         0x3
#define MT6359_RG_VS1_SLEEP_TIME_SHIFT                        8
#define MT6359_RG_VS1_NLIM_GATING_ADDR                        \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_NLIM_GATING_MASK                        0x1
#define MT6359_RG_VS1_NLIM_GATING_SHIFT                       10
#define MT6359_RG_VS1_VREFUP_ADDR                             \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_VREFUP_MASK                             0x3
#define MT6359_RG_VS1_VREFUP_SHIFT                            11
#define MT6359_RG_VS1_TB_WIDTH_ADDR                           \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_TB_WIDTH_MASK                           0x3
#define MT6359_RG_VS1_TB_WIDTH_SHIFT                          13
#define MT6359_RG_VS1_VDIFFPFMOFF_ADDR                        \
	MT6359_VS1_ANA_CON0
#define MT6359_RG_VS1_VDIFFPFMOFF_MASK                        0x1
#define MT6359_RG_VS1_VDIFFPFMOFF_SHIFT                       15
#define MT6359_RG_VS1_VDIFF_OFF_ADDR                          \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_VDIFF_OFF_MASK                          0x1
#define MT6359_RG_VS1_VDIFF_OFF_SHIFT                         0
#define MT6359_RG_VS1_UG_SR_ADDR                              \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_UG_SR_MASK                              0x3
#define MT6359_RG_VS1_UG_SR_SHIFT                             1
#define MT6359_RG_VS1_LG_SR_ADDR                              \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_LG_SR_MASK                              0x3
#define MT6359_RG_VS1_LG_SR_SHIFT                             3
#define MT6359_RG_VS1_NDIS_EN_ADDR                            \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_NDIS_EN_MASK                            0x1
#define MT6359_RG_VS1_NDIS_EN_SHIFT                           5
#define MT6359_RG_VS1_TMDL_ADDR                               \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_TMDL_MASK                               0x1
#define MT6359_RG_VS1_TMDL_SHIFT                              6
#define MT6359_RG_VS1_CMPV_FCOT_ADDR                          \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_CMPV_FCOT_MASK                          0x1
#define MT6359_RG_VS1_CMPV_FCOT_SHIFT                         7
#define MT6359_RG_VS1_RSV1_ADDR                               \
	MT6359_VS1_ANA_CON1
#define MT6359_RG_VS1_RSV1_MASK                               0xFF
#define MT6359_RG_VS1_RSV1_SHIFT                              8
#define MT6359_RG_VS1_RSV2_ADDR                               \
	MT6359_VS1_ANA_CON2
#define MT6359_RG_VS1_RSV2_MASK                               0xFF
#define MT6359_RG_VS1_RSV2_SHIFT                              0
#define MT6359_RG_VS1_FUGON_ADDR                              \
	MT6359_VS1_ANA_CON2
#define MT6359_RG_VS1_FUGON_MASK                              0x1
#define MT6359_RG_VS1_FUGON_SHIFT                             8
#define MT6359_RG_VS1_FLGON_ADDR                              \
	MT6359_VS1_ANA_CON2
#define MT6359_RG_VS1_FLGON_MASK                              0x1
#define MT6359_RG_VS1_FLGON_SHIFT                             9
#define MT6359_RGS_VS1_OC_STATUS_ADDR                         \
	MT6359_VS1_ANA_CON2
#define MT6359_RGS_VS1_OC_STATUS_MASK                         0x1
#define MT6359_RGS_VS1_OC_STATUS_SHIFT                        10
#define MT6359_RGS_VS1_DIG_MON_ADDR                           \
	MT6359_VS1_ANA_CON3
#define MT6359_RGS_VS1_DIG_MON_MASK                           0x1
#define MT6359_RGS_VS1_DIG_MON_SHIFT                          0
#define MT6359_RG_VS1_NONAUDIBLE_EN_ADDR                      \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_NONAUDIBLE_EN_MASK                      0x1
#define MT6359_RG_VS1_NONAUDIBLE_EN_SHIFT                     1
#define MT6359_RG_VS1_OCP_ADDR                                \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_OCP_MASK                                0x7
#define MT6359_RG_VS1_OCP_SHIFT                               2
#define MT6359_RG_VS1_OCN_ADDR                                \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_OCN_MASK                                0x7
#define MT6359_RG_VS1_OCN_SHIFT                               5
#define MT6359_RG_VS1_SONIC_PFM_TON_ADDR                      \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_SONIC_PFM_TON_MASK                      0x7
#define MT6359_RG_VS1_SONIC_PFM_TON_SHIFT                     8
#define MT6359_RG_VS1_RETENTION_EN_ADDR                       \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_RETENTION_EN_MASK                       0x1
#define MT6359_RG_VS1_RETENTION_EN_SHIFT                      11
#define MT6359_RG_VS1_DIGMON_SEL_ADDR                         \
	MT6359_VS1_ANA_CON3
#define MT6359_RG_VS1_DIGMON_SEL_MASK                         0x7
#define MT6359_RG_VS1_DIGMON_SEL_SHIFT                        12
#define MT6359_RG_VS2_TON_TRIM_EN_ADDR                        \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_TON_TRIM_EN_MASK                        0x1
#define MT6359_RG_VS2_TON_TRIM_EN_SHIFT                       1
#define MT6359_RG_VS2_TB_DIS_ADDR                             \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_TB_DIS_MASK                             0x1
#define MT6359_RG_VS2_TB_DIS_SHIFT                            2
#define MT6359_RG_VS2_FPWM_ADDR                               \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_FPWM_MASK                               0x1
#define MT6359_RG_VS2_FPWM_SHIFT                              3
#define MT6359_RG_VS2_PFM_TON_ADDR                            \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_PFM_TON_MASK                            0x7
#define MT6359_RG_VS2_PFM_TON_SHIFT                           4
#define MT6359_RG_VS2_VREF_TRIM_EN_ADDR                       \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_VREF_TRIM_EN_MASK                       0x1
#define MT6359_RG_VS2_VREF_TRIM_EN_SHIFT                      7
#define MT6359_RG_VS2_SLEEP_TIME_ADDR                         \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_SLEEP_TIME_MASK                         0x3
#define MT6359_RG_VS2_SLEEP_TIME_SHIFT                        8
#define MT6359_RG_VS2_NLIM_GATING_ADDR                        \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_NLIM_GATING_MASK                        0x1
#define MT6359_RG_VS2_NLIM_GATING_SHIFT                       10
#define MT6359_RG_VS2_VREFUP_ADDR                             \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_VREFUP_MASK                             0x3
#define MT6359_RG_VS2_VREFUP_SHIFT                            11
#define MT6359_RG_VS2_TB_WIDTH_ADDR                           \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_TB_WIDTH_MASK                           0x3
#define MT6359_RG_VS2_TB_WIDTH_SHIFT                          13
#define MT6359_RG_VS2_VDIFFPFMOFF_ADDR                        \
	MT6359_VS2_ANA_CON0
#define MT6359_RG_VS2_VDIFFPFMOFF_MASK                        0x1
#define MT6359_RG_VS2_VDIFFPFMOFF_SHIFT                       15
#define MT6359_RG_VS2_VDIFF_OFF_ADDR                          \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_VDIFF_OFF_MASK                          0x1
#define MT6359_RG_VS2_VDIFF_OFF_SHIFT                         0
#define MT6359_RG_VS2_UG_SR_ADDR                              \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_UG_SR_MASK                              0x3
#define MT6359_RG_VS2_UG_SR_SHIFT                             1
#define MT6359_RG_VS2_LG_SR_ADDR                              \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_LG_SR_MASK                              0x3
#define MT6359_RG_VS2_LG_SR_SHIFT                             3
#define MT6359_RG_VS2_NDIS_EN_ADDR                            \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_NDIS_EN_MASK                            0x1
#define MT6359_RG_VS2_NDIS_EN_SHIFT                           5
#define MT6359_RG_VS2_TMDL_ADDR                               \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_TMDL_MASK                               0x1
#define MT6359_RG_VS2_TMDL_SHIFT                              6
#define MT6359_RG_VS2_CMPV_FCOT_ADDR                          \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_CMPV_FCOT_MASK                          0x1
#define MT6359_RG_VS2_CMPV_FCOT_SHIFT                         7
#define MT6359_RG_VS2_RSV1_ADDR                               \
	MT6359_VS2_ANA_CON1
#define MT6359_RG_VS2_RSV1_MASK                               0xFF
#define MT6359_RG_VS2_RSV1_SHIFT                              8
#define MT6359_RG_VS2_RSV2_ADDR                               \
	MT6359_VS2_ANA_CON2
#define MT6359_RG_VS2_RSV2_MASK                               0xFF
#define MT6359_RG_VS2_RSV2_SHIFT                              0
#define MT6359_RG_VS2_FUGON_ADDR                              \
	MT6359_VS2_ANA_CON2
#define MT6359_RG_VS2_FUGON_MASK                              0x1
#define MT6359_RG_VS2_FUGON_SHIFT                             8
#define MT6359_RG_VS2_FLGON_ADDR                              \
	MT6359_VS2_ANA_CON2
#define MT6359_RG_VS2_FLGON_MASK                              0x1
#define MT6359_RG_VS2_FLGON_SHIFT                             9
#define MT6359_RGS_VS2_OC_STATUS_ADDR                         \
	MT6359_VS2_ANA_CON2
#define MT6359_RGS_VS2_OC_STATUS_MASK                         0x1
#define MT6359_RGS_VS2_OC_STATUS_SHIFT                        10
#define MT6359_RGS_VS2_DIG_MON_ADDR                           \
	MT6359_VS2_ANA_CON3
#define MT6359_RGS_VS2_DIG_MON_MASK                           0x1
#define MT6359_RGS_VS2_DIG_MON_SHIFT                          0
#define MT6359_RG_VS2_NONAUDIBLE_EN_ADDR                      \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_NONAUDIBLE_EN_MASK                      0x1
#define MT6359_RG_VS2_NONAUDIBLE_EN_SHIFT                     1
#define MT6359_RG_VS2_OCP_ADDR                                \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_OCP_MASK                                0x7
#define MT6359_RG_VS2_OCP_SHIFT                               2
#define MT6359_RG_VS2_OCN_ADDR                                \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_OCN_MASK                                0x7
#define MT6359_RG_VS2_OCN_SHIFT                               5
#define MT6359_RG_VS2_SONIC_PFM_TON_ADDR                      \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_SONIC_PFM_TON_MASK                      0x7
#define MT6359_RG_VS2_SONIC_PFM_TON_SHIFT                     8
#define MT6359_RG_VS2_RETENTION_EN_ADDR                       \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_RETENTION_EN_MASK                       0x1
#define MT6359_RG_VS2_RETENTION_EN_SHIFT                      11
#define MT6359_RG_VS2_DIGMON_SEL_ADDR                         \
	MT6359_VS2_ANA_CON3
#define MT6359_RG_VS2_DIGMON_SEL_MASK                         0x7
#define MT6359_RG_VS2_DIGMON_SEL_SHIFT                        12
#define MT6359_RG_VPA_NDIS_EN_ADDR                            \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_NDIS_EN_MASK                            0x1
#define MT6359_RG_VPA_NDIS_EN_SHIFT                           0
#define MT6359_RG_VPA_MODESET_ADDR                            \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_MODESET_MASK                            0x1
#define MT6359_RG_VPA_MODESET_SHIFT                           1
#define MT6359_RG_VPA_CC_ADDR                                 \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_CC_MASK                                 0x3
#define MT6359_RG_VPA_CC_SHIFT                                2
#define MT6359_RG_VPA_CSR_ADDR                                \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_CSR_MASK                                0x3
#define MT6359_RG_VPA_CSR_SHIFT                               4
#define MT6359_RG_VPA_CSMIR_ADDR                              \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_CSMIR_MASK                              0x3
#define MT6359_RG_VPA_CSMIR_SHIFT                             6
#define MT6359_RG_VPA_CSL_ADDR                                \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_CSL_MASK                                0x3
#define MT6359_RG_VPA_CSL_SHIFT                               8
#define MT6359_RG_VPA_SLP_ADDR                                \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_SLP_MASK                                0x3
#define MT6359_RG_VPA_SLP_SHIFT                               10
#define MT6359_RG_VPA_ZXFT_L_ADDR                             \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_ZXFT_L_MASK                             0x1
#define MT6359_RG_VPA_ZXFT_L_SHIFT                            12
#define MT6359_RG_VPA_CP_FWUPOFF_ADDR                         \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_CP_FWUPOFF_MASK                         0x1
#define MT6359_RG_VPA_CP_FWUPOFF_SHIFT                        13
#define MT6359_RG_VPA_NONAUDIBLE_EN_ADDR                      \
	MT6359_VPA_ANA_CON0
#define MT6359_RG_VPA_NONAUDIBLE_EN_MASK                      0x1
#define MT6359_RG_VPA_NONAUDIBLE_EN_SHIFT                     14
#define MT6359_RG_VPA_RZSEL_ADDR                              \
	MT6359_VPA_ANA_CON1
#define MT6359_RG_VPA_RZSEL_MASK                              0x3
#define MT6359_RG_VPA_RZSEL_SHIFT                             0
#define MT6359_RG_VPA_SLEW_ADDR                               \
	MT6359_VPA_ANA_CON1
#define MT6359_RG_VPA_SLEW_MASK                               0x3
#define MT6359_RG_VPA_SLEW_SHIFT                              2
#define MT6359_RG_VPA_SLEW_NMOS_ADDR                          \
	MT6359_VPA_ANA_CON1
#define MT6359_RG_VPA_SLEW_NMOS_MASK                          0x3
#define MT6359_RG_VPA_SLEW_NMOS_SHIFT                         4
#define MT6359_RG_VPA_MIN_ON_ADDR                             \
	MT6359_VPA_ANA_CON1
#define MT6359_RG_VPA_MIN_ON_MASK                             0x3
#define MT6359_RG_VPA_MIN_ON_SHIFT                            6
#define MT6359_RG_VPA_BURST_SEL_ADDR                          \
	MT6359_VPA_ANA_CON1
#define MT6359_RG_VPA_BURST_SEL_MASK                          0x3
#define MT6359_RG_VPA_BURST_SEL_SHIFT                         8
#define MT6359_RG_VPA_ZC_ADDR                                 \
	MT6359_VPA_ANA_CON2
#define MT6359_RG_VPA_ZC_MASK                                 0x3
#define MT6359_RG_VPA_ZC_SHIFT                                0
#define MT6359_RG_VPA_RSV1_ADDR                               \
	MT6359_VPA_ANA_CON2
#define MT6359_RG_VPA_RSV1_MASK                               0xFF
#define MT6359_RG_VPA_RSV1_SHIFT                              8
#define MT6359_RG_VPA_RSV2_ADDR                               \
	MT6359_VPA_ANA_CON3
#define MT6359_RG_VPA_RSV2_MASK                               0xFF
#define MT6359_RG_VPA_RSV2_SHIFT                              0
#define MT6359_RGS_VPA_OC_STATUS_ADDR                         \
	MT6359_VPA_ANA_CON3
#define MT6359_RGS_VPA_OC_STATUS_MASK                         0x1
#define MT6359_RGS_VPA_OC_STATUS_SHIFT                        8
#define MT6359_RGS_VPA_AZC_ZX_ADDR                            \
	MT6359_VPA_ANA_CON3
#define MT6359_RGS_VPA_AZC_ZX_MASK                            0x1
#define MT6359_RGS_VPA_AZC_ZX_SHIFT                           9
#define MT6359_RGS_VPA_DIG_MON_ADDR                           \
	MT6359_VPA_ANA_CON3
#define MT6359_RGS_VPA_DIG_MON_MASK                           0x1
#define MT6359_RGS_VPA_DIG_MON_SHIFT                          10
#define MT6359_RG_VPA_PFM_DLC1_VTH_ADDR                       \
	MT6359_VPA_ANA_CON3
#define MT6359_RG_VPA_PFM_DLC1_VTH_MASK                       0x3
#define MT6359_RG_VPA_PFM_DLC1_VTH_SHIFT                      11
#define MT6359_RG_VPA_PFM_DLC2_VTH_ADDR                       \
	MT6359_VPA_ANA_CON3
#define MT6359_RG_VPA_PFM_DLC2_VTH_MASK                       0x3
#define MT6359_RG_VPA_PFM_DLC2_VTH_SHIFT                      13
#define MT6359_RG_VPA_PFM_DLC3_VTH_ADDR                       \
	MT6359_VPA_ANA_CON4
#define MT6359_RG_VPA_PFM_DLC3_VTH_MASK                       0x3
#define MT6359_RG_VPA_PFM_DLC3_VTH_SHIFT                      0
#define MT6359_RG_VPA_PFM_DLC4_VTH_ADDR                       \
	MT6359_VPA_ANA_CON4
#define MT6359_RG_VPA_PFM_DLC4_VTH_MASK                       0x3
#define MT6359_RG_VPA_PFM_DLC4_VTH_SHIFT                      2
#define MT6359_RG_VPA_ZXFT_H_ADDR                             \
	MT6359_VPA_ANA_CON4
#define MT6359_RG_VPA_ZXFT_H_MASK                             0x1
#define MT6359_RG_VPA_ZXFT_H_SHIFT                            4
#define MT6359_RG_VPA_DECODE_TMB_ADDR                         \
	MT6359_VPA_ANA_CON4
#define MT6359_RG_VPA_DECODE_TMB_MASK                         0x1
#define MT6359_RG_VPA_DECODE_TMB_SHIFT                        5
#define MT6359_RG_VPA_RSV3_ADDR                               \
	MT6359_VPA_ANA_CON4
#define MT6359_RG_VPA_RSV3_MASK                               0xFF
#define MT6359_RG_VPA_RSV3_SHIFT                              8
#define MT6359_BUCK_ANA1_ELR_LEN_ADDR                         \
	MT6359_BUCK_ANA1_ELR_NUM
#define MT6359_BUCK_ANA1_ELR_LEN_MASK                         0xFF
#define MT6359_BUCK_ANA1_ELR_LEN_SHIFT                        0
#define MT6359_RG_VPROC2_DRIVER_SR_TRIM_ADDR                  \
	MT6359_VPROC2_ELR_0
#define MT6359_RG_VPROC2_DRIVER_SR_TRIM_MASK                  0x7
#define MT6359_RG_VPROC2_DRIVER_SR_TRIM_SHIFT                 0
#define MT6359_RG_VPROC2_CCOMP_ADDR                           \
	MT6359_VPROC2_ELR_0
#define MT6359_RG_VPROC2_CCOMP_MASK                           0x3
#define MT6359_RG_VPROC2_CCOMP_SHIFT                          3
#define MT6359_RG_VPROC2_RCOMP_ADDR                           \
	MT6359_VPROC2_ELR_0
#define MT6359_RG_VPROC2_RCOMP_MASK                           0xF
#define MT6359_RG_VPROC2_RCOMP_SHIFT                          5
#define MT6359_RG_VPROC2_RAMP_SLP_ADDR                        \
	MT6359_VPROC2_ELR_0
#define MT6359_RG_VPROC2_RAMP_SLP_MASK                        0x7
#define MT6359_RG_VPROC2_RAMP_SLP_SHIFT                       9
#define MT6359_RG_VPROC2_NLIM_TRIM_ADDR                       \
	MT6359_VPROC2_ELR_0
#define MT6359_RG_VPROC2_NLIM_TRIM_MASK                       0xF
#define MT6359_RG_VPROC2_NLIM_TRIM_SHIFT                      12
#define MT6359_RG_VPROC2_CSNSLP_TRIM_ADDR                     \
	MT6359_VPROC2_ELR_1
#define MT6359_RG_VPROC2_CSNSLP_TRIM_MASK                     0xF
#define MT6359_RG_VPROC2_CSNSLP_TRIM_SHIFT                    0
#define MT6359_RG_VPROC2_ZC_TRIM_ADDR                         \
	MT6359_VPROC2_ELR_1
#define MT6359_RG_VPROC2_ZC_TRIM_MASK                         0x3
#define MT6359_RG_VPROC2_ZC_TRIM_SHIFT                        4
#define MT6359_RG_VPROC2_CSPSLP_TRIM_ADDR                     \
	MT6359_VPROC2_ELR_2
#define MT6359_RG_VPROC2_CSPSLP_TRIM_MASK                     0xF
#define MT6359_RG_VPROC2_CSPSLP_TRIM_SHIFT                    0
#define MT6359_RG_VMODEM_DRIVER_SR_TRIM_ADDR                  \
	MT6359_VPROC2_ELR_3
#define MT6359_RG_VMODEM_DRIVER_SR_TRIM_MASK                  0x7
#define MT6359_RG_VMODEM_DRIVER_SR_TRIM_SHIFT                 0
#define MT6359_RG_VMODEM_CCOMP_ADDR                           \
	MT6359_VPROC2_ELR_3
#define MT6359_RG_VMODEM_CCOMP_MASK                           0x3
#define MT6359_RG_VMODEM_CCOMP_SHIFT                          3
#define MT6359_RG_VMODEM_RCOMP_ADDR                           \
	MT6359_VPROC2_ELR_3
#define MT6359_RG_VMODEM_RCOMP_MASK                           0xF
#define MT6359_RG_VMODEM_RCOMP_SHIFT                          5
#define MT6359_RG_VMODEM_RAMP_SLP_ADDR                        \
	MT6359_VPROC2_ELR_3
#define MT6359_RG_VMODEM_RAMP_SLP_MASK                        0x7
#define MT6359_RG_VMODEM_RAMP_SLP_SHIFT                       9
#define MT6359_RG_VMODEM_NLIM_TRIM_ADDR                       \
	MT6359_VPROC2_ELR_3
#define MT6359_RG_VMODEM_NLIM_TRIM_MASK                       0xF
#define MT6359_RG_VMODEM_NLIM_TRIM_SHIFT                      12
#define MT6359_RG_VMODEM_CSNSLP_TRIM_ADDR                     \
	MT6359_VPROC2_ELR_4
#define MT6359_RG_VMODEM_CSNSLP_TRIM_MASK                     0xF
#define MT6359_RG_VMODEM_CSNSLP_TRIM_SHIFT                    0
#define MT6359_RG_VMODEM_ZC_TRIM_ADDR                         \
	MT6359_VPROC2_ELR_4
#define MT6359_RG_VMODEM_ZC_TRIM_MASK                         0x3
#define MT6359_RG_VMODEM_ZC_TRIM_SHIFT                        4
#define MT6359_RG_VMODEM_CSPSLP_TRIM_ADDR                     \
	MT6359_VPROC2_ELR_5
#define MT6359_RG_VMODEM_CSPSLP_TRIM_MASK                     0xF
#define MT6359_RG_VMODEM_CSPSLP_TRIM_SHIFT                    0
#define MT6359_RG_VPU_DRIVER_SR_TRIM_ADDR                     \
	MT6359_VPROC2_ELR_6
#define MT6359_RG_VPU_DRIVER_SR_TRIM_MASK                     0x7
#define MT6359_RG_VPU_DRIVER_SR_TRIM_SHIFT                    0
#define MT6359_RG_VPU_CCOMP_ADDR                              \
	MT6359_VPROC2_ELR_6
#define MT6359_RG_VPU_CCOMP_MASK                              0x3
#define MT6359_RG_VPU_CCOMP_SHIFT                             3
#define MT6359_RG_VPU_RCOMP_ADDR                              \
	MT6359_VPROC2_ELR_6
#define MT6359_RG_VPU_RCOMP_MASK                              0xF
#define MT6359_RG_VPU_RCOMP_SHIFT                             5
#define MT6359_RG_VPU_RAMP_SLP_ADDR                           \
	MT6359_VPROC2_ELR_6
#define MT6359_RG_VPU_RAMP_SLP_MASK                           0x7
#define MT6359_RG_VPU_RAMP_SLP_SHIFT                          9
#define MT6359_RG_VPU_NLIM_TRIM_ADDR                          \
	MT6359_VPROC2_ELR_6
#define MT6359_RG_VPU_NLIM_TRIM_MASK                          0xF
#define MT6359_RG_VPU_NLIM_TRIM_SHIFT                         12
#define MT6359_RG_VPU_CSNSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_7
#define MT6359_RG_VPU_CSNSLP_TRIM_MASK                        0xF
#define MT6359_RG_VPU_CSNSLP_TRIM_SHIFT                       0
#define MT6359_RG_VPU_ZC_TRIM_ADDR                            \
	MT6359_VPROC2_ELR_7
#define MT6359_RG_VPU_ZC_TRIM_MASK                            0x3
#define MT6359_RG_VPU_ZC_TRIM_SHIFT                           4
#define MT6359_RG_VPU_CSPSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_8
#define MT6359_RG_VPU_CSPSLP_TRIM_MASK                        0xF
#define MT6359_RG_VPU_CSPSLP_TRIM_SHIFT                       0
#define MT6359_RG_VS1_CSNSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_CSNSLP_TRIM_MASK                        0xF
#define MT6359_RG_VS1_CSNSLP_TRIM_SHIFT                       0
#define MT6359_RG_VS1_CCOMP_ADDR                              \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_CCOMP_MASK                              0x3
#define MT6359_RG_VS1_CCOMP_SHIFT                             4
#define MT6359_RG_VS1_RCOMP_ADDR                              \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_RCOMP_MASK                              0xF
#define MT6359_RG_VS1_RCOMP_SHIFT                             6
#define MT6359_RG_VS1_COTRAMP_SLP_ADDR                        \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_COTRAMP_SLP_MASK                        0x7
#define MT6359_RG_VS1_COTRAMP_SLP_SHIFT                       10
#define MT6359_RG_VS1_ZC_TRIM_ADDR                            \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_ZC_TRIM_MASK                            0x3
#define MT6359_RG_VS1_ZC_TRIM_SHIFT                           13
#define MT6359_RG_VS1_LDO_SENSE_ADDR                          \
	MT6359_VPROC2_ELR_9
#define MT6359_RG_VS1_LDO_SENSE_MASK                          0x1
#define MT6359_RG_VS1_LDO_SENSE_SHIFT                         15
#define MT6359_RG_VS1_CSPSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_10
#define MT6359_RG_VS1_CSPSLP_TRIM_MASK                        0xF
#define MT6359_RG_VS1_CSPSLP_TRIM_SHIFT                       0
#define MT6359_RG_VS1_NLIM_TRIM_ADDR                          \
	MT6359_VPROC2_ELR_10
#define MT6359_RG_VS1_NLIM_TRIM_MASK                          0xF
#define MT6359_RG_VS1_NLIM_TRIM_SHIFT                         4
#define MT6359_RG_VS2_CSNSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_CSNSLP_TRIM_MASK                        0xF
#define MT6359_RG_VS2_CSNSLP_TRIM_SHIFT                       0
#define MT6359_RG_VS2_CCOMP_ADDR                              \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_CCOMP_MASK                              0x3
#define MT6359_RG_VS2_CCOMP_SHIFT                             4
#define MT6359_RG_VS2_RCOMP_ADDR                              \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_RCOMP_MASK                              0xF
#define MT6359_RG_VS2_RCOMP_SHIFT                             6
#define MT6359_RG_VS2_COTRAMP_SLP_ADDR                        \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_COTRAMP_SLP_MASK                        0x7
#define MT6359_RG_VS2_COTRAMP_SLP_SHIFT                       10
#define MT6359_RG_VS2_ZC_TRIM_ADDR                            \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_ZC_TRIM_MASK                            0x3
#define MT6359_RG_VS2_ZC_TRIM_SHIFT                           13
#define MT6359_RG_VS2_LDO_SENSE_ADDR                          \
	MT6359_VPROC2_ELR_11
#define MT6359_RG_VS2_LDO_SENSE_MASK                          0x1
#define MT6359_RG_VS2_LDO_SENSE_SHIFT                         15
#define MT6359_RG_VS2_CSPSLP_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_12
#define MT6359_RG_VS2_CSPSLP_TRIM_MASK                        0xF
#define MT6359_RG_VS2_CSPSLP_TRIM_SHIFT                       0
#define MT6359_RG_VS2_NLIM_TRIM_ADDR                          \
	MT6359_VPROC2_ELR_12
#define MT6359_RG_VS2_NLIM_TRIM_MASK                          0xF
#define MT6359_RG_VS2_NLIM_TRIM_SHIFT                         4
#define MT6359_RG_VPROC2_TON_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_13
#define MT6359_RG_VPROC2_TON_TRIM_MASK                        0x3F
#define MT6359_RG_VPROC2_TON_TRIM_SHIFT                       0
#define MT6359_RG_VMODEM_TON_TRIM_ADDR                        \
	MT6359_VPROC2_ELR_13
#define MT6359_RG_VMODEM_TON_TRIM_MASK                        0x3F
#define MT6359_RG_VMODEM_TON_TRIM_SHIFT                       6
#define MT6359_RG_VPU_TON_TRIM_ADDR                           \
	MT6359_VPROC2_ELR_14
#define MT6359_RG_VPU_TON_TRIM_MASK                           0x3F
#define MT6359_RG_VPU_TON_TRIM_SHIFT                          0
#define MT6359_RG_VS1_TON_TRIM_ADDR                           \
	MT6359_VPROC2_ELR_14
#define MT6359_RG_VS1_TON_TRIM_MASK                           0x3F
#define MT6359_RG_VS1_TON_TRIM_SHIFT                          6
#define MT6359_RG_VS2_TON_TRIM_ADDR                           \
	MT6359_VPROC2_ELR_15
#define MT6359_RG_VS2_TON_TRIM_MASK                           0x3F
#define MT6359_RG_VS2_TON_TRIM_SHIFT                          0
#define MT6359_RG_VPA_NLIM_SEL_ADDR                           \
	MT6359_VPROC2_ELR_15
#define MT6359_RG_VPA_NLIM_SEL_MASK                           0xF
#define MT6359_RG_VPA_NLIM_SEL_SHIFT                          6
#define MT6359_LDO_TOP_ANA_ID_ADDR                            \
	MT6359_LDO_TOP_ID
#define MT6359_LDO_TOP_ANA_ID_MASK                            0xFF
#define MT6359_LDO_TOP_ANA_ID_SHIFT                           0
#define MT6359_LDO_TOP_DIG_ID_ADDR                            \
	MT6359_LDO_TOP_ID
#define MT6359_LDO_TOP_DIG_ID_MASK                            0xFF
#define MT6359_LDO_TOP_DIG_ID_SHIFT                           8
#define MT6359_LDO_TOP_ANA_MINOR_REV_ADDR                     \
	MT6359_LDO_TOP_REV0
#define MT6359_LDO_TOP_ANA_MINOR_REV_MASK                     0xF
#define MT6359_LDO_TOP_ANA_MINOR_REV_SHIFT                    0
#define MT6359_LDO_TOP_ANA_MAJOR_REV_ADDR                     \
	MT6359_LDO_TOP_REV0
#define MT6359_LDO_TOP_ANA_MAJOR_REV_MASK                     0xF
#define MT6359_LDO_TOP_ANA_MAJOR_REV_SHIFT                    4
#define MT6359_LDO_TOP_DIG_MINOR_REV_ADDR                     \
	MT6359_LDO_TOP_REV0
#define MT6359_LDO_TOP_DIG_MINOR_REV_MASK                     0xF
#define MT6359_LDO_TOP_DIG_MINOR_REV_SHIFT                    8
#define MT6359_LDO_TOP_DIG_MAJOR_REV_ADDR                     \
	MT6359_LDO_TOP_REV0
#define MT6359_LDO_TOP_DIG_MAJOR_REV_MASK                     0xF
#define MT6359_LDO_TOP_DIG_MAJOR_REV_SHIFT                    12
#define MT6359_LDO_TOP_CBS_ADDR                               \
	MT6359_LDO_TOP_DBI
#define MT6359_LDO_TOP_CBS_MASK                               0x3
#define MT6359_LDO_TOP_CBS_SHIFT                              0
#define MT6359_LDO_TOP_BIX_ADDR                               \
	MT6359_LDO_TOP_DBI
#define MT6359_LDO_TOP_BIX_MASK                               0x3
#define MT6359_LDO_TOP_BIX_SHIFT                              2
#define MT6359_LDO_TOP_ESP_ADDR                               \
	MT6359_LDO_TOP_DBI
#define MT6359_LDO_TOP_ESP_MASK                               0xFF
#define MT6359_LDO_TOP_ESP_SHIFT                              8
#define MT6359_LDO_TOP_FPI_ADDR                               \
	MT6359_LDO_TOP_DXI
#define MT6359_LDO_TOP_FPI_MASK                               0xFF
#define MT6359_LDO_TOP_FPI_SHIFT                              0
#define MT6359_LDO_TOP_CLK_OFFSET_ADDR                        \
	MT6359_LDO_TPM0
#define MT6359_LDO_TOP_CLK_OFFSET_MASK                        0xFF
#define MT6359_LDO_TOP_CLK_OFFSET_SHIFT                       0
#define MT6359_LDO_TOP_RST_OFFSET_ADDR                        \
	MT6359_LDO_TPM0
#define MT6359_LDO_TOP_RST_OFFSET_MASK                        0xFF
#define MT6359_LDO_TOP_RST_OFFSET_SHIFT                       8
#define MT6359_LDO_TOP_INT_OFFSET_ADDR                        \
	MT6359_LDO_TPM1
#define MT6359_LDO_TOP_INT_OFFSET_MASK                        0xFF
#define MT6359_LDO_TOP_INT_OFFSET_SHIFT                       0
#define MT6359_LDO_TOP_INT_LEN_ADDR                           \
	MT6359_LDO_TPM1
#define MT6359_LDO_TOP_INT_LEN_MASK                           0xFF
#define MT6359_LDO_TOP_INT_LEN_SHIFT                          8
#define MT6359_RG_LDO_32K_CK_PDN_ADDR                         \
	MT6359_LDO_TOP_CKPDN_CON0
#define MT6359_RG_LDO_32K_CK_PDN_MASK                         0x1
#define MT6359_RG_LDO_32K_CK_PDN_SHIFT                        0
#define MT6359_RG_LDO_INTRP_CK_PDN_ADDR                       \
	MT6359_LDO_TOP_CKPDN_CON0
#define MT6359_RG_LDO_INTRP_CK_PDN_MASK                       0x1
#define MT6359_RG_LDO_INTRP_CK_PDN_SHIFT                      1
#define MT6359_RG_LDO_1M_CK_PDN_ADDR                          \
	MT6359_LDO_TOP_CKPDN_CON0
#define MT6359_RG_LDO_1M_CK_PDN_MASK                          0x1
#define MT6359_RG_LDO_1M_CK_PDN_SHIFT                         2
#define MT6359_RG_LDO_26M_CK_PDN_ADDR                         \
	MT6359_LDO_TOP_CKPDN_CON0
#define MT6359_RG_LDO_26M_CK_PDN_MASK                         0x1
#define MT6359_RG_LDO_26M_CK_PDN_SHIFT                        3
#define MT6359_RG_LDO_32K_CK_PDN_HWEN_ADDR                    \
	MT6359_TOP_TOP_CKHWEN_CON0
#define MT6359_RG_LDO_32K_CK_PDN_HWEN_MASK                    0x1
#define MT6359_RG_LDO_32K_CK_PDN_HWEN_SHIFT                   0
#define MT6359_RG_LDO_INTRP_CK_PDN_HWEN_ADDR                  \
	MT6359_TOP_TOP_CKHWEN_CON0
#define MT6359_RG_LDO_INTRP_CK_PDN_HWEN_MASK                  0x1
#define MT6359_RG_LDO_INTRP_CK_PDN_HWEN_SHIFT                 1
#define MT6359_RG_LDO_1M_CK_PDN_HWEN_ADDR                     \
	MT6359_TOP_TOP_CKHWEN_CON0
#define MT6359_RG_LDO_1M_CK_PDN_HWEN_MASK                     0x1
#define MT6359_RG_LDO_1M_CK_PDN_HWEN_SHIFT                    2
#define MT6359_RG_LDO_26M_CK_PDN_HWEN_ADDR                    \
	MT6359_TOP_TOP_CKHWEN_CON0
#define MT6359_RG_LDO_26M_CK_PDN_HWEN_MASK                    0x1
#define MT6359_RG_LDO_26M_CK_PDN_HWEN_SHIFT                   3
#define MT6359_RG_LDO_DCM_MODE_ADDR                           \
	MT6359_LDO_TOP_CLK_DCM_CON0
#define MT6359_RG_LDO_DCM_MODE_MASK                           0x1
#define MT6359_RG_LDO_DCM_MODE_SHIFT                          0
#define MT6359_RG_LDO_VSRAM_PROC1_OSC_SEL_DIS_ADDR            \
	MT6359_LDO_TOP_CLK_VSRAM_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_OSC_SEL_DIS_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_OSC_SEL_DIS_SHIFT           0
#define MT6359_RG_LDO_VSRAM_PROC2_OSC_SEL_DIS_ADDR            \
	MT6359_LDO_TOP_CLK_VSRAM_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_OSC_SEL_DIS_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_OSC_SEL_DIS_SHIFT           1
#define MT6359_RG_LDO_VSRAM_OTHERS_OSC_SEL_DIS_ADDR           \
	MT6359_LDO_TOP_CLK_VSRAM_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_OSC_SEL_DIS_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_OSC_SEL_DIS_SHIFT          2
#define MT6359_RG_LDO_VSRAM_MD_OSC_SEL_DIS_ADDR               \
	MT6359_LDO_TOP_CLK_VSRAM_CON0
#define MT6359_RG_LDO_VSRAM_MD_OSC_SEL_DIS_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_OSC_SEL_DIS_SHIFT              3
#define MT6359_RG_INT_EN_VFE28_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VFE28_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VFE28_OC_SHIFT                       0
#define MT6359_RG_INT_EN_VXO22_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VXO22_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VXO22_OC_SHIFT                       1
#define MT6359_RG_INT_EN_VRF18_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VRF18_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VRF18_OC_SHIFT                       2
#define MT6359_RG_INT_EN_VRF12_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VRF12_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VRF12_OC_SHIFT                       3
#define MT6359_RG_INT_EN_VEFUSE_OC_ADDR                       \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VEFUSE_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VEFUSE_OC_SHIFT                      4
#define MT6359_RG_INT_EN_VCN33_1_OC_ADDR                      \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCN33_1_OC_MASK                      0x1
#define MT6359_RG_INT_EN_VCN33_1_OC_SHIFT                     5
#define MT6359_RG_INT_EN_VCN33_2_OC_ADDR                      \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCN33_2_OC_MASK                      0x1
#define MT6359_RG_INT_EN_VCN33_2_OC_SHIFT                     6
#define MT6359_RG_INT_EN_VCN13_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCN13_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VCN13_OC_SHIFT                       7
#define MT6359_RG_INT_EN_VCN18_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCN18_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VCN18_OC_SHIFT                       8
#define MT6359_RG_INT_EN_VA09_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VA09_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VA09_OC_SHIFT                        9
#define MT6359_RG_INT_EN_VCAMIO_OC_ADDR                       \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VCAMIO_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VCAMIO_OC_SHIFT                      10
#define MT6359_RG_INT_EN_VA12_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VA12_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VA12_OC_SHIFT                        11
#define MT6359_RG_INT_EN_VAUX18_OC_ADDR                       \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VAUX18_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VAUX18_OC_SHIFT                      12
#define MT6359_RG_INT_EN_VAUD18_OC_ADDR                       \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VAUD18_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VAUD18_OC_SHIFT                      13
#define MT6359_RG_INT_EN_VIO18_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VIO18_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VIO18_OC_SHIFT                       14
#define MT6359_RG_INT_EN_VSRAM_PROC1_OC_ADDR                  \
	MT6359_LDO_TOP_INT_CON0
#define MT6359_RG_INT_EN_VSRAM_PROC1_OC_MASK                  0x1
#define MT6359_RG_INT_EN_VSRAM_PROC1_OC_SHIFT                 15
#define MT6359_LDO_INT_CON0_SET_ADDR                          \
	MT6359_LDO_TOP_INT_CON0_SET
#define MT6359_LDO_INT_CON0_SET_MASK                          0xFFFF
#define MT6359_LDO_INT_CON0_SET_SHIFT                         0
#define MT6359_LDO_INT_CON0_CLR_ADDR                          \
	MT6359_LDO_TOP_INT_CON0_CLR
#define MT6359_LDO_INT_CON0_CLR_MASK                          0xFFFF
#define MT6359_LDO_INT_CON0_CLR_SHIFT                         0
#define MT6359_RG_INT_EN_VSRAM_PROC2_OC_ADDR                  \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VSRAM_PROC2_OC_MASK                  0x1
#define MT6359_RG_INT_EN_VSRAM_PROC2_OC_SHIFT                 0
#define MT6359_RG_INT_EN_VSRAM_OTHERS_OC_ADDR                 \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VSRAM_OTHERS_OC_MASK                 0x1
#define MT6359_RG_INT_EN_VSRAM_OTHERS_OC_SHIFT                1
#define MT6359_RG_INT_EN_VSRAM_MD_OC_ADDR                     \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VSRAM_MD_OC_MASK                     0x1
#define MT6359_RG_INT_EN_VSRAM_MD_OC_SHIFT                    2
#define MT6359_RG_INT_EN_VEMC_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VEMC_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VEMC_OC_SHIFT                        3
#define MT6359_RG_INT_EN_VSIM1_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VSIM1_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VSIM1_OC_SHIFT                       4
#define MT6359_RG_INT_EN_VSIM2_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VSIM2_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VSIM2_OC_SHIFT                       5
#define MT6359_RG_INT_EN_VUSB_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VUSB_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VUSB_OC_SHIFT                        6
#define MT6359_RG_INT_EN_VRFCK_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VRFCK_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VRFCK_OC_SHIFT                       7
#define MT6359_RG_INT_EN_VBBCK_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VBBCK_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VBBCK_OC_SHIFT                       8
#define MT6359_RG_INT_EN_VBIF28_OC_ADDR                       \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VBIF28_OC_MASK                       0x1
#define MT6359_RG_INT_EN_VBIF28_OC_SHIFT                      9
#define MT6359_RG_INT_EN_VIBR_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VIBR_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VIBR_OC_SHIFT                        10
#define MT6359_RG_INT_EN_VIO28_OC_ADDR                        \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VIO28_OC_MASK                        0x1
#define MT6359_RG_INT_EN_VIO28_OC_SHIFT                       11
#define MT6359_RG_INT_EN_VM18_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VM18_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VM18_OC_SHIFT                        12
#define MT6359_RG_INT_EN_VUFS_OC_ADDR                         \
	MT6359_LDO_TOP_INT_CON1
#define MT6359_RG_INT_EN_VUFS_OC_MASK                         0x1
#define MT6359_RG_INT_EN_VUFS_OC_SHIFT                        13
#define MT6359_RG_INT_MASK_VFE28_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VFE28_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VFE28_OC_SHIFT                     0
#define MT6359_RG_INT_MASK_VXO22_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VXO22_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VXO22_OC_SHIFT                     1
#define MT6359_RG_INT_MASK_VRF18_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VRF18_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VRF18_OC_SHIFT                     2
#define MT6359_RG_INT_MASK_VRF12_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VRF12_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VRF12_OC_SHIFT                     3
#define MT6359_RG_INT_MASK_VEFUSE_OC_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VEFUSE_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VEFUSE_OC_SHIFT                    4
#define MT6359_RG_INT_MASK_VCN33_1_OC_ADDR                    \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCN33_1_OC_MASK                    0x1
#define MT6359_RG_INT_MASK_VCN33_1_OC_SHIFT                   5
#define MT6359_RG_INT_MASK_VCN33_2_OC_ADDR                    \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCN33_2_OC_MASK                    0x1
#define MT6359_RG_INT_MASK_VCN33_2_OC_SHIFT                   6
#define MT6359_RG_INT_MASK_VCN13_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCN13_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VCN13_OC_SHIFT                     7
#define MT6359_RG_INT_MASK_VCN18_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCN18_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VCN18_OC_SHIFT                     8
#define MT6359_RG_INT_MASK_VA09_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VA09_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VA09_OC_SHIFT                      9
#define MT6359_RG_INT_MASK_VCAMIO_OC_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VCAMIO_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VCAMIO_OC_SHIFT                    10
#define MT6359_RG_INT_MASK_VA12_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VA12_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VA12_OC_SHIFT                      11
#define MT6359_RG_INT_MASK_VAUX18_OC_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VAUX18_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VAUX18_OC_SHIFT                    12
#define MT6359_RG_INT_MASK_VAUD18_OC_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VAUD18_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VAUD18_OC_SHIFT                    13
#define MT6359_RG_INT_MASK_VIO18_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VIO18_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VIO18_OC_SHIFT                     14
#define MT6359_RG_INT_MASK_VSRAM_PROC1_OC_ADDR                \
	MT6359_LDO_TOP_INT_MASK_CON0
#define MT6359_RG_INT_MASK_VSRAM_PROC1_OC_MASK                0x1
#define MT6359_RG_INT_MASK_VSRAM_PROC1_OC_SHIFT               15
#define MT6359_LDO_INT_MASK_CON0_SET_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0_SET
#define MT6359_LDO_INT_MASK_CON0_SET_MASK                     0xFFFF
#define MT6359_LDO_INT_MASK_CON0_SET_SHIFT                    0
#define MT6359_LDO_INT_MASK_CON0_CLR_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON0_CLR
#define MT6359_LDO_INT_MASK_CON0_CLR_MASK                     0xFFFF
#define MT6359_LDO_INT_MASK_CON0_CLR_SHIFT                    0
#define MT6359_RG_INT_MASK_VSRAM_PROC2_OC_ADDR                \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VSRAM_PROC2_OC_MASK                0x1
#define MT6359_RG_INT_MASK_VSRAM_PROC2_OC_SHIFT               0
#define MT6359_RG_INT_MASK_VSRAM_OTHERS_OC_ADDR               \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VSRAM_OTHERS_OC_MASK               0x1
#define MT6359_RG_INT_MASK_VSRAM_OTHERS_OC_SHIFT              1
#define MT6359_RG_INT_MASK_VSRAM_MD_OC_ADDR                   \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VSRAM_MD_OC_MASK                   0x1
#define MT6359_RG_INT_MASK_VSRAM_MD_OC_SHIFT                  2
#define MT6359_RG_INT_MASK_VEMC_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VEMC_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VEMC_OC_SHIFT                      3
#define MT6359_RG_INT_MASK_VSIM1_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VSIM1_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VSIM1_OC_SHIFT                     4
#define MT6359_RG_INT_MASK_VSIM2_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VSIM2_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VSIM2_OC_SHIFT                     5
#define MT6359_RG_INT_MASK_VUSB_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VUSB_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VUSB_OC_SHIFT                      6
#define MT6359_RG_INT_MASK_VRFCK_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VRFCK_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VRFCK_OC_SHIFT                     7
#define MT6359_RG_INT_MASK_VBBCK_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VBBCK_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VBBCK_OC_SHIFT                     8
#define MT6359_RG_INT_MASK_VBIF28_OC_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VBIF28_OC_MASK                     0x1
#define MT6359_RG_INT_MASK_VBIF28_OC_SHIFT                    9
#define MT6359_RG_INT_MASK_VIBR_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VIBR_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VIBR_OC_SHIFT                      10
#define MT6359_RG_INT_MASK_VIO28_OC_ADDR                      \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VIO28_OC_MASK                      0x1
#define MT6359_RG_INT_MASK_VIO28_OC_SHIFT                     11
#define MT6359_RG_INT_MASK_VM18_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VM18_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VM18_OC_SHIFT                      12
#define MT6359_RG_INT_MASK_VUFS_OC_ADDR                       \
	MT6359_LDO_TOP_INT_MASK_CON1
#define MT6359_RG_INT_MASK_VUFS_OC_MASK                       0x1
#define MT6359_RG_INT_MASK_VUFS_OC_SHIFT                      13
#define MT6359_LDO_INT_MASK_CON1_SET_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON1_SET
#define MT6359_LDO_INT_MASK_CON1_SET_MASK                     0xFFFF
#define MT6359_LDO_INT_MASK_CON1_SET_SHIFT                    0
#define MT6359_LDO_INT_MASK_CON1_CLR_ADDR                     \
	MT6359_LDO_TOP_INT_MASK_CON1_CLR
#define MT6359_LDO_INT_MASK_CON1_CLR_MASK                     0xFFFF
#define MT6359_LDO_INT_MASK_CON1_CLR_SHIFT                    0
#define MT6359_RG_INT_STATUS_VFE28_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VFE28_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VFE28_OC_SHIFT                   0
#define MT6359_RG_INT_STATUS_VXO22_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VXO22_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VXO22_OC_SHIFT                   1
#define MT6359_RG_INT_STATUS_VRF18_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VRF18_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VRF18_OC_SHIFT                   2
#define MT6359_RG_INT_STATUS_VRF12_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VRF12_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VRF12_OC_SHIFT                   3
#define MT6359_RG_INT_STATUS_VEFUSE_OC_ADDR                   \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VEFUSE_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VEFUSE_OC_SHIFT                  4
#define MT6359_RG_INT_STATUS_VCN33_1_OC_ADDR                  \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCN33_1_OC_MASK                  0x1
#define MT6359_RG_INT_STATUS_VCN33_1_OC_SHIFT                 5
#define MT6359_RG_INT_STATUS_VCN33_2_OC_ADDR                  \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCN33_2_OC_MASK                  0x1
#define MT6359_RG_INT_STATUS_VCN33_2_OC_SHIFT                 6
#define MT6359_RG_INT_STATUS_VCN13_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCN13_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VCN13_OC_SHIFT                   7
#define MT6359_RG_INT_STATUS_VCN18_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCN18_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VCN18_OC_SHIFT                   8
#define MT6359_RG_INT_STATUS_VA09_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VA09_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VA09_OC_SHIFT                    9
#define MT6359_RG_INT_STATUS_VCAMIO_OC_ADDR                   \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VCAMIO_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VCAMIO_OC_SHIFT                  10
#define MT6359_RG_INT_STATUS_VA12_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VA12_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VA12_OC_SHIFT                    11
#define MT6359_RG_INT_STATUS_VAUX18_OC_ADDR                   \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VAUX18_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VAUX18_OC_SHIFT                  12
#define MT6359_RG_INT_STATUS_VAUD18_OC_ADDR                   \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VAUD18_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VAUD18_OC_SHIFT                  13
#define MT6359_RG_INT_STATUS_VIO18_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VIO18_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VIO18_OC_SHIFT                   14
#define MT6359_RG_INT_STATUS_VSRAM_PROC1_OC_ADDR              \
	MT6359_LDO_TOP_INT_STATUS0
#define MT6359_RG_INT_STATUS_VSRAM_PROC1_OC_MASK              0x1
#define MT6359_RG_INT_STATUS_VSRAM_PROC1_OC_SHIFT             15
#define MT6359_RG_INT_STATUS_VSRAM_PROC2_OC_ADDR              \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VSRAM_PROC2_OC_MASK              0x1
#define MT6359_RG_INT_STATUS_VSRAM_PROC2_OC_SHIFT             0
#define MT6359_RG_INT_STATUS_VSRAM_OTHERS_OC_ADDR             \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VSRAM_OTHERS_OC_MASK             0x1
#define MT6359_RG_INT_STATUS_VSRAM_OTHERS_OC_SHIFT            1
#define MT6359_RG_INT_STATUS_VSRAM_MD_OC_ADDR                 \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VSRAM_MD_OC_MASK                 0x1
#define MT6359_RG_INT_STATUS_VSRAM_MD_OC_SHIFT                2
#define MT6359_RG_INT_STATUS_VEMC_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VEMC_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VEMC_OC_SHIFT                    3
#define MT6359_RG_INT_STATUS_VSIM1_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VSIM1_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VSIM1_OC_SHIFT                   4
#define MT6359_RG_INT_STATUS_VSIM2_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VSIM2_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VSIM2_OC_SHIFT                   5
#define MT6359_RG_INT_STATUS_VUSB_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VUSB_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VUSB_OC_SHIFT                    6
#define MT6359_RG_INT_STATUS_VRFCK_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VRFCK_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VRFCK_OC_SHIFT                   7
#define MT6359_RG_INT_STATUS_VBBCK_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VBBCK_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VBBCK_OC_SHIFT                   8
#define MT6359_RG_INT_STATUS_VBIF28_OC_ADDR                   \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VBIF28_OC_MASK                   0x1
#define MT6359_RG_INT_STATUS_VBIF28_OC_SHIFT                  9
#define MT6359_RG_INT_STATUS_VIBR_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VIBR_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VIBR_OC_SHIFT                    10
#define MT6359_RG_INT_STATUS_VIO28_OC_ADDR                    \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VIO28_OC_MASK                    0x1
#define MT6359_RG_INT_STATUS_VIO28_OC_SHIFT                   11
#define MT6359_RG_INT_STATUS_VM18_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VM18_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VM18_OC_SHIFT                    12
#define MT6359_RG_INT_STATUS_VUFS_OC_ADDR                     \
	MT6359_LDO_TOP_INT_STATUS1
#define MT6359_RG_INT_STATUS_VUFS_OC_MASK                     0x1
#define MT6359_RG_INT_STATUS_VUFS_OC_SHIFT                    13
#define MT6359_RG_INT_RAW_STATUS_VFE28_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VFE28_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VFE28_OC_SHIFT               0
#define MT6359_RG_INT_RAW_STATUS_VXO22_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VXO22_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VXO22_OC_SHIFT               1
#define MT6359_RG_INT_RAW_STATUS_VRF18_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VRF18_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VRF18_OC_SHIFT               2
#define MT6359_RG_INT_RAW_STATUS_VRF12_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VRF12_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VRF12_OC_SHIFT               3
#define MT6359_RG_INT_RAW_STATUS_VEFUSE_OC_ADDR               \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VEFUSE_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VEFUSE_OC_SHIFT              4
#define MT6359_RG_INT_RAW_STATUS_VCN33_1_OC_ADDR              \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCN33_1_OC_MASK              0x1
#define MT6359_RG_INT_RAW_STATUS_VCN33_1_OC_SHIFT             5
#define MT6359_RG_INT_RAW_STATUS_VCN33_2_OC_ADDR              \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCN33_2_OC_MASK              0x1
#define MT6359_RG_INT_RAW_STATUS_VCN33_2_OC_SHIFT             6
#define MT6359_RG_INT_RAW_STATUS_VCN13_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCN13_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VCN13_OC_SHIFT               7
#define MT6359_RG_INT_RAW_STATUS_VCN18_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCN18_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VCN18_OC_SHIFT               8
#define MT6359_RG_INT_RAW_STATUS_VA09_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VA09_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VA09_OC_SHIFT                9
#define MT6359_RG_INT_RAW_STATUS_VCAMIO_OC_ADDR               \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VCAMIO_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VCAMIO_OC_SHIFT              10
#define MT6359_RG_INT_RAW_STATUS_VA12_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VA12_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VA12_OC_SHIFT                11
#define MT6359_RG_INT_RAW_STATUS_VAUX18_OC_ADDR               \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VAUX18_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VAUX18_OC_SHIFT              12
#define MT6359_RG_INT_RAW_STATUS_VAUD18_OC_ADDR               \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VAUD18_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VAUD18_OC_SHIFT              13
#define MT6359_RG_INT_RAW_STATUS_VIO18_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VIO18_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VIO18_OC_SHIFT               14
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC1_OC_ADDR          \
	MT6359_LDO_TOP_INT_RAW_STATUS0
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC1_OC_MASK          0x1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC1_OC_SHIFT         15
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC2_OC_ADDR          \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC2_OC_MASK          0x1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_PROC2_OC_SHIFT         0
#define MT6359_RG_INT_RAW_STATUS_VSRAM_OTHERS_OC_ADDR         \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_OTHERS_OC_MASK         0x1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_OTHERS_OC_SHIFT        1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_MD_OC_ADDR             \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_MD_OC_MASK             0x1
#define MT6359_RG_INT_RAW_STATUS_VSRAM_MD_OC_SHIFT            2
#define MT6359_RG_INT_RAW_STATUS_VEMC_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VEMC_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VEMC_OC_SHIFT                3
#define MT6359_RG_INT_RAW_STATUS_VSIM1_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VSIM1_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VSIM1_OC_SHIFT               4
#define MT6359_RG_INT_RAW_STATUS_VSIM2_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VSIM2_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VSIM2_OC_SHIFT               5
#define MT6359_RG_INT_RAW_STATUS_VUSB_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VUSB_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VUSB_OC_SHIFT                6
#define MT6359_RG_INT_RAW_STATUS_VRFCK_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VRFCK_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VRFCK_OC_SHIFT               7
#define MT6359_RG_INT_RAW_STATUS_VBBCK_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VBBCK_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VBBCK_OC_SHIFT               8
#define MT6359_RG_INT_RAW_STATUS_VBIF28_OC_ADDR               \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VBIF28_OC_MASK               0x1
#define MT6359_RG_INT_RAW_STATUS_VBIF28_OC_SHIFT              9
#define MT6359_RG_INT_RAW_STATUS_VIBR_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VIBR_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VIBR_OC_SHIFT                10
#define MT6359_RG_INT_RAW_STATUS_VIO28_OC_ADDR                \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VIO28_OC_MASK                0x1
#define MT6359_RG_INT_RAW_STATUS_VIO28_OC_SHIFT               11
#define MT6359_RG_INT_RAW_STATUS_VM18_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VM18_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VM18_OC_SHIFT                12
#define MT6359_RG_INT_RAW_STATUS_VUFS_OC_ADDR                 \
	MT6359_LDO_TOP_INT_RAW_STATUS1
#define MT6359_RG_INT_RAW_STATUS_VUFS_OC_MASK                 0x1
#define MT6359_RG_INT_RAW_STATUS_VUFS_OC_SHIFT                13
#define MT6359_RG_LDO_MON_FLAG_SEL_ADDR                       \
	MT6359_LDO_TEST_CON0
#define MT6359_RG_LDO_MON_FLAG_SEL_MASK                       0xFF
#define MT6359_RG_LDO_MON_FLAG_SEL_SHIFT                      0
#define MT6359_RG_LDO_INT_FLAG_EN_ADDR                        \
	MT6359_LDO_TEST_CON0
#define MT6359_RG_LDO_INT_FLAG_EN_MASK                        0x1
#define MT6359_RG_LDO_INT_FLAG_EN_SHIFT                       9
#define MT6359_RG_LDO_MON_GRP_SEL_ADDR                        \
	MT6359_LDO_TEST_CON0
#define MT6359_RG_LDO_MON_GRP_SEL_MASK                        0x1
#define MT6359_RG_LDO_MON_GRP_SEL_SHIFT                       10
#define MT6359_RG_LDO_WDT_MODE_ADDR                           \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_WDT_MODE_MASK                           0x1
#define MT6359_RG_LDO_WDT_MODE_SHIFT                          0
#define MT6359_RG_LDO_DUMMY_LOAD_GATED_DIS_ADDR               \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_DUMMY_LOAD_GATED_DIS_MASK               0x1
#define MT6359_RG_LDO_DUMMY_LOAD_GATED_DIS_SHIFT              1
#define MT6359_RG_LDO_LP_PROT_DISABLE_ADDR                    \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_LP_PROT_DISABLE_MASK                    0x1
#define MT6359_RG_LDO_LP_PROT_DISABLE_SHIFT                   2
#define MT6359_RG_LDO_SLEEP_CTRL_MODE_ADDR                    \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_SLEEP_CTRL_MODE_MASK                    0x1
#define MT6359_RG_LDO_SLEEP_CTRL_MODE_SHIFT                   3
#define MT6359_RG_LDO_TOP_RSV1_ADDR                           \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_TOP_RSV1_MASK                           0xF
#define MT6359_RG_LDO_TOP_RSV1_SHIFT                          8
#define MT6359_RG_LDO_TOP_RSV0_ADDR                           \
	MT6359_LDO_TOP_CON
#define MT6359_RG_LDO_TOP_RSV0_MASK                           0xF
#define MT6359_RG_LDO_TOP_RSV0_SHIFT                          12
#define MT6359_RG_VRTC28_EN_ADDR                              \
	MT6359_VRTC28_CON
#define MT6359_RG_VRTC28_EN_MASK                              0x1
#define MT6359_RG_VRTC28_EN_SHIFT                             1
#define MT6359_DA_VRTC28_EN_ADDR                              \
	MT6359_VRTC28_CON
#define MT6359_DA_VRTC28_EN_MASK                              0x1
#define MT6359_DA_VRTC28_EN_SHIFT                             15
#define MT6359_RG_VAUX18_OFF_ACKTIME_SEL_ADDR                 \
	MT6359_VAUX18_ACK
#define MT6359_RG_VAUX18_OFF_ACKTIME_SEL_MASK                 0x1
#define MT6359_RG_VAUX18_OFF_ACKTIME_SEL_SHIFT                0
#define MT6359_RG_VAUX18_LP_ACKTIME_SEL_ADDR                  \
	MT6359_VAUX18_ACK
#define MT6359_RG_VAUX18_LP_ACKTIME_SEL_MASK                  0x1
#define MT6359_RG_VAUX18_LP_ACKTIME_SEL_SHIFT                 1
#define MT6359_RG_VBIF28_OFF_ACKTIME_SEL_ADDR                 \
	MT6359_VBIF28_ACK
#define MT6359_RG_VBIF28_OFF_ACKTIME_SEL_MASK                 0x1
#define MT6359_RG_VBIF28_OFF_ACKTIME_SEL_SHIFT                0
#define MT6359_RG_VBIF28_LP_ACKTIME_SEL_ADDR                  \
	MT6359_VBIF28_ACK
#define MT6359_RG_VBIF28_LP_ACKTIME_SEL_MASK                  0x1
#define MT6359_RG_VBIF28_LP_ACKTIME_SEL_SHIFT                 1
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_DONE_ADDR            \
	MT6359_VOW_DVS_CON
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_DONE_MASK            0x1
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_DONE_SHIFT           0
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_SW_MODE_ADDR         \
	MT6359_VOW_DVS_CON
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_SW_MODE_MASK         0x1
#define MT6359_RG_VOW_LDO_VSRAM_CORE_DVS_SW_MODE_SHIFT        1
#define MT6359_RG_LDO_VXO22_EN_SW_MODE_ADDR                   \
	MT6359_VXO22_CON
#define MT6359_RG_LDO_VXO22_EN_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VXO22_EN_SW_MODE_SHIFT                  0
#define MT6359_RG_LDO_VXO22_EN_TEST_ADDR                      \
	MT6359_VXO22_CON
#define MT6359_RG_LDO_VXO22_EN_TEST_MASK                      0x1
#define MT6359_RG_LDO_VXO22_EN_TEST_SHIFT                     1
#define MT6359_LDO_TOP_ELR_LEN_ADDR                           \
	MT6359_LDO_TOP_ELR_NUM
#define MT6359_LDO_TOP_ELR_LEN_MASK                           0xFF
#define MT6359_LDO_TOP_ELR_LEN_SHIFT                          0
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_ADDR                  \
	MT6359_LDO_VSRAM_PROC1_ELR
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_MASK                  0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_SHIFT                 0
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LIMIT_SEL_ADDR        \
	MT6359_LDO_VSRAM_PROC1_ELR
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LIMIT_SEL_MASK        0x3
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LIMIT_SEL_SHIFT       8
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_ADDR                  \
	MT6359_LDO_VSRAM_PROC2_ELR
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_MASK                  0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_SHIFT                 0
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LIMIT_SEL_ADDR        \
	MT6359_LDO_VSRAM_PROC2_ELR
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LIMIT_SEL_MASK        0x3
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LIMIT_SEL_SHIFT       8
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR                 \
	MT6359_LDO_VSRAM_OTHERS_ELR
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_MASK                 0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT                0
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LIMIT_SEL_ADDR       \
	MT6359_LDO_VSRAM_OTHERS_ELR
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LIMIT_SEL_MASK       0x3
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LIMIT_SEL_SHIFT      8
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_ADDR                     \
	MT6359_LDO_VSRAM_MD_ELR
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_MASK                     0x7F
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_SHIFT                    0
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LIMIT_SEL_ADDR           \
	MT6359_LDO_VSRAM_MD_ELR
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LIMIT_SEL_MASK           0x3
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LIMIT_SEL_SHIFT          8
#define MT6359_RG_LDO_VRFCK_ANA_SEL_ADDR                      \
	MT6359_LDO_VSRAM_MD_ELR
#define MT6359_RG_LDO_VRFCK_ANA_SEL_MASK                      0x1
#define MT6359_RG_LDO_VRFCK_ANA_SEL_SHIFT                     15
#define MT6359_LDO_GNR0_ANA_ID_ADDR                           \
	MT6359_LDO_GNR0_DSN_ID
#define MT6359_LDO_GNR0_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR0_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR0_DIG_ID_ADDR                           \
	MT6359_LDO_GNR0_DSN_ID
#define MT6359_LDO_GNR0_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR0_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR0_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR0_DSN_REV0
#define MT6359_LDO_GNR0_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR0_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR0_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR0_DSN_REV0
#define MT6359_LDO_GNR0_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR0_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR0_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR0_DSN_REV0
#define MT6359_LDO_GNR0_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR0_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR0_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR0_DSN_REV0
#define MT6359_LDO_GNR0_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR0_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR0_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR0_DSN_DBI
#define MT6359_LDO_GNR0_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR0_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR0_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR0_DSN_DBI
#define MT6359_LDO_GNR0_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR0_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR0_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR0_DSN_DBI
#define MT6359_LDO_GNR0_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR0_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR0_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR0_DSN_DXI
#define MT6359_LDO_GNR0_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR0_DSN_FPI_SHIFT                         0
#define MT6359_RG_LDO_VFE28_EN_ADDR                           \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_EN_MASK                           0x1
#define MT6359_RG_LDO_VFE28_EN_SHIFT                          0
#define MT6359_RG_LDO_VFE28_LP_ADDR                           \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_LP_MASK                           0x1
#define MT6359_RG_LDO_VFE28_LP_SHIFT                          1
#define MT6359_RG_LDO_VFE28_STBTD_ADDR                        \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VFE28_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VFE28_ULP_ADDR                          \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_ULP_MASK                          0x1
#define MT6359_RG_LDO_VFE28_ULP_SHIFT                         4
#define MT6359_RG_LDO_VFE28_OCFB_EN_ADDR                      \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VFE28_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VFE28_OC_MODE_ADDR                      \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VFE28_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VFE28_OC_TSEL_ADDR                      \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VFE28_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VFE28_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VFE28_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VFE28_OP_MODE_ADDR                      \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VFE28_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VFE28_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VFE28_CON0
#define MT6359_RG_LDO_VFE28_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VFE28_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VFE28_B_EN_ADDR                             \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_B_EN_MASK                             0x1
#define MT6359_DA_VFE28_B_EN_SHIFT                            0
#define MT6359_DA_VFE28_B_STB_ADDR                            \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_B_STB_MASK                            0x1
#define MT6359_DA_VFE28_B_STB_SHIFT                           1
#define MT6359_DA_VFE28_B_LP_ADDR                             \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_B_LP_MASK                             0x1
#define MT6359_DA_VFE28_B_LP_SHIFT                            2
#define MT6359_DA_VFE28_L_EN_ADDR                             \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_L_EN_MASK                             0x1
#define MT6359_DA_VFE28_L_EN_SHIFT                            3
#define MT6359_DA_VFE28_L_STB_ADDR                            \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_L_STB_MASK                            0x1
#define MT6359_DA_VFE28_L_STB_SHIFT                           4
#define MT6359_DA_VFE28_OCFB_EN_ADDR                          \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_OCFB_EN_MASK                          0x1
#define MT6359_DA_VFE28_OCFB_EN_SHIFT                         5
#define MT6359_DA_VFE28_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VFE28_MON
#define MT6359_DA_VFE28_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VFE28_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VFE28_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VFE28_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VFE28_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VFE28_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VFE28_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VFE28_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VFE28_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VFE28_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VFE28_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VFE28_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VFE28_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VFE28_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VFE28_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VFE28_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VFE28_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VFE28_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VFE28_SW_OP_EN_ADDR                     \
	MT6359_LDO_VFE28_OP_EN
#define MT6359_RG_LDO_VFE28_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VFE28_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VFE28_OP_EN_SET_ADDR                    \
	MT6359_LDO_VFE28_OP_EN_SET
#define MT6359_RG_LDO_VFE28_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VFE28_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VFE28_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VFE28_OP_EN_CLR
#define MT6359_RG_LDO_VFE28_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VFE28_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VFE28_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VFE28_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VFE28_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VFE28_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VFE28_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VFE28_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VFE28_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VFE28_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VFE28_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VFE28_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VFE28_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VFE28_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VFE28_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VFE28_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VFE28_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VFE28_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VFE28_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VFE28_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VFE28_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VFE28_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VFE28_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VFE28_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VFE28_OP_CFG
#define MT6359_RG_LDO_VFE28_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VFE28_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VFE28_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG_SET
#define MT6359_RG_LDO_VFE28_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VFE28_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VFE28_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VFE28_OP_CFG_CLR
#define MT6359_RG_LDO_VFE28_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VFE28_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VXO22_EN_ADDR                           \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_EN_MASK                           0x1
#define MT6359_RG_LDO_VXO22_EN_SHIFT                          0
#define MT6359_RG_LDO_VXO22_LP_ADDR                           \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_LP_MASK                           0x1
#define MT6359_RG_LDO_VXO22_LP_SHIFT                          1
#define MT6359_RG_LDO_VXO22_STBTD_ADDR                        \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VXO22_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VXO22_ULP_ADDR                          \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_ULP_MASK                          0x1
#define MT6359_RG_LDO_VXO22_ULP_SHIFT                         4
#define MT6359_RG_LDO_VXO22_OCFB_EN_ADDR                      \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VXO22_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VXO22_OC_MODE_ADDR                      \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VXO22_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VXO22_OC_TSEL_ADDR                      \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VXO22_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VXO22_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VXO22_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VXO22_OP_MODE_ADDR                      \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VXO22_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VXO22_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VXO22_CON0
#define MT6359_RG_LDO_VXO22_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VXO22_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VXO22_B_EN_ADDR                             \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_B_EN_MASK                             0x1
#define MT6359_DA_VXO22_B_EN_SHIFT                            0
#define MT6359_DA_VXO22_B_STB_ADDR                            \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_B_STB_MASK                            0x1
#define MT6359_DA_VXO22_B_STB_SHIFT                           1
#define MT6359_DA_VXO22_B_LP_ADDR                             \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_B_LP_MASK                             0x1
#define MT6359_DA_VXO22_B_LP_SHIFT                            2
#define MT6359_DA_VXO22_L_EN_ADDR                             \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_L_EN_MASK                             0x1
#define MT6359_DA_VXO22_L_EN_SHIFT                            3
#define MT6359_DA_VXO22_L_STB_ADDR                            \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_L_STB_MASK                            0x1
#define MT6359_DA_VXO22_L_STB_SHIFT                           4
#define MT6359_DA_VXO22_OCFB_EN_ADDR                          \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_OCFB_EN_MASK                          0x1
#define MT6359_DA_VXO22_OCFB_EN_SHIFT                         5
#define MT6359_DA_VXO22_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VXO22_MON
#define MT6359_DA_VXO22_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VXO22_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VXO22_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VXO22_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VXO22_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VXO22_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VXO22_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VXO22_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VXO22_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VXO22_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VXO22_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VXO22_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VXO22_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VXO22_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VXO22_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VXO22_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VXO22_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VXO22_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VXO22_SW_OP_EN_ADDR                     \
	MT6359_LDO_VXO22_OP_EN
#define MT6359_RG_LDO_VXO22_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VXO22_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VXO22_OP_EN_SET_ADDR                    \
	MT6359_LDO_VXO22_OP_EN_SET
#define MT6359_RG_LDO_VXO22_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VXO22_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VXO22_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VXO22_OP_EN_CLR
#define MT6359_RG_LDO_VXO22_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VXO22_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VXO22_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VXO22_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VXO22_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VXO22_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VXO22_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VXO22_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VXO22_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VXO22_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VXO22_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VXO22_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VXO22_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VXO22_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VXO22_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VXO22_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VXO22_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VXO22_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VXO22_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VXO22_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VXO22_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VXO22_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VXO22_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VXO22_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VXO22_OP_CFG
#define MT6359_RG_LDO_VXO22_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VXO22_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VXO22_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG_SET
#define MT6359_RG_LDO_VXO22_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VXO22_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VXO22_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VXO22_OP_CFG_CLR
#define MT6359_RG_LDO_VXO22_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VXO22_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VRF18_EN_ADDR                           \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_EN_MASK                           0x1
#define MT6359_RG_LDO_VRF18_EN_SHIFT                          0
#define MT6359_RG_LDO_VRF18_LP_ADDR                           \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_LP_MASK                           0x1
#define MT6359_RG_LDO_VRF18_LP_SHIFT                          1
#define MT6359_RG_LDO_VRF18_STBTD_ADDR                        \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VRF18_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VRF18_ULP_ADDR                          \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_ULP_MASK                          0x1
#define MT6359_RG_LDO_VRF18_ULP_SHIFT                         4
#define MT6359_RG_LDO_VRF18_OCFB_EN_ADDR                      \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VRF18_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VRF18_OC_MODE_ADDR                      \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VRF18_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VRF18_OC_TSEL_ADDR                      \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VRF18_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VRF18_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VRF18_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VRF18_OP_MODE_ADDR                      \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VRF18_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VRF18_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VRF18_CON0
#define MT6359_RG_LDO_VRF18_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VRF18_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VRF18_B_EN_ADDR                             \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_B_EN_MASK                             0x1
#define MT6359_DA_VRF18_B_EN_SHIFT                            0
#define MT6359_DA_VRF18_B_STB_ADDR                            \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_B_STB_MASK                            0x1
#define MT6359_DA_VRF18_B_STB_SHIFT                           1
#define MT6359_DA_VRF18_B_LP_ADDR                             \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_B_LP_MASK                             0x1
#define MT6359_DA_VRF18_B_LP_SHIFT                            2
#define MT6359_DA_VRF18_L_EN_ADDR                             \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_L_EN_MASK                             0x1
#define MT6359_DA_VRF18_L_EN_SHIFT                            3
#define MT6359_DA_VRF18_L_STB_ADDR                            \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_L_STB_MASK                            0x1
#define MT6359_DA_VRF18_L_STB_SHIFT                           4
#define MT6359_DA_VRF18_OCFB_EN_ADDR                          \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_OCFB_EN_MASK                          0x1
#define MT6359_DA_VRF18_OCFB_EN_SHIFT                         5
#define MT6359_DA_VRF18_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VRF18_MON
#define MT6359_DA_VRF18_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VRF18_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VRF18_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VRF18_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VRF18_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VRF18_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VRF18_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VRF18_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VRF18_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VRF18_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VRF18_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VRF18_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF18_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VRF18_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VRF18_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VRF18_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VRF18_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VRF18_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VRF18_SW_OP_EN_ADDR                     \
	MT6359_LDO_VRF18_OP_EN
#define MT6359_RG_LDO_VRF18_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VRF18_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VRF18_OP_EN_SET_ADDR                    \
	MT6359_LDO_VRF18_OP_EN_SET
#define MT6359_RG_LDO_VRF18_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VRF18_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VRF18_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VRF18_OP_EN_CLR
#define MT6359_RG_LDO_VRF18_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VRF18_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VRF18_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VRF18_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VRF18_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VRF18_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VRF18_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VRF18_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VRF18_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VRF18_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VRF18_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VRF18_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF18_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VRF18_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF18_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VRF18_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF18_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VRF18_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF18_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VRF18_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF18_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VRF18_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF18_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VRF18_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VRF18_OP_CFG
#define MT6359_RG_LDO_VRF18_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VRF18_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VRF18_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG_SET
#define MT6359_RG_LDO_VRF18_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VRF18_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VRF18_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VRF18_OP_CFG_CLR
#define MT6359_RG_LDO_VRF18_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VRF18_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VRF12_EN_ADDR                           \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_EN_MASK                           0x1
#define MT6359_RG_LDO_VRF12_EN_SHIFT                          0
#define MT6359_RG_LDO_VRF12_LP_ADDR                           \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_LP_MASK                           0x1
#define MT6359_RG_LDO_VRF12_LP_SHIFT                          1
#define MT6359_RG_LDO_VRF12_STBTD_ADDR                        \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VRF12_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VRF12_ULP_ADDR                          \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_ULP_MASK                          0x1
#define MT6359_RG_LDO_VRF12_ULP_SHIFT                         4
#define MT6359_RG_LDO_VRF12_OCFB_EN_ADDR                      \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VRF12_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VRF12_OC_MODE_ADDR                      \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VRF12_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VRF12_OC_TSEL_ADDR                      \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VRF12_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VRF12_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VRF12_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VRF12_OP_MODE_ADDR                      \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VRF12_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VRF12_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VRF12_CON0
#define MT6359_RG_LDO_VRF12_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VRF12_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VRF12_B_EN_ADDR                             \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_B_EN_MASK                             0x1
#define MT6359_DA_VRF12_B_EN_SHIFT                            0
#define MT6359_DA_VRF12_B_STB_ADDR                            \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_B_STB_MASK                            0x1
#define MT6359_DA_VRF12_B_STB_SHIFT                           1
#define MT6359_DA_VRF12_B_LP_ADDR                             \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_B_LP_MASK                             0x1
#define MT6359_DA_VRF12_B_LP_SHIFT                            2
#define MT6359_DA_VRF12_L_EN_ADDR                             \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_L_EN_MASK                             0x1
#define MT6359_DA_VRF12_L_EN_SHIFT                            3
#define MT6359_DA_VRF12_L_STB_ADDR                            \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_L_STB_MASK                            0x1
#define MT6359_DA_VRF12_L_STB_SHIFT                           4
#define MT6359_DA_VRF12_OCFB_EN_ADDR                          \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_OCFB_EN_MASK                          0x1
#define MT6359_DA_VRF12_OCFB_EN_SHIFT                         5
#define MT6359_DA_VRF12_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VRF12_MON
#define MT6359_DA_VRF12_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VRF12_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VRF12_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VRF12_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VRF12_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VRF12_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VRF12_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VRF12_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VRF12_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VRF12_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VRF12_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VRF12_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRF12_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VRF12_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VRF12_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VRF12_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VRF12_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VRF12_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VRF12_SW_OP_EN_ADDR                     \
	MT6359_LDO_VRF12_OP_EN
#define MT6359_RG_LDO_VRF12_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VRF12_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VRF12_OP_EN_SET_ADDR                    \
	MT6359_LDO_VRF12_OP_EN_SET
#define MT6359_RG_LDO_VRF12_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VRF12_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VRF12_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VRF12_OP_EN_CLR
#define MT6359_RG_LDO_VRF12_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VRF12_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VRF12_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VRF12_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VRF12_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VRF12_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VRF12_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VRF12_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VRF12_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VRF12_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VRF12_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VRF12_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRF12_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VRF12_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF12_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VRF12_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF12_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VRF12_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF12_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VRF12_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF12_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VRF12_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRF12_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VRF12_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VRF12_OP_CFG
#define MT6359_RG_LDO_VRF12_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VRF12_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VRF12_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG_SET
#define MT6359_RG_LDO_VRF12_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VRF12_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VRF12_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VRF12_OP_CFG_CLR
#define MT6359_RG_LDO_VRF12_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VRF12_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VEFUSE_EN_ADDR                          \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_EN_MASK                          0x1
#define MT6359_RG_LDO_VEFUSE_EN_SHIFT                         0
#define MT6359_RG_LDO_VEFUSE_LP_ADDR                          \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_LP_MASK                          0x1
#define MT6359_RG_LDO_VEFUSE_LP_SHIFT                         1
#define MT6359_RG_LDO_VEFUSE_STBTD_ADDR                       \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_STBTD_MASK                       0x3
#define MT6359_RG_LDO_VEFUSE_STBTD_SHIFT                      2
#define MT6359_RG_LDO_VEFUSE_ULP_ADDR                         \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_ULP_MASK                         0x1
#define MT6359_RG_LDO_VEFUSE_ULP_SHIFT                        4
#define MT6359_RG_LDO_VEFUSE_OCFB_EN_ADDR                     \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_OCFB_EN_MASK                     0x1
#define MT6359_RG_LDO_VEFUSE_OCFB_EN_SHIFT                    5
#define MT6359_RG_LDO_VEFUSE_OC_MODE_ADDR                     \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_OC_MODE_MASK                     0x1
#define MT6359_RG_LDO_VEFUSE_OC_MODE_SHIFT                    6
#define MT6359_RG_LDO_VEFUSE_OC_TSEL_ADDR                     \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_OC_TSEL_MASK                     0x1
#define MT6359_RG_LDO_VEFUSE_OC_TSEL_SHIFT                    7
#define MT6359_RG_LDO_VEFUSE_DUMMY_LOAD_ADDR                  \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_DUMMY_LOAD_MASK                  0x3
#define MT6359_RG_LDO_VEFUSE_DUMMY_LOAD_SHIFT                 8
#define MT6359_RG_LDO_VEFUSE_OP_MODE_ADDR                     \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_OP_MODE_MASK                     0x7
#define MT6359_RG_LDO_VEFUSE_OP_MODE_SHIFT                    10
#define MT6359_RG_LDO_VEFUSE_CK_SW_MODE_ADDR                  \
	MT6359_LDO_VEFUSE_CON0
#define MT6359_RG_LDO_VEFUSE_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_CK_SW_MODE_SHIFT                 15
#define MT6359_DA_VEFUSE_B_EN_ADDR                            \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_B_EN_MASK                            0x1
#define MT6359_DA_VEFUSE_B_EN_SHIFT                           0
#define MT6359_DA_VEFUSE_B_STB_ADDR                           \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_B_STB_MASK                           0x1
#define MT6359_DA_VEFUSE_B_STB_SHIFT                          1
#define MT6359_DA_VEFUSE_B_LP_ADDR                            \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_B_LP_MASK                            0x1
#define MT6359_DA_VEFUSE_B_LP_SHIFT                           2
#define MT6359_DA_VEFUSE_L_EN_ADDR                            \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_L_EN_MASK                            0x1
#define MT6359_DA_VEFUSE_L_EN_SHIFT                           3
#define MT6359_DA_VEFUSE_L_STB_ADDR                           \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_L_STB_MASK                           0x1
#define MT6359_DA_VEFUSE_L_STB_SHIFT                          4
#define MT6359_DA_VEFUSE_OCFB_EN_ADDR                         \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_OCFB_EN_MASK                         0x1
#define MT6359_DA_VEFUSE_OCFB_EN_SHIFT                        5
#define MT6359_DA_VEFUSE_DUMMY_LOAD_ADDR                      \
	MT6359_LDO_VEFUSE_MON
#define MT6359_DA_VEFUSE_DUMMY_LOAD_MASK                      0x3
#define MT6359_DA_VEFUSE_DUMMY_LOAD_SHIFT                     6
#define MT6359_RG_LDO_VEFUSE_HW0_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_LDO_VEFUSE_HW1_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_LDO_VEFUSE_HW2_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_LDO_VEFUSE_HW3_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_LDO_VEFUSE_HW4_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_LDO_VEFUSE_HW5_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_LDO_VEFUSE_HW6_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_LDO_VEFUSE_HW7_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_LDO_VEFUSE_HW8_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_LDO_VEFUSE_HW9_OP_EN_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_LDO_VEFUSE_HW10_OP_EN_ADDR                  \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_LDO_VEFUSE_HW11_OP_EN_ADDR                  \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_LDO_VEFUSE_HW12_OP_EN_ADDR                  \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_LDO_VEFUSE_HW13_OP_EN_ADDR                  \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_LDO_VEFUSE_HW14_OP_EN_ADDR                  \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_LDO_VEFUSE_SW_OP_EN_ADDR                    \
	MT6359_LDO_VEFUSE_OP_EN
#define MT6359_RG_LDO_VEFUSE_SW_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEFUSE_SW_OP_EN_SHIFT                   15
#define MT6359_RG_LDO_VEFUSE_OP_EN_SET_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN_SET
#define MT6359_RG_LDO_VEFUSE_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VEFUSE_OP_EN_SET_SHIFT                  0
#define MT6359_RG_LDO_VEFUSE_OP_EN_CLR_ADDR                   \
	MT6359_LDO_VEFUSE_OP_EN_CLR
#define MT6359_RG_LDO_VEFUSE_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VEFUSE_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_LDO_VEFUSE_HW0_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_LDO_VEFUSE_HW1_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_LDO_VEFUSE_HW2_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_LDO_VEFUSE_HW3_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_LDO_VEFUSE_HW4_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_LDO_VEFUSE_HW5_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_LDO_VEFUSE_HW6_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_LDO_VEFUSE_HW7_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_LDO_VEFUSE_HW8_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_LDO_VEFUSE_HW9_OP_CFG_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VEFUSE_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_LDO_VEFUSE_HW10_OP_CFG_ADDR                 \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VEFUSE_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_LDO_VEFUSE_HW11_OP_CFG_ADDR                 \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VEFUSE_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_LDO_VEFUSE_HW12_OP_CFG_ADDR                 \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VEFUSE_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_LDO_VEFUSE_HW13_OP_CFG_ADDR                 \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VEFUSE_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_LDO_VEFUSE_HW14_OP_CFG_ADDR                 \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VEFUSE_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_LDO_VEFUSE_SW_OP_CFG_ADDR                   \
	MT6359_LDO_VEFUSE_OP_CFG
#define MT6359_RG_LDO_VEFUSE_SW_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEFUSE_SW_OP_CFG_SHIFT                  15
#define MT6359_RG_LDO_VEFUSE_OP_CFG_SET_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG_SET
#define MT6359_RG_LDO_VEFUSE_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VEFUSE_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_LDO_VEFUSE_OP_CFG_CLR_ADDR                  \
	MT6359_LDO_VEFUSE_OP_CFG_CLR
#define MT6359_RG_LDO_VEFUSE_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VEFUSE_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_LDO_VCN33_1_EN_0_ADDR                       \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_EN_0_MASK                       0x1
#define MT6359_RG_LDO_VCN33_1_EN_0_SHIFT                      0
#define MT6359_RG_LDO_VCN33_1_LP_ADDR                         \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_LP_MASK                         0x1
#define MT6359_RG_LDO_VCN33_1_LP_SHIFT                        1
#define MT6359_RG_LDO_VCN33_1_STBTD_ADDR                      \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_STBTD_MASK                      0x3
#define MT6359_RG_LDO_VCN33_1_STBTD_SHIFT                     2
#define MT6359_RG_LDO_VCN33_1_ULP_ADDR                        \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_ULP_MASK                        0x1
#define MT6359_RG_LDO_VCN33_1_ULP_SHIFT                       4
#define MT6359_RG_LDO_VCN33_1_OCFB_EN_ADDR                    \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_OCFB_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN33_1_OCFB_EN_SHIFT                   5
#define MT6359_RG_LDO_VCN33_1_OC_MODE_ADDR                    \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_OC_MODE_MASK                    0x1
#define MT6359_RG_LDO_VCN33_1_OC_MODE_SHIFT                   6
#define MT6359_RG_LDO_VCN33_1_OC_TSEL_ADDR                    \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_OC_TSEL_MASK                    0x1
#define MT6359_RG_LDO_VCN33_1_OC_TSEL_SHIFT                   7
#define MT6359_RG_LDO_VCN33_1_DUMMY_LOAD_ADDR                 \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_DUMMY_LOAD_MASK                 0x3
#define MT6359_RG_LDO_VCN33_1_DUMMY_LOAD_SHIFT                8
#define MT6359_RG_LDO_VCN33_1_OP_MODE_ADDR                    \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_OP_MODE_MASK                    0x7
#define MT6359_RG_LDO_VCN33_1_OP_MODE_SHIFT                   10
#define MT6359_RG_LDO_VCN33_1_CK_SW_MODE_ADDR                 \
	MT6359_LDO_VCN33_1_CON0
#define MT6359_RG_LDO_VCN33_1_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_CK_SW_MODE_SHIFT                15
#define MT6359_DA_VCN33_1_B_EN_ADDR                           \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_B_EN_MASK                           0x1
#define MT6359_DA_VCN33_1_B_EN_SHIFT                          0
#define MT6359_DA_VCN33_1_B_STB_ADDR                          \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_B_STB_MASK                          0x1
#define MT6359_DA_VCN33_1_B_STB_SHIFT                         1
#define MT6359_DA_VCN33_1_B_LP_ADDR                           \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_B_LP_MASK                           0x1
#define MT6359_DA_VCN33_1_B_LP_SHIFT                          2
#define MT6359_DA_VCN33_1_L_EN_ADDR                           \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_L_EN_MASK                           0x1
#define MT6359_DA_VCN33_1_L_EN_SHIFT                          3
#define MT6359_DA_VCN33_1_L_STB_ADDR                          \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_L_STB_MASK                          0x1
#define MT6359_DA_VCN33_1_L_STB_SHIFT                         4
#define MT6359_DA_VCN33_1_OCFB_EN_ADDR                        \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_OCFB_EN_MASK                        0x1
#define MT6359_DA_VCN33_1_OCFB_EN_SHIFT                       5
#define MT6359_DA_VCN33_1_DUMMY_LOAD_ADDR                     \
	MT6359_LDO_VCN33_1_MON
#define MT6359_DA_VCN33_1_DUMMY_LOAD_MASK                     0x3
#define MT6359_DA_VCN33_1_DUMMY_LOAD_SHIFT                    6
#define MT6359_RG_LDO_VCN33_1_HW0_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_LDO_VCN33_1_HW1_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_LDO_VCN33_1_HW2_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_LDO_VCN33_1_HW3_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_LDO_VCN33_1_HW4_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_LDO_VCN33_1_HW5_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_LDO_VCN33_1_HW6_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_LDO_VCN33_1_HW7_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_LDO_VCN33_1_HW8_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_LDO_VCN33_1_HW9_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_LDO_VCN33_1_HW10_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW10_OP_EN_SHIFT                10
#define MT6359_RG_LDO_VCN33_1_HW11_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW11_OP_EN_SHIFT                11
#define MT6359_RG_LDO_VCN33_1_HW12_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW12_OP_EN_SHIFT                12
#define MT6359_RG_LDO_VCN33_1_HW13_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW13_OP_EN_SHIFT                13
#define MT6359_RG_LDO_VCN33_1_HW14_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW14_OP_EN_SHIFT                14
#define MT6359_RG_LDO_VCN33_1_SW_OP_EN_ADDR                   \
	MT6359_LDO_VCN33_1_OP_EN
#define MT6359_RG_LDO_VCN33_1_SW_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN33_1_SW_OP_EN_SHIFT                  15
#define MT6359_RG_LDO_VCN33_1_OP_EN_SET_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN_SET
#define MT6359_RG_LDO_VCN33_1_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VCN33_1_OP_EN_SET_SHIFT                 0
#define MT6359_RG_LDO_VCN33_1_OP_EN_CLR_ADDR                  \
	MT6359_LDO_VCN33_1_OP_EN_CLR
#define MT6359_RG_LDO_VCN33_1_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VCN33_1_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_LDO_VCN33_1_HW0_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_LDO_VCN33_1_HW1_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_LDO_VCN33_1_HW2_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_LDO_VCN33_1_HW3_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_LDO_VCN33_1_HW4_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_LDO_VCN33_1_HW5_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_LDO_VCN33_1_HW6_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_LDO_VCN33_1_HW7_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_LDO_VCN33_1_HW8_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_LDO_VCN33_1_HW9_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_1_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_LDO_VCN33_1_HW10_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_1_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_LDO_VCN33_1_HW11_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_1_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_LDO_VCN33_1_HW12_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_1_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_LDO_VCN33_1_HW13_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_1_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_LDO_VCN33_1_HW14_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_1_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_LDO_VCN33_1_SW_OP_CFG_ADDR                  \
	MT6359_LDO_VCN33_1_OP_CFG
#define MT6359_RG_LDO_VCN33_1_SW_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN33_1_SW_OP_CFG_SHIFT                 15
#define MT6359_RG_LDO_VCN33_1_OP_CFG_SET_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG_SET
#define MT6359_RG_LDO_VCN33_1_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_LDO_VCN33_1_OP_CFG_SET_SHIFT                0
#define MT6359_RG_LDO_VCN33_1_OP_CFG_CLR_ADDR                 \
	MT6359_LDO_VCN33_1_OP_CFG_CLR
#define MT6359_RG_LDO_VCN33_1_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_LDO_VCN33_1_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_LDO_VCN33_1_EN_1_ADDR                       \
	MT6359_LDO_VCN33_1_MULTI_SW
#define MT6359_RG_LDO_VCN33_1_EN_1_MASK                       0x1
#define MT6359_RG_LDO_VCN33_1_EN_1_SHIFT                      15
#define MT6359_LDO_GNR1_ANA_ID_ADDR                           \
	MT6359_LDO_GNR1_DSN_ID
#define MT6359_LDO_GNR1_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR1_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR1_DIG_ID_ADDR                           \
	MT6359_LDO_GNR1_DSN_ID
#define MT6359_LDO_GNR1_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR1_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR1_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR1_DSN_REV0
#define MT6359_LDO_GNR1_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR1_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR1_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR1_DSN_REV0
#define MT6359_LDO_GNR1_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR1_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR1_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR1_DSN_REV0
#define MT6359_LDO_GNR1_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR1_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR1_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR1_DSN_REV0
#define MT6359_LDO_GNR1_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR1_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR1_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR1_DSN_DBI
#define MT6359_LDO_GNR1_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR1_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR1_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR1_DSN_DBI
#define MT6359_LDO_GNR1_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR1_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR1_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR1_DSN_DBI
#define MT6359_LDO_GNR1_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR1_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR1_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR1_DSN_DXI
#define MT6359_LDO_GNR1_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR1_DSN_FPI_SHIFT                         0
#define MT6359_RG_LDO_VCN33_2_EN_0_ADDR                       \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_EN_0_MASK                       0x1
#define MT6359_RG_LDO_VCN33_2_EN_0_SHIFT                      0
#define MT6359_RG_LDO_VCN33_2_LP_ADDR                         \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_LP_MASK                         0x1
#define MT6359_RG_LDO_VCN33_2_LP_SHIFT                        1
#define MT6359_RG_LDO_VCN33_2_STBTD_ADDR                      \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_STBTD_MASK                      0x3
#define MT6359_RG_LDO_VCN33_2_STBTD_SHIFT                     2
#define MT6359_RG_LDO_VCN33_2_ULP_ADDR                        \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_ULP_MASK                        0x1
#define MT6359_RG_LDO_VCN33_2_ULP_SHIFT                       4
#define MT6359_RG_LDO_VCN33_2_OCFB_EN_ADDR                    \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_OCFB_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN33_2_OCFB_EN_SHIFT                   5
#define MT6359_RG_LDO_VCN33_2_OC_MODE_ADDR                    \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_OC_MODE_MASK                    0x1
#define MT6359_RG_LDO_VCN33_2_OC_MODE_SHIFT                   6
#define MT6359_RG_LDO_VCN33_2_OC_TSEL_ADDR                    \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_OC_TSEL_MASK                    0x1
#define MT6359_RG_LDO_VCN33_2_OC_TSEL_SHIFT                   7
#define MT6359_RG_LDO_VCN33_2_DUMMY_LOAD_ADDR                 \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_DUMMY_LOAD_MASK                 0x3
#define MT6359_RG_LDO_VCN33_2_DUMMY_LOAD_SHIFT                8
#define MT6359_RG_LDO_VCN33_2_OP_MODE_ADDR                    \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_OP_MODE_MASK                    0x7
#define MT6359_RG_LDO_VCN33_2_OP_MODE_SHIFT                   10
#define MT6359_RG_LDO_VCN33_2_CK_SW_MODE_ADDR                 \
	MT6359_LDO_VCN33_2_CON0
#define MT6359_RG_LDO_VCN33_2_CK_SW_MODE_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_CK_SW_MODE_SHIFT                15
#define MT6359_DA_VCN33_2_B_EN_ADDR                           \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_B_EN_MASK                           0x1
#define MT6359_DA_VCN33_2_B_EN_SHIFT                          0
#define MT6359_DA_VCN33_2_B_STB_ADDR                          \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_B_STB_MASK                          0x1
#define MT6359_DA_VCN33_2_B_STB_SHIFT                         1
#define MT6359_DA_VCN33_2_B_LP_ADDR                           \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_B_LP_MASK                           0x1
#define MT6359_DA_VCN33_2_B_LP_SHIFT                          2
#define MT6359_DA_VCN33_2_L_EN_ADDR                           \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_L_EN_MASK                           0x1
#define MT6359_DA_VCN33_2_L_EN_SHIFT                          3
#define MT6359_DA_VCN33_2_L_STB_ADDR                          \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_L_STB_MASK                          0x1
#define MT6359_DA_VCN33_2_L_STB_SHIFT                         4
#define MT6359_DA_VCN33_2_OCFB_EN_ADDR                        \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_OCFB_EN_MASK                        0x1
#define MT6359_DA_VCN33_2_OCFB_EN_SHIFT                       5
#define MT6359_DA_VCN33_2_DUMMY_LOAD_ADDR                     \
	MT6359_LDO_VCN33_2_MON
#define MT6359_DA_VCN33_2_DUMMY_LOAD_MASK                     0x3
#define MT6359_DA_VCN33_2_DUMMY_LOAD_SHIFT                    6
#define MT6359_RG_LDO_VCN33_2_HW0_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW0_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW0_OP_EN_SHIFT                 0
#define MT6359_RG_LDO_VCN33_2_HW1_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW1_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW1_OP_EN_SHIFT                 1
#define MT6359_RG_LDO_VCN33_2_HW2_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW2_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW2_OP_EN_SHIFT                 2
#define MT6359_RG_LDO_VCN33_2_HW3_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW3_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW3_OP_EN_SHIFT                 3
#define MT6359_RG_LDO_VCN33_2_HW4_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW4_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW4_OP_EN_SHIFT                 4
#define MT6359_RG_LDO_VCN33_2_HW5_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW5_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW5_OP_EN_SHIFT                 5
#define MT6359_RG_LDO_VCN33_2_HW6_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW6_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW6_OP_EN_SHIFT                 6
#define MT6359_RG_LDO_VCN33_2_HW7_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW7_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW7_OP_EN_SHIFT                 7
#define MT6359_RG_LDO_VCN33_2_HW8_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW8_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW8_OP_EN_SHIFT                 8
#define MT6359_RG_LDO_VCN33_2_HW9_OP_EN_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW9_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_HW9_OP_EN_SHIFT                 9
#define MT6359_RG_LDO_VCN33_2_HW10_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW10_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW10_OP_EN_SHIFT                10
#define MT6359_RG_LDO_VCN33_2_HW11_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW11_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW11_OP_EN_SHIFT                11
#define MT6359_RG_LDO_VCN33_2_HW12_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW12_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW12_OP_EN_SHIFT                12
#define MT6359_RG_LDO_VCN33_2_HW13_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW13_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW13_OP_EN_SHIFT                13
#define MT6359_RG_LDO_VCN33_2_HW14_OP_EN_ADDR                 \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_HW14_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW14_OP_EN_SHIFT                14
#define MT6359_RG_LDO_VCN33_2_SW_OP_EN_ADDR                   \
	MT6359_LDO_VCN33_2_OP_EN
#define MT6359_RG_LDO_VCN33_2_SW_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN33_2_SW_OP_EN_SHIFT                  15
#define MT6359_RG_LDO_VCN33_2_OP_EN_SET_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN_SET
#define MT6359_RG_LDO_VCN33_2_OP_EN_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VCN33_2_OP_EN_SET_SHIFT                 0
#define MT6359_RG_LDO_VCN33_2_OP_EN_CLR_ADDR                  \
	MT6359_LDO_VCN33_2_OP_EN_CLR
#define MT6359_RG_LDO_VCN33_2_OP_EN_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VCN33_2_OP_EN_CLR_SHIFT                 0
#define MT6359_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW0_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW0_OP_CFG_SHIFT                0
#define MT6359_RG_LDO_VCN33_2_HW1_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW1_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW1_OP_CFG_SHIFT                1
#define MT6359_RG_LDO_VCN33_2_HW2_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW2_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW2_OP_CFG_SHIFT                2
#define MT6359_RG_LDO_VCN33_2_HW3_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW3_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW3_OP_CFG_SHIFT                3
#define MT6359_RG_LDO_VCN33_2_HW4_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW4_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW4_OP_CFG_SHIFT                4
#define MT6359_RG_LDO_VCN33_2_HW5_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW5_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW5_OP_CFG_SHIFT                5
#define MT6359_RG_LDO_VCN33_2_HW6_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW6_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW6_OP_CFG_SHIFT                6
#define MT6359_RG_LDO_VCN33_2_HW7_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW7_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW7_OP_CFG_SHIFT                7
#define MT6359_RG_LDO_VCN33_2_HW8_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW8_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW8_OP_CFG_SHIFT                8
#define MT6359_RG_LDO_VCN33_2_HW9_OP_CFG_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW9_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCN33_2_HW9_OP_CFG_SHIFT                9
#define MT6359_RG_LDO_VCN33_2_HW10_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW10_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_2_HW10_OP_CFG_SHIFT               10
#define MT6359_RG_LDO_VCN33_2_HW11_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW11_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_2_HW11_OP_CFG_SHIFT               11
#define MT6359_RG_LDO_VCN33_2_HW12_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW12_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_2_HW12_OP_CFG_SHIFT               12
#define MT6359_RG_LDO_VCN33_2_HW13_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW13_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_2_HW13_OP_CFG_SHIFT               13
#define MT6359_RG_LDO_VCN33_2_HW14_OP_CFG_ADDR                \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_HW14_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VCN33_2_HW14_OP_CFG_SHIFT               14
#define MT6359_RG_LDO_VCN33_2_SW_OP_CFG_ADDR                  \
	MT6359_LDO_VCN33_2_OP_CFG
#define MT6359_RG_LDO_VCN33_2_SW_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN33_2_SW_OP_CFG_SHIFT                 15
#define MT6359_RG_LDO_VCN33_2_OP_CFG_SET_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG_SET
#define MT6359_RG_LDO_VCN33_2_OP_CFG_SET_MASK                 0xFFFF
#define MT6359_RG_LDO_VCN33_2_OP_CFG_SET_SHIFT                0
#define MT6359_RG_LDO_VCN33_2_OP_CFG_CLR_ADDR                 \
	MT6359_LDO_VCN33_2_OP_CFG_CLR
#define MT6359_RG_LDO_VCN33_2_OP_CFG_CLR_MASK                 0xFFFF
#define MT6359_RG_LDO_VCN33_2_OP_CFG_CLR_SHIFT                0
#define MT6359_RG_LDO_VCN33_2_EN_1_ADDR                       \
	MT6359_LDO_VCN33_2_MULTI_SW
#define MT6359_RG_LDO_VCN33_2_EN_1_MASK                       0x1
#define MT6359_RG_LDO_VCN33_2_EN_1_SHIFT                      15
#define MT6359_RG_LDO_VCN13_EN_ADDR                           \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_EN_MASK                           0x1
#define MT6359_RG_LDO_VCN13_EN_SHIFT                          0
#define MT6359_RG_LDO_VCN13_LP_ADDR                           \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_LP_MASK                           0x1
#define MT6359_RG_LDO_VCN13_LP_SHIFT                          1
#define MT6359_RG_LDO_VCN13_STBTD_ADDR                        \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VCN13_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VCN13_ULP_ADDR                          \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_ULP_MASK                          0x1
#define MT6359_RG_LDO_VCN13_ULP_SHIFT                         4
#define MT6359_RG_LDO_VCN13_OCFB_EN_ADDR                      \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VCN13_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VCN13_OC_MODE_ADDR                      \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VCN13_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VCN13_OC_TSEL_ADDR                      \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VCN13_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VCN13_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VCN13_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VCN13_OP_MODE_ADDR                      \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VCN13_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VCN13_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VCN13_CON0
#define MT6359_RG_LDO_VCN13_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VCN13_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VCN13_B_EN_ADDR                             \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_B_EN_MASK                             0x1
#define MT6359_DA_VCN13_B_EN_SHIFT                            0
#define MT6359_DA_VCN13_B_STB_ADDR                            \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_B_STB_MASK                            0x1
#define MT6359_DA_VCN13_B_STB_SHIFT                           1
#define MT6359_DA_VCN13_B_LP_ADDR                             \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_B_LP_MASK                             0x1
#define MT6359_DA_VCN13_B_LP_SHIFT                            2
#define MT6359_DA_VCN13_L_EN_ADDR                             \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_L_EN_MASK                             0x1
#define MT6359_DA_VCN13_L_EN_SHIFT                            3
#define MT6359_DA_VCN13_L_STB_ADDR                            \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_L_STB_MASK                            0x1
#define MT6359_DA_VCN13_L_STB_SHIFT                           4
#define MT6359_DA_VCN13_OCFB_EN_ADDR                          \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_OCFB_EN_MASK                          0x1
#define MT6359_DA_VCN13_OCFB_EN_SHIFT                         5
#define MT6359_DA_VCN13_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VCN13_MON
#define MT6359_DA_VCN13_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VCN13_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VCN13_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VCN13_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VCN13_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VCN13_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VCN13_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VCN13_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VCN13_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VCN13_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VCN13_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VCN13_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN13_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VCN13_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VCN13_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VCN13_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VCN13_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VCN13_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VCN13_SW_OP_EN_ADDR                     \
	MT6359_LDO_VCN13_OP_EN
#define MT6359_RG_LDO_VCN13_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VCN13_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VCN13_OP_EN_SET_ADDR                    \
	MT6359_LDO_VCN13_OP_EN_SET
#define MT6359_RG_LDO_VCN13_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VCN13_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VCN13_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VCN13_OP_EN_CLR
#define MT6359_RG_LDO_VCN13_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VCN13_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VCN13_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VCN13_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VCN13_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VCN13_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VCN13_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VCN13_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VCN13_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VCN13_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VCN13_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VCN13_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN13_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VCN13_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN13_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VCN13_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN13_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VCN13_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN13_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VCN13_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN13_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VCN13_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN13_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VCN13_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VCN13_OP_CFG
#define MT6359_RG_LDO_VCN13_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VCN13_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VCN13_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG_SET
#define MT6359_RG_LDO_VCN13_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VCN13_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VCN13_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VCN13_OP_CFG_CLR
#define MT6359_RG_LDO_VCN13_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VCN13_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VCN18_EN_ADDR                           \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_EN_MASK                           0x1
#define MT6359_RG_LDO_VCN18_EN_SHIFT                          0
#define MT6359_RG_LDO_VCN18_LP_ADDR                           \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_LP_MASK                           0x1
#define MT6359_RG_LDO_VCN18_LP_SHIFT                          1
#define MT6359_RG_LDO_VCN18_STBTD_ADDR                        \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VCN18_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VCN18_ULP_ADDR                          \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_ULP_MASK                          0x1
#define MT6359_RG_LDO_VCN18_ULP_SHIFT                         4
#define MT6359_RG_LDO_VCN18_OCFB_EN_ADDR                      \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VCN18_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VCN18_OC_MODE_ADDR                      \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VCN18_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VCN18_OC_TSEL_ADDR                      \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VCN18_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VCN18_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VCN18_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VCN18_OP_MODE_ADDR                      \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VCN18_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VCN18_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VCN18_CON0
#define MT6359_RG_LDO_VCN18_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VCN18_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VCN18_B_EN_ADDR                             \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_B_EN_MASK                             0x1
#define MT6359_DA_VCN18_B_EN_SHIFT                            0
#define MT6359_DA_VCN18_B_STB_ADDR                            \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_B_STB_MASK                            0x1
#define MT6359_DA_VCN18_B_STB_SHIFT                           1
#define MT6359_DA_VCN18_B_LP_ADDR                             \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_B_LP_MASK                             0x1
#define MT6359_DA_VCN18_B_LP_SHIFT                            2
#define MT6359_DA_VCN18_L_EN_ADDR                             \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_L_EN_MASK                             0x1
#define MT6359_DA_VCN18_L_EN_SHIFT                            3
#define MT6359_DA_VCN18_L_STB_ADDR                            \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_L_STB_MASK                            0x1
#define MT6359_DA_VCN18_L_STB_SHIFT                           4
#define MT6359_DA_VCN18_OCFB_EN_ADDR                          \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_OCFB_EN_MASK                          0x1
#define MT6359_DA_VCN18_OCFB_EN_SHIFT                         5
#define MT6359_DA_VCN18_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VCN18_MON
#define MT6359_DA_VCN18_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VCN18_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VCN18_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VCN18_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VCN18_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VCN18_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VCN18_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VCN18_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VCN18_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VCN18_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VCN18_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VCN18_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCN18_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VCN18_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VCN18_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VCN18_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VCN18_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VCN18_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VCN18_SW_OP_EN_ADDR                     \
	MT6359_LDO_VCN18_OP_EN
#define MT6359_RG_LDO_VCN18_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VCN18_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VCN18_OP_EN_SET_ADDR                    \
	MT6359_LDO_VCN18_OP_EN_SET
#define MT6359_RG_LDO_VCN18_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VCN18_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VCN18_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VCN18_OP_EN_CLR
#define MT6359_RG_LDO_VCN18_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VCN18_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VCN18_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VCN18_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VCN18_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VCN18_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VCN18_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VCN18_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VCN18_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VCN18_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VCN18_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VCN18_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCN18_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VCN18_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN18_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VCN18_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN18_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VCN18_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN18_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VCN18_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN18_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VCN18_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCN18_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VCN18_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VCN18_OP_CFG
#define MT6359_RG_LDO_VCN18_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VCN18_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VCN18_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG_SET
#define MT6359_RG_LDO_VCN18_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VCN18_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VCN18_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VCN18_OP_CFG_CLR
#define MT6359_RG_LDO_VCN18_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VCN18_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VA09_EN_ADDR                            \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_EN_MASK                            0x1
#define MT6359_RG_LDO_VA09_EN_SHIFT                           0
#define MT6359_RG_LDO_VA09_LP_ADDR                            \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_LP_MASK                            0x1
#define MT6359_RG_LDO_VA09_LP_SHIFT                           1
#define MT6359_RG_LDO_VA09_STBTD_ADDR                         \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VA09_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VA09_ULP_ADDR                           \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_ULP_MASK                           0x1
#define MT6359_RG_LDO_VA09_ULP_SHIFT                          4
#define MT6359_RG_LDO_VA09_OCFB_EN_ADDR                       \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VA09_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VA09_OC_MODE_ADDR                       \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VA09_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VA09_OC_TSEL_ADDR                       \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VA09_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VA09_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VA09_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VA09_OP_MODE_ADDR                       \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VA09_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VA09_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VA09_CON0
#define MT6359_RG_LDO_VA09_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VA09_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VA09_B_EN_ADDR                              \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_B_EN_MASK                              0x1
#define MT6359_DA_VA09_B_EN_SHIFT                             0
#define MT6359_DA_VA09_B_STB_ADDR                             \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_B_STB_MASK                             0x1
#define MT6359_DA_VA09_B_STB_SHIFT                            1
#define MT6359_DA_VA09_B_LP_ADDR                              \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_B_LP_MASK                              0x1
#define MT6359_DA_VA09_B_LP_SHIFT                             2
#define MT6359_DA_VA09_L_EN_ADDR                              \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_L_EN_MASK                              0x1
#define MT6359_DA_VA09_L_EN_SHIFT                             3
#define MT6359_DA_VA09_L_STB_ADDR                             \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_L_STB_MASK                             0x1
#define MT6359_DA_VA09_L_STB_SHIFT                            4
#define MT6359_DA_VA09_OCFB_EN_ADDR                           \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_OCFB_EN_MASK                           0x1
#define MT6359_DA_VA09_OCFB_EN_SHIFT                          5
#define MT6359_DA_VA09_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VA09_MON
#define MT6359_DA_VA09_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VA09_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VA09_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VA09_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VA09_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VA09_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VA09_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VA09_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VA09_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VA09_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VA09_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VA09_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA09_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VA09_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VA09_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VA09_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VA09_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VA09_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VA09_SW_OP_EN_ADDR                      \
	MT6359_LDO_VA09_OP_EN
#define MT6359_RG_LDO_VA09_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VA09_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VA09_OP_EN_SET_ADDR                     \
	MT6359_LDO_VA09_OP_EN_SET
#define MT6359_RG_LDO_VA09_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VA09_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VA09_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VA09_OP_EN_CLR
#define MT6359_RG_LDO_VA09_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VA09_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VA09_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VA09_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VA09_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VA09_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VA09_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VA09_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VA09_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VA09_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VA09_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VA09_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA09_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VA09_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA09_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VA09_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA09_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VA09_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA09_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VA09_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA09_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VA09_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA09_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VA09_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VA09_OP_CFG
#define MT6359_RG_LDO_VA09_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VA09_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VA09_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VA09_OP_CFG_SET
#define MT6359_RG_LDO_VA09_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VA09_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VA09_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VA09_OP_CFG_CLR
#define MT6359_RG_LDO_VA09_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VA09_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_LDO_VCAMIO_EN_ADDR                          \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_EN_MASK                          0x1
#define MT6359_RG_LDO_VCAMIO_EN_SHIFT                         0
#define MT6359_RG_LDO_VCAMIO_LP_ADDR                          \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_LP_MASK                          0x1
#define MT6359_RG_LDO_VCAMIO_LP_SHIFT                         1
#define MT6359_RG_LDO_VCAMIO_STBTD_ADDR                       \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_STBTD_MASK                       0x3
#define MT6359_RG_LDO_VCAMIO_STBTD_SHIFT                      2
#define MT6359_RG_LDO_VCAMIO_ULP_ADDR                         \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_ULP_MASK                         0x1
#define MT6359_RG_LDO_VCAMIO_ULP_SHIFT                        4
#define MT6359_RG_LDO_VCAMIO_OCFB_EN_ADDR                     \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_OCFB_EN_MASK                     0x1
#define MT6359_RG_LDO_VCAMIO_OCFB_EN_SHIFT                    5
#define MT6359_RG_LDO_VCAMIO_OC_MODE_ADDR                     \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_OC_MODE_MASK                     0x1
#define MT6359_RG_LDO_VCAMIO_OC_MODE_SHIFT                    6
#define MT6359_RG_LDO_VCAMIO_OC_TSEL_ADDR                     \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_OC_TSEL_MASK                     0x1
#define MT6359_RG_LDO_VCAMIO_OC_TSEL_SHIFT                    7
#define MT6359_RG_LDO_VCAMIO_DUMMY_LOAD_ADDR                  \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_DUMMY_LOAD_MASK                  0x3
#define MT6359_RG_LDO_VCAMIO_DUMMY_LOAD_SHIFT                 8
#define MT6359_RG_LDO_VCAMIO_OP_MODE_ADDR                     \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_OP_MODE_MASK                     0x7
#define MT6359_RG_LDO_VCAMIO_OP_MODE_SHIFT                    10
#define MT6359_RG_LDO_VCAMIO_CK_SW_MODE_ADDR                  \
	MT6359_LDO_VCAMIO_CON0
#define MT6359_RG_LDO_VCAMIO_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_CK_SW_MODE_SHIFT                 15
#define MT6359_DA_VCAMIO_B_EN_ADDR                            \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_B_EN_MASK                            0x1
#define MT6359_DA_VCAMIO_B_EN_SHIFT                           0
#define MT6359_DA_VCAMIO_B_STB_ADDR                           \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_B_STB_MASK                           0x1
#define MT6359_DA_VCAMIO_B_STB_SHIFT                          1
#define MT6359_DA_VCAMIO_B_LP_ADDR                            \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_B_LP_MASK                            0x1
#define MT6359_DA_VCAMIO_B_LP_SHIFT                           2
#define MT6359_DA_VCAMIO_L_EN_ADDR                            \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_L_EN_MASK                            0x1
#define MT6359_DA_VCAMIO_L_EN_SHIFT                           3
#define MT6359_DA_VCAMIO_L_STB_ADDR                           \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_L_STB_MASK                           0x1
#define MT6359_DA_VCAMIO_L_STB_SHIFT                          4
#define MT6359_DA_VCAMIO_OCFB_EN_ADDR                         \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_OCFB_EN_MASK                         0x1
#define MT6359_DA_VCAMIO_OCFB_EN_SHIFT                        5
#define MT6359_DA_VCAMIO_DUMMY_LOAD_ADDR                      \
	MT6359_LDO_VCAMIO_MON
#define MT6359_DA_VCAMIO_DUMMY_LOAD_MASK                      0x3
#define MT6359_DA_VCAMIO_DUMMY_LOAD_SHIFT                     6
#define MT6359_RG_LDO_VCAMIO_HW0_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_LDO_VCAMIO_HW1_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_LDO_VCAMIO_HW2_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_LDO_VCAMIO_HW3_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_LDO_VCAMIO_HW4_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_LDO_VCAMIO_HW5_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_LDO_VCAMIO_HW6_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_LDO_VCAMIO_HW7_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_LDO_VCAMIO_HW8_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_LDO_VCAMIO_HW9_OP_EN_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_LDO_VCAMIO_HW10_OP_EN_ADDR                  \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_LDO_VCAMIO_HW11_OP_EN_ADDR                  \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_LDO_VCAMIO_HW12_OP_EN_ADDR                  \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_LDO_VCAMIO_HW13_OP_EN_ADDR                  \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_LDO_VCAMIO_HW14_OP_EN_ADDR                  \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_LDO_VCAMIO_SW_OP_EN_ADDR                    \
	MT6359_LDO_VCAMIO_OP_EN
#define MT6359_RG_LDO_VCAMIO_SW_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VCAMIO_SW_OP_EN_SHIFT                   15
#define MT6359_RG_LDO_VCAMIO_OP_EN_SET_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN_SET
#define MT6359_RG_LDO_VCAMIO_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VCAMIO_OP_EN_SET_SHIFT                  0
#define MT6359_RG_LDO_VCAMIO_OP_EN_CLR_ADDR                   \
	MT6359_LDO_VCAMIO_OP_EN_CLR
#define MT6359_RG_LDO_VCAMIO_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VCAMIO_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_LDO_VCAMIO_HW0_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_LDO_VCAMIO_HW1_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_LDO_VCAMIO_HW2_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_LDO_VCAMIO_HW3_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_LDO_VCAMIO_HW4_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_LDO_VCAMIO_HW5_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_LDO_VCAMIO_HW6_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_LDO_VCAMIO_HW7_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_LDO_VCAMIO_HW8_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_LDO_VCAMIO_HW9_OP_CFG_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VCAMIO_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_LDO_VCAMIO_HW10_OP_CFG_ADDR                 \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCAMIO_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_LDO_VCAMIO_HW11_OP_CFG_ADDR                 \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCAMIO_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_LDO_VCAMIO_HW12_OP_CFG_ADDR                 \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCAMIO_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_LDO_VCAMIO_HW13_OP_CFG_ADDR                 \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCAMIO_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_LDO_VCAMIO_HW14_OP_CFG_ADDR                 \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VCAMIO_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_LDO_VCAMIO_SW_OP_CFG_ADDR                   \
	MT6359_LDO_VCAMIO_OP_CFG
#define MT6359_RG_LDO_VCAMIO_SW_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VCAMIO_SW_OP_CFG_SHIFT                  15
#define MT6359_RG_LDO_VCAMIO_OP_CFG_SET_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG_SET
#define MT6359_RG_LDO_VCAMIO_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VCAMIO_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_LDO_VCAMIO_OP_CFG_CLR_ADDR                  \
	MT6359_LDO_VCAMIO_OP_CFG_CLR
#define MT6359_RG_LDO_VCAMIO_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VCAMIO_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_LDO_VA12_EN_ADDR                            \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_EN_MASK                            0x1
#define MT6359_RG_LDO_VA12_EN_SHIFT                           0
#define MT6359_RG_LDO_VA12_LP_ADDR                            \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_LP_MASK                            0x1
#define MT6359_RG_LDO_VA12_LP_SHIFT                           1
#define MT6359_RG_LDO_VA12_STBTD_ADDR                         \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VA12_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VA12_ULP_ADDR                           \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_ULP_MASK                           0x1
#define MT6359_RG_LDO_VA12_ULP_SHIFT                          4
#define MT6359_RG_LDO_VA12_OCFB_EN_ADDR                       \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VA12_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VA12_OC_MODE_ADDR                       \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VA12_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VA12_OC_TSEL_ADDR                       \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VA12_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VA12_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VA12_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VA12_OP_MODE_ADDR                       \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VA12_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VA12_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VA12_CON0
#define MT6359_RG_LDO_VA12_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VA12_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VA12_B_EN_ADDR                              \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_B_EN_MASK                              0x1
#define MT6359_DA_VA12_B_EN_SHIFT                             0
#define MT6359_DA_VA12_B_STB_ADDR                             \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_B_STB_MASK                             0x1
#define MT6359_DA_VA12_B_STB_SHIFT                            1
#define MT6359_DA_VA12_B_LP_ADDR                              \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_B_LP_MASK                              0x1
#define MT6359_DA_VA12_B_LP_SHIFT                             2
#define MT6359_DA_VA12_L_EN_ADDR                              \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_L_EN_MASK                              0x1
#define MT6359_DA_VA12_L_EN_SHIFT                             3
#define MT6359_DA_VA12_L_STB_ADDR                             \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_L_STB_MASK                             0x1
#define MT6359_DA_VA12_L_STB_SHIFT                            4
#define MT6359_DA_VA12_OCFB_EN_ADDR                           \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_OCFB_EN_MASK                           0x1
#define MT6359_DA_VA12_OCFB_EN_SHIFT                          5
#define MT6359_DA_VA12_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VA12_MON
#define MT6359_DA_VA12_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VA12_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VA12_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VA12_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VA12_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VA12_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VA12_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VA12_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VA12_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VA12_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VA12_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VA12_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VA12_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VA12_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VA12_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VA12_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VA12_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VA12_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VA12_SW_OP_EN_ADDR                      \
	MT6359_LDO_VA12_OP_EN
#define MT6359_RG_LDO_VA12_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VA12_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VA12_OP_EN_SET_ADDR                     \
	MT6359_LDO_VA12_OP_EN_SET
#define MT6359_RG_LDO_VA12_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VA12_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VA12_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VA12_OP_EN_CLR
#define MT6359_RG_LDO_VA12_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VA12_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VA12_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VA12_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VA12_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VA12_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VA12_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VA12_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VA12_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VA12_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VA12_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VA12_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VA12_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VA12_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA12_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VA12_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA12_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VA12_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA12_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VA12_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA12_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VA12_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VA12_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VA12_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VA12_OP_CFG
#define MT6359_RG_LDO_VA12_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VA12_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VA12_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VA12_OP_CFG_SET
#define MT6359_RG_LDO_VA12_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VA12_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VA12_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VA12_OP_CFG_CLR
#define MT6359_RG_LDO_VA12_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VA12_OP_CFG_CLR_SHIFT                   0
#define MT6359_LDO_GNR2_ANA_ID_ADDR                           \
	MT6359_LDO_GNR2_DSN_ID
#define MT6359_LDO_GNR2_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR2_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR2_DIG_ID_ADDR                           \
	MT6359_LDO_GNR2_DSN_ID
#define MT6359_LDO_GNR2_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR2_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR2_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR2_DSN_REV0
#define MT6359_LDO_GNR2_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR2_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR2_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR2_DSN_REV0
#define MT6359_LDO_GNR2_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR2_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR2_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR2_DSN_REV0
#define MT6359_LDO_GNR2_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR2_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR2_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR2_DSN_REV0
#define MT6359_LDO_GNR2_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR2_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR2_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR2_DSN_DBI
#define MT6359_LDO_GNR2_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR2_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR2_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR2_DSN_DBI
#define MT6359_LDO_GNR2_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR2_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR2_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR2_DSN_DBI
#define MT6359_LDO_GNR2_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR2_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR2_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR2_DSN_DXI
#define MT6359_LDO_GNR2_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR2_DSN_FPI_SHIFT                         0
#define MT6359_RG_LDO_VAUX18_EN_ADDR                          \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_EN_MASK                          0x1
#define MT6359_RG_LDO_VAUX18_EN_SHIFT                         0
#define MT6359_RG_LDO_VAUX18_LP_ADDR                          \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_LP_MASK                          0x1
#define MT6359_RG_LDO_VAUX18_LP_SHIFT                         1
#define MT6359_RG_LDO_VAUX18_STBTD_ADDR                       \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_STBTD_MASK                       0x3
#define MT6359_RG_LDO_VAUX18_STBTD_SHIFT                      2
#define MT6359_RG_LDO_VAUX18_ULP_ADDR                         \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_ULP_MASK                         0x1
#define MT6359_RG_LDO_VAUX18_ULP_SHIFT                        4
#define MT6359_RG_LDO_VAUX18_OCFB_EN_ADDR                     \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_OCFB_EN_MASK                     0x1
#define MT6359_RG_LDO_VAUX18_OCFB_EN_SHIFT                    5
#define MT6359_RG_LDO_VAUX18_OC_MODE_ADDR                     \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_OC_MODE_MASK                     0x1
#define MT6359_RG_LDO_VAUX18_OC_MODE_SHIFT                    6
#define MT6359_RG_LDO_VAUX18_OC_TSEL_ADDR                     \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_OC_TSEL_MASK                     0x1
#define MT6359_RG_LDO_VAUX18_OC_TSEL_SHIFT                    7
#define MT6359_RG_LDO_VAUX18_DUMMY_LOAD_ADDR                  \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_DUMMY_LOAD_MASK                  0x3
#define MT6359_RG_LDO_VAUX18_DUMMY_LOAD_SHIFT                 8
#define MT6359_RG_LDO_VAUX18_OP_MODE_ADDR                     \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_OP_MODE_MASK                     0x7
#define MT6359_RG_LDO_VAUX18_OP_MODE_SHIFT                    10
#define MT6359_RG_LDO_VAUX18_CK_SW_MODE_ADDR                  \
	MT6359_LDO_VAUX18_CON0
#define MT6359_RG_LDO_VAUX18_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_CK_SW_MODE_SHIFT                 15
#define MT6359_DA_VAUX18_B_EN_ADDR                            \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_B_EN_MASK                            0x1
#define MT6359_DA_VAUX18_B_EN_SHIFT                           0
#define MT6359_DA_VAUX18_B_STB_ADDR                           \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_B_STB_MASK                           0x1
#define MT6359_DA_VAUX18_B_STB_SHIFT                          1
#define MT6359_DA_VAUX18_B_LP_ADDR                            \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_B_LP_MASK                            0x1
#define MT6359_DA_VAUX18_B_LP_SHIFT                           2
#define MT6359_DA_VAUX18_L_EN_ADDR                            \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_L_EN_MASK                            0x1
#define MT6359_DA_VAUX18_L_EN_SHIFT                           3
#define MT6359_DA_VAUX18_L_STB_ADDR                           \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_L_STB_MASK                           0x1
#define MT6359_DA_VAUX18_L_STB_SHIFT                          4
#define MT6359_DA_VAUX18_OCFB_EN_ADDR                         \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_OCFB_EN_MASK                         0x1
#define MT6359_DA_VAUX18_OCFB_EN_SHIFT                        5
#define MT6359_DA_VAUX18_DUMMY_LOAD_ADDR                      \
	MT6359_LDO_VAUX18_MON
#define MT6359_DA_VAUX18_DUMMY_LOAD_MASK                      0x3
#define MT6359_DA_VAUX18_DUMMY_LOAD_SHIFT                     6
#define MT6359_RG_LDO_VAUX18_HW0_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_LDO_VAUX18_HW1_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_LDO_VAUX18_HW2_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_LDO_VAUX18_HW3_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_LDO_VAUX18_HW4_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_LDO_VAUX18_HW5_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_LDO_VAUX18_HW6_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_LDO_VAUX18_HW7_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_LDO_VAUX18_HW8_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_LDO_VAUX18_HW9_OP_EN_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_LDO_VAUX18_HW10_OP_EN_ADDR                  \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_LDO_VAUX18_HW11_OP_EN_ADDR                  \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_LDO_VAUX18_HW12_OP_EN_ADDR                  \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_LDO_VAUX18_HW13_OP_EN_ADDR                  \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_LDO_VAUX18_HW14_OP_EN_ADDR                  \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_LDO_VAUX18_SW_OP_EN_ADDR                    \
	MT6359_LDO_VAUX18_OP_EN
#define MT6359_RG_LDO_VAUX18_SW_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VAUX18_SW_OP_EN_SHIFT                   15
#define MT6359_RG_LDO_VAUX18_OP_EN_SET_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN_SET
#define MT6359_RG_LDO_VAUX18_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VAUX18_OP_EN_SET_SHIFT                  0
#define MT6359_RG_LDO_VAUX18_OP_EN_CLR_ADDR                   \
	MT6359_LDO_VAUX18_OP_EN_CLR
#define MT6359_RG_LDO_VAUX18_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VAUX18_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_LDO_VAUX18_HW0_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_LDO_VAUX18_HW1_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_LDO_VAUX18_HW2_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_LDO_VAUX18_HW3_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_LDO_VAUX18_HW4_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_LDO_VAUX18_HW5_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_LDO_VAUX18_HW6_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_LDO_VAUX18_HW7_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_LDO_VAUX18_HW8_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_LDO_VAUX18_HW9_OP_CFG_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUX18_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_LDO_VAUX18_HW10_OP_CFG_ADDR                 \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUX18_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_LDO_VAUX18_HW11_OP_CFG_ADDR                 \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUX18_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_LDO_VAUX18_HW12_OP_CFG_ADDR                 \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUX18_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_LDO_VAUX18_HW13_OP_CFG_ADDR                 \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUX18_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_LDO_VAUX18_HW14_OP_CFG_ADDR                 \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUX18_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_LDO_VAUX18_SW_OP_CFG_ADDR                   \
	MT6359_LDO_VAUX18_OP_CFG
#define MT6359_RG_LDO_VAUX18_SW_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VAUX18_SW_OP_CFG_SHIFT                  15
#define MT6359_RG_LDO_VAUX18_OP_CFG_SET_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG_SET
#define MT6359_RG_LDO_VAUX18_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VAUX18_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_LDO_VAUX18_OP_CFG_CLR_ADDR                  \
	MT6359_LDO_VAUX18_OP_CFG_CLR
#define MT6359_RG_LDO_VAUX18_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VAUX18_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_LDO_VAUD18_EN_ADDR                          \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_EN_MASK                          0x1
#define MT6359_RG_LDO_VAUD18_EN_SHIFT                         0
#define MT6359_RG_LDO_VAUD18_LP_ADDR                          \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_LP_MASK                          0x1
#define MT6359_RG_LDO_VAUD18_LP_SHIFT                         1
#define MT6359_RG_LDO_VAUD18_STBTD_ADDR                       \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_STBTD_MASK                       0x3
#define MT6359_RG_LDO_VAUD18_STBTD_SHIFT                      2
#define MT6359_RG_LDO_VAUD18_ULP_ADDR                         \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_ULP_MASK                         0x1
#define MT6359_RG_LDO_VAUD18_ULP_SHIFT                        4
#define MT6359_RG_LDO_VAUD18_OCFB_EN_ADDR                     \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_OCFB_EN_MASK                     0x1
#define MT6359_RG_LDO_VAUD18_OCFB_EN_SHIFT                    5
#define MT6359_RG_LDO_VAUD18_OC_MODE_ADDR                     \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_OC_MODE_MASK                     0x1
#define MT6359_RG_LDO_VAUD18_OC_MODE_SHIFT                    6
#define MT6359_RG_LDO_VAUD18_OC_TSEL_ADDR                     \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_OC_TSEL_MASK                     0x1
#define MT6359_RG_LDO_VAUD18_OC_TSEL_SHIFT                    7
#define MT6359_RG_LDO_VAUD18_DUMMY_LOAD_ADDR                  \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_DUMMY_LOAD_MASK                  0x3
#define MT6359_RG_LDO_VAUD18_DUMMY_LOAD_SHIFT                 8
#define MT6359_RG_LDO_VAUD18_OP_MODE_ADDR                     \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_OP_MODE_MASK                     0x7
#define MT6359_RG_LDO_VAUD18_OP_MODE_SHIFT                    10
#define MT6359_RG_LDO_VAUD18_CK_SW_MODE_ADDR                  \
	MT6359_LDO_VAUD18_CON0
#define MT6359_RG_LDO_VAUD18_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_CK_SW_MODE_SHIFT                 15
#define MT6359_DA_VAUD18_B_EN_ADDR                            \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_B_EN_MASK                            0x1
#define MT6359_DA_VAUD18_B_EN_SHIFT                           0
#define MT6359_DA_VAUD18_B_STB_ADDR                           \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_B_STB_MASK                           0x1
#define MT6359_DA_VAUD18_B_STB_SHIFT                          1
#define MT6359_DA_VAUD18_B_LP_ADDR                            \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_B_LP_MASK                            0x1
#define MT6359_DA_VAUD18_B_LP_SHIFT                           2
#define MT6359_DA_VAUD18_L_EN_ADDR                            \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_L_EN_MASK                            0x1
#define MT6359_DA_VAUD18_L_EN_SHIFT                           3
#define MT6359_DA_VAUD18_L_STB_ADDR                           \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_L_STB_MASK                           0x1
#define MT6359_DA_VAUD18_L_STB_SHIFT                          4
#define MT6359_DA_VAUD18_OCFB_EN_ADDR                         \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_OCFB_EN_MASK                         0x1
#define MT6359_DA_VAUD18_OCFB_EN_SHIFT                        5
#define MT6359_DA_VAUD18_DUMMY_LOAD_ADDR                      \
	MT6359_LDO_VAUD18_MON
#define MT6359_DA_VAUD18_DUMMY_LOAD_MASK                      0x3
#define MT6359_DA_VAUD18_DUMMY_LOAD_SHIFT                     6
#define MT6359_RG_LDO_VAUD18_HW0_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_LDO_VAUD18_HW1_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_LDO_VAUD18_HW2_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_LDO_VAUD18_HW3_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_LDO_VAUD18_HW4_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_LDO_VAUD18_HW5_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_LDO_VAUD18_HW6_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_LDO_VAUD18_HW7_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_LDO_VAUD18_HW8_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_LDO_VAUD18_HW9_OP_EN_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_LDO_VAUD18_HW10_OP_EN_ADDR                  \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_LDO_VAUD18_HW11_OP_EN_ADDR                  \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_LDO_VAUD18_HW12_OP_EN_ADDR                  \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_LDO_VAUD18_HW13_OP_EN_ADDR                  \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_LDO_VAUD18_HW14_OP_EN_ADDR                  \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_LDO_VAUD18_SW_OP_EN_ADDR                    \
	MT6359_LDO_VAUD18_OP_EN
#define MT6359_RG_LDO_VAUD18_SW_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VAUD18_SW_OP_EN_SHIFT                   15
#define MT6359_RG_LDO_VAUD18_OP_EN_SET_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN_SET
#define MT6359_RG_LDO_VAUD18_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VAUD18_OP_EN_SET_SHIFT                  0
#define MT6359_RG_LDO_VAUD18_OP_EN_CLR_ADDR                   \
	MT6359_LDO_VAUD18_OP_EN_CLR
#define MT6359_RG_LDO_VAUD18_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VAUD18_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_LDO_VAUD18_HW0_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_LDO_VAUD18_HW1_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_LDO_VAUD18_HW2_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_LDO_VAUD18_HW3_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_LDO_VAUD18_HW4_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_LDO_VAUD18_HW5_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_LDO_VAUD18_HW6_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_LDO_VAUD18_HW7_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_LDO_VAUD18_HW8_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_LDO_VAUD18_HW9_OP_CFG_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VAUD18_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_LDO_VAUD18_HW10_OP_CFG_ADDR                 \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUD18_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_LDO_VAUD18_HW11_OP_CFG_ADDR                 \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUD18_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_LDO_VAUD18_HW12_OP_CFG_ADDR                 \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUD18_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_LDO_VAUD18_HW13_OP_CFG_ADDR                 \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUD18_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_LDO_VAUD18_HW14_OP_CFG_ADDR                 \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VAUD18_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_LDO_VAUD18_SW_OP_CFG_ADDR                   \
	MT6359_LDO_VAUD18_OP_CFG
#define MT6359_RG_LDO_VAUD18_SW_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VAUD18_SW_OP_CFG_SHIFT                  15
#define MT6359_RG_LDO_VAUD18_OP_CFG_SET_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG_SET
#define MT6359_RG_LDO_VAUD18_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VAUD18_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_LDO_VAUD18_OP_CFG_CLR_ADDR                  \
	MT6359_LDO_VAUD18_OP_CFG_CLR
#define MT6359_RG_LDO_VAUD18_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VAUD18_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_LDO_VIO18_EN_ADDR                           \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_EN_MASK                           0x1
#define MT6359_RG_LDO_VIO18_EN_SHIFT                          0
#define MT6359_RG_LDO_VIO18_LP_ADDR                           \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_LP_MASK                           0x1
#define MT6359_RG_LDO_VIO18_LP_SHIFT                          1
#define MT6359_RG_LDO_VIO18_STBTD_ADDR                        \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VIO18_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VIO18_ULP_ADDR                          \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_ULP_MASK                          0x1
#define MT6359_RG_LDO_VIO18_ULP_SHIFT                         4
#define MT6359_RG_LDO_VIO18_OCFB_EN_ADDR                      \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VIO18_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VIO18_OC_MODE_ADDR                      \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VIO18_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VIO18_OC_TSEL_ADDR                      \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VIO18_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VIO18_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VIO18_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VIO18_OP_MODE_ADDR                      \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VIO18_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VIO18_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VIO18_CON0
#define MT6359_RG_LDO_VIO18_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VIO18_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VIO18_B_EN_ADDR                             \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_B_EN_MASK                             0x1
#define MT6359_DA_VIO18_B_EN_SHIFT                            0
#define MT6359_DA_VIO18_B_STB_ADDR                            \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_B_STB_MASK                            0x1
#define MT6359_DA_VIO18_B_STB_SHIFT                           1
#define MT6359_DA_VIO18_B_LP_ADDR                             \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_B_LP_MASK                             0x1
#define MT6359_DA_VIO18_B_LP_SHIFT                            2
#define MT6359_DA_VIO18_L_EN_ADDR                             \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_L_EN_MASK                             0x1
#define MT6359_DA_VIO18_L_EN_SHIFT                            3
#define MT6359_DA_VIO18_L_STB_ADDR                            \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_L_STB_MASK                            0x1
#define MT6359_DA_VIO18_L_STB_SHIFT                           4
#define MT6359_DA_VIO18_OCFB_EN_ADDR                          \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_OCFB_EN_MASK                          0x1
#define MT6359_DA_VIO18_OCFB_EN_SHIFT                         5
#define MT6359_DA_VIO18_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VIO18_MON
#define MT6359_DA_VIO18_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VIO18_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VIO18_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VIO18_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VIO18_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VIO18_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VIO18_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VIO18_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VIO18_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VIO18_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VIO18_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VIO18_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO18_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VIO18_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VIO18_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VIO18_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VIO18_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VIO18_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VIO18_SW_OP_EN_ADDR                     \
	MT6359_LDO_VIO18_OP_EN
#define MT6359_RG_LDO_VIO18_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIO18_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VIO18_OP_EN_SET_ADDR                    \
	MT6359_LDO_VIO18_OP_EN_SET
#define MT6359_RG_LDO_VIO18_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VIO18_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VIO18_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VIO18_OP_EN_CLR
#define MT6359_RG_LDO_VIO18_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VIO18_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VIO18_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VIO18_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VIO18_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VIO18_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VIO18_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VIO18_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VIO18_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VIO18_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VIO18_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VIO18_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO18_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VIO18_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO18_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VIO18_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO18_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VIO18_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO18_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VIO18_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO18_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VIO18_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO18_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VIO18_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VIO18_OP_CFG
#define MT6359_RG_LDO_VIO18_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIO18_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VIO18_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG_SET
#define MT6359_RG_LDO_VIO18_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VIO18_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VIO18_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VIO18_OP_CFG_CLR
#define MT6359_RG_LDO_VIO18_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VIO18_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VEMC_EN_ADDR                            \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_EN_MASK                            0x1
#define MT6359_RG_LDO_VEMC_EN_SHIFT                           0
#define MT6359_RG_LDO_VEMC_LP_ADDR                            \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_LP_MASK                            0x1
#define MT6359_RG_LDO_VEMC_LP_SHIFT                           1
#define MT6359_RG_LDO_VEMC_STBTD_ADDR                         \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VEMC_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VEMC_ULP_ADDR                           \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_ULP_MASK                           0x1
#define MT6359_RG_LDO_VEMC_ULP_SHIFT                          4
#define MT6359_RG_LDO_VEMC_OCFB_EN_ADDR                       \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VEMC_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VEMC_OC_MODE_ADDR                       \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VEMC_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VEMC_OC_TSEL_ADDR                       \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VEMC_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VEMC_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VEMC_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VEMC_OP_MODE_ADDR                       \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VEMC_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VEMC_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VEMC_CON0
#define MT6359_RG_LDO_VEMC_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VEMC_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VEMC_B_EN_ADDR                              \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_B_EN_MASK                              0x1
#define MT6359_DA_VEMC_B_EN_SHIFT                             0
#define MT6359_DA_VEMC_B_STB_ADDR                             \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_B_STB_MASK                             0x1
#define MT6359_DA_VEMC_B_STB_SHIFT                            1
#define MT6359_DA_VEMC_B_LP_ADDR                              \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_B_LP_MASK                              0x1
#define MT6359_DA_VEMC_B_LP_SHIFT                             2
#define MT6359_DA_VEMC_L_EN_ADDR                              \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_L_EN_MASK                              0x1
#define MT6359_DA_VEMC_L_EN_SHIFT                             3
#define MT6359_DA_VEMC_L_STB_ADDR                             \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_L_STB_MASK                             0x1
#define MT6359_DA_VEMC_L_STB_SHIFT                            4
#define MT6359_DA_VEMC_OCFB_EN_ADDR                           \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_OCFB_EN_MASK                           0x1
#define MT6359_DA_VEMC_OCFB_EN_SHIFT                          5
#define MT6359_DA_VEMC_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VEMC_MON
#define MT6359_DA_VEMC_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VEMC_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VEMC_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VEMC_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VEMC_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VEMC_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VEMC_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VEMC_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VEMC_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VEMC_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VEMC_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VEMC_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VEMC_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VEMC_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VEMC_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VEMC_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VEMC_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VEMC_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VEMC_SW_OP_EN_ADDR                      \
	MT6359_LDO_VEMC_OP_EN
#define MT6359_RG_LDO_VEMC_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VEMC_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VEMC_OP_EN_SET_ADDR                     \
	MT6359_LDO_VEMC_OP_EN_SET
#define MT6359_RG_LDO_VEMC_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VEMC_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VEMC_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VEMC_OP_EN_CLR
#define MT6359_RG_LDO_VEMC_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VEMC_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VEMC_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VEMC_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VEMC_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VEMC_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VEMC_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VEMC_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VEMC_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VEMC_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VEMC_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VEMC_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VEMC_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VEMC_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEMC_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VEMC_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEMC_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VEMC_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEMC_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VEMC_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEMC_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VEMC_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VEMC_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VEMC_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VEMC_OP_CFG
#define MT6359_RG_LDO_VEMC_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VEMC_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VEMC_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG_SET
#define MT6359_RG_LDO_VEMC_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VEMC_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VEMC_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VEMC_OP_CFG_CLR
#define MT6359_RG_LDO_VEMC_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VEMC_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_LDO_VSIM1_EN_ADDR                           \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_EN_MASK                           0x1
#define MT6359_RG_LDO_VSIM1_EN_SHIFT                          0
#define MT6359_RG_LDO_VSIM1_LP_ADDR                           \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_LP_MASK                           0x1
#define MT6359_RG_LDO_VSIM1_LP_SHIFT                          1
#define MT6359_RG_LDO_VSIM1_STBTD_ADDR                        \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VSIM1_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VSIM1_ULP_ADDR                          \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_ULP_MASK                          0x1
#define MT6359_RG_LDO_VSIM1_ULP_SHIFT                         4
#define MT6359_RG_LDO_VSIM1_OCFB_EN_ADDR                      \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VSIM1_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VSIM1_OC_MODE_ADDR                      \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VSIM1_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VSIM1_OC_TSEL_ADDR                      \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VSIM1_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VSIM1_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VSIM1_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VSIM1_OP_MODE_ADDR                      \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VSIM1_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VSIM1_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VSIM1_CON0
#define MT6359_RG_LDO_VSIM1_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VSIM1_B_EN_ADDR                             \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_B_EN_MASK                             0x1
#define MT6359_DA_VSIM1_B_EN_SHIFT                            0
#define MT6359_DA_VSIM1_B_STB_ADDR                            \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_B_STB_MASK                            0x1
#define MT6359_DA_VSIM1_B_STB_SHIFT                           1
#define MT6359_DA_VSIM1_B_LP_ADDR                             \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_B_LP_MASK                             0x1
#define MT6359_DA_VSIM1_B_LP_SHIFT                            2
#define MT6359_DA_VSIM1_L_EN_ADDR                             \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_L_EN_MASK                             0x1
#define MT6359_DA_VSIM1_L_EN_SHIFT                            3
#define MT6359_DA_VSIM1_L_STB_ADDR                            \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_L_STB_MASK                            0x1
#define MT6359_DA_VSIM1_L_STB_SHIFT                           4
#define MT6359_DA_VSIM1_OCFB_EN_ADDR                          \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_OCFB_EN_MASK                          0x1
#define MT6359_DA_VSIM1_OCFB_EN_SHIFT                         5
#define MT6359_DA_VSIM1_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VSIM1_MON
#define MT6359_DA_VSIM1_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VSIM1_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VSIM1_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VSIM1_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VSIM1_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VSIM1_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VSIM1_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VSIM1_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VSIM1_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VSIM1_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VSIM1_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VSIM1_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VSIM1_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VSIM1_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VSIM1_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VSIM1_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VSIM1_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VSIM1_SW_OP_EN_ADDR                     \
	MT6359_LDO_VSIM1_OP_EN
#define MT6359_RG_LDO_VSIM1_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VSIM1_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VSIM1_OP_EN_SET_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN_SET
#define MT6359_RG_LDO_VSIM1_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VSIM1_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VSIM1_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VSIM1_OP_EN_CLR
#define MT6359_RG_LDO_VSIM1_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VSIM1_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VSIM1_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VSIM1_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VSIM1_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VSIM1_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VSIM1_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VSIM1_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VSIM1_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VSIM1_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VSIM1_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VSIM1_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM1_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VSIM1_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM1_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VSIM1_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM1_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VSIM1_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM1_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VSIM1_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM1_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VSIM1_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM1_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VSIM1_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VSIM1_OP_CFG
#define MT6359_RG_LDO_VSIM1_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VSIM1_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VSIM1_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG_SET
#define MT6359_RG_LDO_VSIM1_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VSIM1_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VSIM1_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VSIM1_OP_CFG_CLR
#define MT6359_RG_LDO_VSIM1_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VSIM1_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VSIM2_EN_ADDR                           \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_EN_MASK                           0x1
#define MT6359_RG_LDO_VSIM2_EN_SHIFT                          0
#define MT6359_RG_LDO_VSIM2_LP_ADDR                           \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_LP_MASK                           0x1
#define MT6359_RG_LDO_VSIM2_LP_SHIFT                          1
#define MT6359_RG_LDO_VSIM2_STBTD_ADDR                        \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VSIM2_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VSIM2_ULP_ADDR                          \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_ULP_MASK                          0x1
#define MT6359_RG_LDO_VSIM2_ULP_SHIFT                         4
#define MT6359_RG_LDO_VSIM2_OCFB_EN_ADDR                      \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VSIM2_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VSIM2_OC_MODE_ADDR                      \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VSIM2_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VSIM2_OC_TSEL_ADDR                      \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VSIM2_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VSIM2_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VSIM2_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VSIM2_OP_MODE_ADDR                      \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VSIM2_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VSIM2_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VSIM2_CON0
#define MT6359_RG_LDO_VSIM2_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VSIM2_B_EN_ADDR                             \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_B_EN_MASK                             0x1
#define MT6359_DA_VSIM2_B_EN_SHIFT                            0
#define MT6359_DA_VSIM2_B_STB_ADDR                            \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_B_STB_MASK                            0x1
#define MT6359_DA_VSIM2_B_STB_SHIFT                           1
#define MT6359_DA_VSIM2_B_LP_ADDR                             \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_B_LP_MASK                             0x1
#define MT6359_DA_VSIM2_B_LP_SHIFT                            2
#define MT6359_DA_VSIM2_L_EN_ADDR                             \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_L_EN_MASK                             0x1
#define MT6359_DA_VSIM2_L_EN_SHIFT                            3
#define MT6359_DA_VSIM2_L_STB_ADDR                            \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_L_STB_MASK                            0x1
#define MT6359_DA_VSIM2_L_STB_SHIFT                           4
#define MT6359_DA_VSIM2_OCFB_EN_ADDR                          \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_OCFB_EN_MASK                          0x1
#define MT6359_DA_VSIM2_OCFB_EN_SHIFT                         5
#define MT6359_DA_VSIM2_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VSIM2_MON
#define MT6359_DA_VSIM2_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VSIM2_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VSIM2_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VSIM2_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VSIM2_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VSIM2_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VSIM2_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VSIM2_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VSIM2_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VSIM2_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VSIM2_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VSIM2_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VSIM2_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VSIM2_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VSIM2_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VSIM2_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VSIM2_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VSIM2_SW_OP_EN_ADDR                     \
	MT6359_LDO_VSIM2_OP_EN
#define MT6359_RG_LDO_VSIM2_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VSIM2_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VSIM2_OP_EN_SET_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN_SET
#define MT6359_RG_LDO_VSIM2_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VSIM2_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VSIM2_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VSIM2_OP_EN_CLR
#define MT6359_RG_LDO_VSIM2_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VSIM2_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VSIM2_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VSIM2_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VSIM2_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VSIM2_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VSIM2_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VSIM2_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VSIM2_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VSIM2_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VSIM2_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VSIM2_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VSIM2_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VSIM2_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM2_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VSIM2_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM2_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VSIM2_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM2_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VSIM2_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM2_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VSIM2_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VSIM2_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VSIM2_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VSIM2_OP_CFG
#define MT6359_RG_LDO_VSIM2_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VSIM2_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VSIM2_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG_SET
#define MT6359_RG_LDO_VSIM2_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VSIM2_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VSIM2_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VSIM2_OP_CFG_CLR
#define MT6359_RG_LDO_VSIM2_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VSIM2_OP_CFG_CLR_SHIFT                  0
#define MT6359_LDO_GNR3_ANA_ID_ADDR                           \
	MT6359_LDO_GNR3_DSN_ID
#define MT6359_LDO_GNR3_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR3_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR3_DIG_ID_ADDR                           \
	MT6359_LDO_GNR3_DSN_ID
#define MT6359_LDO_GNR3_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR3_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR3_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR3_DSN_REV0
#define MT6359_LDO_GNR3_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR3_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR3_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR3_DSN_REV0
#define MT6359_LDO_GNR3_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR3_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR3_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR3_DSN_REV0
#define MT6359_LDO_GNR3_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR3_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR3_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR3_DSN_REV0
#define MT6359_LDO_GNR3_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR3_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR3_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR3_DSN_DBI
#define MT6359_LDO_GNR3_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR3_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR3_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR3_DSN_DBI
#define MT6359_LDO_GNR3_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR3_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR3_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR3_DSN_DBI
#define MT6359_LDO_GNR3_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR3_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR3_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR3_DSN_DXI
#define MT6359_LDO_GNR3_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR3_DSN_FPI_SHIFT                         0
#define MT6359_RG_LDO_VUSB_EN_0_ADDR                          \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_EN_0_MASK                          0x1
#define MT6359_RG_LDO_VUSB_EN_0_SHIFT                         0
#define MT6359_RG_LDO_VUSB_LP_ADDR                            \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_LP_MASK                            0x1
#define MT6359_RG_LDO_VUSB_LP_SHIFT                           1
#define MT6359_RG_LDO_VUSB_STBTD_ADDR                         \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VUSB_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VUSB_ULP_ADDR                           \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_ULP_MASK                           0x1
#define MT6359_RG_LDO_VUSB_ULP_SHIFT                          4
#define MT6359_RG_LDO_VUSB_OCFB_EN_ADDR                       \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VUSB_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VUSB_OC_MODE_ADDR                       \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VUSB_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VUSB_OC_TSEL_ADDR                       \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VUSB_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VUSB_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VUSB_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VUSB_OP_MODE_ADDR                       \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VUSB_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VUSB_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VUSB_CON0
#define MT6359_RG_LDO_VUSB_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VUSB_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VUSB_B_EN_ADDR                              \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_B_EN_MASK                              0x1
#define MT6359_DA_VUSB_B_EN_SHIFT                             0
#define MT6359_DA_VUSB_B_STB_ADDR                             \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_B_STB_MASK                             0x1
#define MT6359_DA_VUSB_B_STB_SHIFT                            1
#define MT6359_DA_VUSB_B_LP_ADDR                              \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_B_LP_MASK                              0x1
#define MT6359_DA_VUSB_B_LP_SHIFT                             2
#define MT6359_DA_VUSB_L_EN_ADDR                              \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_L_EN_MASK                              0x1
#define MT6359_DA_VUSB_L_EN_SHIFT                             3
#define MT6359_DA_VUSB_L_STB_ADDR                             \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_L_STB_MASK                             0x1
#define MT6359_DA_VUSB_L_STB_SHIFT                            4
#define MT6359_DA_VUSB_OCFB_EN_ADDR                           \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_OCFB_EN_MASK                           0x1
#define MT6359_DA_VUSB_OCFB_EN_SHIFT                          5
#define MT6359_DA_VUSB_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VUSB_MON
#define MT6359_DA_VUSB_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VUSB_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VUSB_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VUSB_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VUSB_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VUSB_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VUSB_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VUSB_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VUSB_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VUSB_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VUSB_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VUSB_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUSB_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VUSB_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VUSB_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VUSB_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VUSB_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VUSB_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VUSB_SW_OP_EN_ADDR                      \
	MT6359_LDO_VUSB_OP_EN
#define MT6359_RG_LDO_VUSB_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VUSB_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VUSB_OP_EN_SET_ADDR                     \
	MT6359_LDO_VUSB_OP_EN_SET
#define MT6359_RG_LDO_VUSB_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VUSB_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VUSB_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VUSB_OP_EN_CLR
#define MT6359_RG_LDO_VUSB_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VUSB_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VUSB_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VUSB_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VUSB_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VUSB_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VUSB_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VUSB_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VUSB_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VUSB_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VUSB_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VUSB_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUSB_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VUSB_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUSB_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VUSB_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUSB_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VUSB_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUSB_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VUSB_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUSB_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VUSB_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUSB_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VUSB_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VUSB_OP_CFG
#define MT6359_RG_LDO_VUSB_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VUSB_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VUSB_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG_SET
#define MT6359_RG_LDO_VUSB_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VUSB_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VUSB_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VUSB_OP_CFG_CLR
#define MT6359_RG_LDO_VUSB_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VUSB_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_LDO_VUSB_EN_1_ADDR                          \
	MT6359_LDO_VUSB_MULTI_SW
#define MT6359_RG_LDO_VUSB_EN_1_MASK                          0x1
#define MT6359_RG_LDO_VUSB_EN_1_SHIFT                         15
#define MT6359_RG_LDO_VRFCK_EN_ADDR                           \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_EN_MASK                           0x1
#define MT6359_RG_LDO_VRFCK_EN_SHIFT                          0
#define MT6359_RG_LDO_VRFCK_LP_ADDR                           \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_LP_MASK                           0x1
#define MT6359_RG_LDO_VRFCK_LP_SHIFT                          1
#define MT6359_RG_LDO_VRFCK_STBTD_ADDR                        \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VRFCK_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VRFCK_ULP_ADDR                          \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_ULP_MASK                          0x1
#define MT6359_RG_LDO_VRFCK_ULP_SHIFT                         4
#define MT6359_RG_LDO_VRFCK_OCFB_EN_ADDR                      \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VRFCK_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VRFCK_OC_MODE_ADDR                      \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VRFCK_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VRFCK_OC_TSEL_ADDR                      \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VRFCK_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VRFCK_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VRFCK_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VRFCK_OP_MODE_ADDR                      \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VRFCK_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VRFCK_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VRFCK_CON0
#define MT6359_RG_LDO_VRFCK_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VRFCK_B_EN_ADDR                             \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_B_EN_MASK                             0x1
#define MT6359_DA_VRFCK_B_EN_SHIFT                            0
#define MT6359_DA_VRFCK_B_STB_ADDR                            \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_B_STB_MASK                            0x1
#define MT6359_DA_VRFCK_B_STB_SHIFT                           1
#define MT6359_DA_VRFCK_B_LP_ADDR                             \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_B_LP_MASK                             0x1
#define MT6359_DA_VRFCK_B_LP_SHIFT                            2
#define MT6359_DA_VRFCK_L_EN_ADDR                             \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_L_EN_MASK                             0x1
#define MT6359_DA_VRFCK_L_EN_SHIFT                            3
#define MT6359_DA_VRFCK_L_STB_ADDR                            \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_L_STB_MASK                            0x1
#define MT6359_DA_VRFCK_L_STB_SHIFT                           4
#define MT6359_DA_VRFCK_OCFB_EN_ADDR                          \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_OCFB_EN_MASK                          0x1
#define MT6359_DA_VRFCK_OCFB_EN_SHIFT                         5
#define MT6359_DA_VRFCK_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VRFCK_MON
#define MT6359_DA_VRFCK_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VRFCK_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VRFCK_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VRFCK_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VRFCK_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VRFCK_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VRFCK_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VRFCK_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VRFCK_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VRFCK_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VRFCK_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VRFCK_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VRFCK_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VRFCK_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VRFCK_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VRFCK_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VRFCK_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VRFCK_SW_OP_EN_ADDR                     \
	MT6359_LDO_VRFCK_OP_EN
#define MT6359_RG_LDO_VRFCK_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VRFCK_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VRFCK_OP_EN_SET_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN_SET
#define MT6359_RG_LDO_VRFCK_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VRFCK_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VRFCK_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VRFCK_OP_EN_CLR
#define MT6359_RG_LDO_VRFCK_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VRFCK_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VRFCK_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VRFCK_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VRFCK_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VRFCK_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VRFCK_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VRFCK_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VRFCK_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VRFCK_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VRFCK_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VRFCK_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VRFCK_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VRFCK_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRFCK_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VRFCK_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRFCK_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VRFCK_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRFCK_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VRFCK_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRFCK_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VRFCK_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VRFCK_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VRFCK_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VRFCK_OP_CFG
#define MT6359_RG_LDO_VRFCK_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VRFCK_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VRFCK_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG_SET
#define MT6359_RG_LDO_VRFCK_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VRFCK_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VRFCK_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VRFCK_OP_CFG_CLR
#define MT6359_RG_LDO_VRFCK_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VRFCK_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VBBCK_EN_ADDR                           \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_EN_MASK                           0x1
#define MT6359_RG_LDO_VBBCK_EN_SHIFT                          0
#define MT6359_RG_LDO_VBBCK_LP_ADDR                           \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_LP_MASK                           0x1
#define MT6359_RG_LDO_VBBCK_LP_SHIFT                          1
#define MT6359_RG_LDO_VBBCK_STBTD_ADDR                        \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VBBCK_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VBBCK_ULP_ADDR                          \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_ULP_MASK                          0x1
#define MT6359_RG_LDO_VBBCK_ULP_SHIFT                         4
#define MT6359_RG_LDO_VBBCK_OCFB_EN_ADDR                      \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VBBCK_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VBBCK_OC_MODE_ADDR                      \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VBBCK_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VBBCK_OC_TSEL_ADDR                      \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VBBCK_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VBBCK_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VBBCK_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VBBCK_OP_MODE_ADDR                      \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VBBCK_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VBBCK_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VBBCK_CON0
#define MT6359_RG_LDO_VBBCK_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VBBCK_B_EN_ADDR                             \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_B_EN_MASK                             0x1
#define MT6359_DA_VBBCK_B_EN_SHIFT                            0
#define MT6359_DA_VBBCK_B_STB_ADDR                            \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_B_STB_MASK                            0x1
#define MT6359_DA_VBBCK_B_STB_SHIFT                           1
#define MT6359_DA_VBBCK_B_LP_ADDR                             \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_B_LP_MASK                             0x1
#define MT6359_DA_VBBCK_B_LP_SHIFT                            2
#define MT6359_DA_VBBCK_L_EN_ADDR                             \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_L_EN_MASK                             0x1
#define MT6359_DA_VBBCK_L_EN_SHIFT                            3
#define MT6359_DA_VBBCK_L_STB_ADDR                            \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_L_STB_MASK                            0x1
#define MT6359_DA_VBBCK_L_STB_SHIFT                           4
#define MT6359_DA_VBBCK_OCFB_EN_ADDR                          \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_OCFB_EN_MASK                          0x1
#define MT6359_DA_VBBCK_OCFB_EN_SHIFT                         5
#define MT6359_DA_VBBCK_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VBBCK_MON
#define MT6359_DA_VBBCK_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VBBCK_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VBBCK_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VBBCK_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VBBCK_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VBBCK_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VBBCK_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VBBCK_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VBBCK_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VBBCK_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VBBCK_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VBBCK_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VBBCK_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VBBCK_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VBBCK_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VBBCK_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VBBCK_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VBBCK_SW_OP_EN_ADDR                     \
	MT6359_LDO_VBBCK_OP_EN
#define MT6359_RG_LDO_VBBCK_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VBBCK_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VBBCK_OP_EN_SET_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN_SET
#define MT6359_RG_LDO_VBBCK_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VBBCK_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VBBCK_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VBBCK_OP_EN_CLR
#define MT6359_RG_LDO_VBBCK_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VBBCK_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VBBCK_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VBBCK_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VBBCK_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VBBCK_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VBBCK_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VBBCK_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VBBCK_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VBBCK_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VBBCK_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VBBCK_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBBCK_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VBBCK_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBBCK_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VBBCK_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBBCK_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VBBCK_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBBCK_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VBBCK_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBBCK_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VBBCK_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBBCK_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VBBCK_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VBBCK_OP_CFG
#define MT6359_RG_LDO_VBBCK_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VBBCK_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VBBCK_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG_SET
#define MT6359_RG_LDO_VBBCK_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VBBCK_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VBBCK_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VBBCK_OP_CFG_CLR
#define MT6359_RG_LDO_VBBCK_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VBBCK_OP_CFG_CLR_SHIFT                  0
#define MT6359_RG_LDO_VBIF28_EN_ADDR                          \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_EN_MASK                          0x1
#define MT6359_RG_LDO_VBIF28_EN_SHIFT                         0
#define MT6359_RG_LDO_VBIF28_LP_ADDR                          \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_LP_MASK                          0x1
#define MT6359_RG_LDO_VBIF28_LP_SHIFT                         1
#define MT6359_RG_LDO_VBIF28_STBTD_ADDR                       \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_STBTD_MASK                       0x3
#define MT6359_RG_LDO_VBIF28_STBTD_SHIFT                      2
#define MT6359_RG_LDO_VBIF28_ULP_ADDR                         \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_ULP_MASK                         0x1
#define MT6359_RG_LDO_VBIF28_ULP_SHIFT                        4
#define MT6359_RG_LDO_VBIF28_OCFB_EN_ADDR                     \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_OCFB_EN_MASK                     0x1
#define MT6359_RG_LDO_VBIF28_OCFB_EN_SHIFT                    5
#define MT6359_RG_LDO_VBIF28_OC_MODE_ADDR                     \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_OC_MODE_MASK                     0x1
#define MT6359_RG_LDO_VBIF28_OC_MODE_SHIFT                    6
#define MT6359_RG_LDO_VBIF28_OC_TSEL_ADDR                     \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_OC_TSEL_MASK                     0x1
#define MT6359_RG_LDO_VBIF28_OC_TSEL_SHIFT                    7
#define MT6359_RG_LDO_VBIF28_DUMMY_LOAD_ADDR                  \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_DUMMY_LOAD_MASK                  0x3
#define MT6359_RG_LDO_VBIF28_DUMMY_LOAD_SHIFT                 8
#define MT6359_RG_LDO_VBIF28_OP_MODE_ADDR                     \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_OP_MODE_MASK                     0x7
#define MT6359_RG_LDO_VBIF28_OP_MODE_SHIFT                    10
#define MT6359_RG_LDO_VBIF28_CK_SW_MODE_ADDR                  \
	MT6359_LDO_VBIF28_CON0
#define MT6359_RG_LDO_VBIF28_CK_SW_MODE_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_CK_SW_MODE_SHIFT                 15
#define MT6359_DA_VBIF28_B_EN_ADDR                            \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_B_EN_MASK                            0x1
#define MT6359_DA_VBIF28_B_EN_SHIFT                           0
#define MT6359_DA_VBIF28_B_STB_ADDR                           \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_B_STB_MASK                           0x1
#define MT6359_DA_VBIF28_B_STB_SHIFT                          1
#define MT6359_DA_VBIF28_B_LP_ADDR                            \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_B_LP_MASK                            0x1
#define MT6359_DA_VBIF28_B_LP_SHIFT                           2
#define MT6359_DA_VBIF28_L_EN_ADDR                            \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_L_EN_MASK                            0x1
#define MT6359_DA_VBIF28_L_EN_SHIFT                           3
#define MT6359_DA_VBIF28_L_STB_ADDR                           \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_L_STB_MASK                           0x1
#define MT6359_DA_VBIF28_L_STB_SHIFT                          4
#define MT6359_DA_VBIF28_OCFB_EN_ADDR                         \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_OCFB_EN_MASK                         0x1
#define MT6359_DA_VBIF28_OCFB_EN_SHIFT                        5
#define MT6359_DA_VBIF28_DUMMY_LOAD_ADDR                      \
	MT6359_LDO_VBIF28_MON
#define MT6359_DA_VBIF28_DUMMY_LOAD_MASK                      0x3
#define MT6359_DA_VBIF28_DUMMY_LOAD_SHIFT                     6
#define MT6359_RG_LDO_VBIF28_HW0_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW0_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW0_OP_EN_SHIFT                  0
#define MT6359_RG_LDO_VBIF28_HW1_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW1_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW1_OP_EN_SHIFT                  1
#define MT6359_RG_LDO_VBIF28_HW2_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW2_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW2_OP_EN_SHIFT                  2
#define MT6359_RG_LDO_VBIF28_HW3_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW3_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW3_OP_EN_SHIFT                  3
#define MT6359_RG_LDO_VBIF28_HW4_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW4_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW4_OP_EN_SHIFT                  4
#define MT6359_RG_LDO_VBIF28_HW5_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW5_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW5_OP_EN_SHIFT                  5
#define MT6359_RG_LDO_VBIF28_HW6_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW6_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW6_OP_EN_SHIFT                  6
#define MT6359_RG_LDO_VBIF28_HW7_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW7_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW7_OP_EN_SHIFT                  7
#define MT6359_RG_LDO_VBIF28_HW8_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW8_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW8_OP_EN_SHIFT                  8
#define MT6359_RG_LDO_VBIF28_HW9_OP_EN_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW9_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_HW9_OP_EN_SHIFT                  9
#define MT6359_RG_LDO_VBIF28_HW10_OP_EN_ADDR                  \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW10_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW10_OP_EN_SHIFT                 10
#define MT6359_RG_LDO_VBIF28_HW11_OP_EN_ADDR                  \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW11_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW11_OP_EN_SHIFT                 11
#define MT6359_RG_LDO_VBIF28_HW12_OP_EN_ADDR                  \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW12_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW12_OP_EN_SHIFT                 12
#define MT6359_RG_LDO_VBIF28_HW13_OP_EN_ADDR                  \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW13_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW13_OP_EN_SHIFT                 13
#define MT6359_RG_LDO_VBIF28_HW14_OP_EN_ADDR                  \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_HW14_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW14_OP_EN_SHIFT                 14
#define MT6359_RG_LDO_VBIF28_SW_OP_EN_ADDR                    \
	MT6359_LDO_VBIF28_OP_EN
#define MT6359_RG_LDO_VBIF28_SW_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VBIF28_SW_OP_EN_SHIFT                   15
#define MT6359_RG_LDO_VBIF28_OP_EN_SET_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN_SET
#define MT6359_RG_LDO_VBIF28_OP_EN_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VBIF28_OP_EN_SET_SHIFT                  0
#define MT6359_RG_LDO_VBIF28_OP_EN_CLR_ADDR                   \
	MT6359_LDO_VBIF28_OP_EN_CLR
#define MT6359_RG_LDO_VBIF28_OP_EN_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VBIF28_OP_EN_CLR_SHIFT                  0
#define MT6359_RG_LDO_VBIF28_HW0_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW0_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW0_OP_CFG_SHIFT                 0
#define MT6359_RG_LDO_VBIF28_HW1_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW1_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW1_OP_CFG_SHIFT                 1
#define MT6359_RG_LDO_VBIF28_HW2_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW2_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW2_OP_CFG_SHIFT                 2
#define MT6359_RG_LDO_VBIF28_HW3_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW3_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW3_OP_CFG_SHIFT                 3
#define MT6359_RG_LDO_VBIF28_HW4_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW4_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW4_OP_CFG_SHIFT                 4
#define MT6359_RG_LDO_VBIF28_HW5_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW5_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW5_OP_CFG_SHIFT                 5
#define MT6359_RG_LDO_VBIF28_HW6_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW6_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW6_OP_CFG_SHIFT                 6
#define MT6359_RG_LDO_VBIF28_HW7_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW7_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW7_OP_CFG_SHIFT                 7
#define MT6359_RG_LDO_VBIF28_HW8_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW8_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW8_OP_CFG_SHIFT                 8
#define MT6359_RG_LDO_VBIF28_HW9_OP_CFG_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW9_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VBIF28_HW9_OP_CFG_SHIFT                 9
#define MT6359_RG_LDO_VBIF28_HW10_OP_CFG_ADDR                 \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW10_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VBIF28_HW10_OP_CFG_SHIFT                10
#define MT6359_RG_LDO_VBIF28_HW11_OP_CFG_ADDR                 \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW11_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VBIF28_HW11_OP_CFG_SHIFT                11
#define MT6359_RG_LDO_VBIF28_HW12_OP_CFG_ADDR                 \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW12_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VBIF28_HW12_OP_CFG_SHIFT                12
#define MT6359_RG_LDO_VBIF28_HW13_OP_CFG_ADDR                 \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW13_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VBIF28_HW13_OP_CFG_SHIFT                13
#define MT6359_RG_LDO_VBIF28_HW14_OP_CFG_ADDR                 \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_HW14_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VBIF28_HW14_OP_CFG_SHIFT                14
#define MT6359_RG_LDO_VBIF28_SW_OP_CFG_ADDR                   \
	MT6359_LDO_VBIF28_OP_CFG
#define MT6359_RG_LDO_VBIF28_SW_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VBIF28_SW_OP_CFG_SHIFT                  15
#define MT6359_RG_LDO_VBIF28_OP_CFG_SET_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG_SET
#define MT6359_RG_LDO_VBIF28_OP_CFG_SET_MASK                  0xFFFF
#define MT6359_RG_LDO_VBIF28_OP_CFG_SET_SHIFT                 0
#define MT6359_RG_LDO_VBIF28_OP_CFG_CLR_ADDR                  \
	MT6359_LDO_VBIF28_OP_CFG_CLR
#define MT6359_RG_LDO_VBIF28_OP_CFG_CLR_MASK                  0xFFFF
#define MT6359_RG_LDO_VBIF28_OP_CFG_CLR_SHIFT                 0
#define MT6359_RG_LDO_VIBR_EN_ADDR                            \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_EN_MASK                            0x1
#define MT6359_RG_LDO_VIBR_EN_SHIFT                           0
#define MT6359_RG_LDO_VIBR_LP_ADDR                            \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_LP_MASK                            0x1
#define MT6359_RG_LDO_VIBR_LP_SHIFT                           1
#define MT6359_RG_LDO_VIBR_STBTD_ADDR                         \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VIBR_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VIBR_ULP_ADDR                           \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_ULP_MASK                           0x1
#define MT6359_RG_LDO_VIBR_ULP_SHIFT                          4
#define MT6359_RG_LDO_VIBR_OCFB_EN_ADDR                       \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VIBR_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VIBR_OC_MODE_ADDR                       \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VIBR_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VIBR_OC_TSEL_ADDR                       \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VIBR_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VIBR_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VIBR_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VIBR_OP_MODE_ADDR                       \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VIBR_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VIBR_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VIBR_CON0
#define MT6359_RG_LDO_VIBR_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VIBR_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VIBR_B_EN_ADDR                              \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_B_EN_MASK                              0x1
#define MT6359_DA_VIBR_B_EN_SHIFT                             0
#define MT6359_DA_VIBR_B_STB_ADDR                             \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_B_STB_MASK                             0x1
#define MT6359_DA_VIBR_B_STB_SHIFT                            1
#define MT6359_DA_VIBR_B_LP_ADDR                              \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_B_LP_MASK                              0x1
#define MT6359_DA_VIBR_B_LP_SHIFT                             2
#define MT6359_DA_VIBR_L_EN_ADDR                              \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_L_EN_MASK                              0x1
#define MT6359_DA_VIBR_L_EN_SHIFT                             3
#define MT6359_DA_VIBR_L_STB_ADDR                             \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_L_STB_MASK                             0x1
#define MT6359_DA_VIBR_L_STB_SHIFT                            4
#define MT6359_DA_VIBR_OCFB_EN_ADDR                           \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_OCFB_EN_MASK                           0x1
#define MT6359_DA_VIBR_OCFB_EN_SHIFT                          5
#define MT6359_DA_VIBR_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VIBR_MON
#define MT6359_DA_VIBR_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VIBR_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VIBR_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VIBR_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VIBR_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VIBR_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VIBR_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VIBR_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VIBR_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VIBR_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VIBR_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VIBR_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIBR_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VIBR_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VIBR_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VIBR_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VIBR_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VIBR_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VIBR_SW_OP_EN_ADDR                      \
	MT6359_LDO_VIBR_OP_EN
#define MT6359_RG_LDO_VIBR_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VIBR_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VIBR_OP_EN_SET_ADDR                     \
	MT6359_LDO_VIBR_OP_EN_SET
#define MT6359_RG_LDO_VIBR_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VIBR_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VIBR_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VIBR_OP_EN_CLR
#define MT6359_RG_LDO_VIBR_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VIBR_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VIBR_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VIBR_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VIBR_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VIBR_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VIBR_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VIBR_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VIBR_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VIBR_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VIBR_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VIBR_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIBR_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VIBR_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIBR_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VIBR_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIBR_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VIBR_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIBR_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VIBR_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIBR_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VIBR_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIBR_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VIBR_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VIBR_OP_CFG
#define MT6359_RG_LDO_VIBR_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VIBR_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VIBR_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG_SET
#define MT6359_RG_LDO_VIBR_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VIBR_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VIBR_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VIBR_OP_CFG_CLR
#define MT6359_RG_LDO_VIBR_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VIBR_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_LDO_VIO28_EN_ADDR                           \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_EN_MASK                           0x1
#define MT6359_RG_LDO_VIO28_EN_SHIFT                          0
#define MT6359_RG_LDO_VIO28_LP_ADDR                           \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_LP_MASK                           0x1
#define MT6359_RG_LDO_VIO28_LP_SHIFT                          1
#define MT6359_RG_LDO_VIO28_STBTD_ADDR                        \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_STBTD_MASK                        0x3
#define MT6359_RG_LDO_VIO28_STBTD_SHIFT                       2
#define MT6359_RG_LDO_VIO28_ULP_ADDR                          \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_ULP_MASK                          0x1
#define MT6359_RG_LDO_VIO28_ULP_SHIFT                         4
#define MT6359_RG_LDO_VIO28_OCFB_EN_ADDR                      \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_OCFB_EN_MASK                      0x1
#define MT6359_RG_LDO_VIO28_OCFB_EN_SHIFT                     5
#define MT6359_RG_LDO_VIO28_OC_MODE_ADDR                      \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_OC_MODE_MASK                      0x1
#define MT6359_RG_LDO_VIO28_OC_MODE_SHIFT                     6
#define MT6359_RG_LDO_VIO28_OC_TSEL_ADDR                      \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_OC_TSEL_MASK                      0x1
#define MT6359_RG_LDO_VIO28_OC_TSEL_SHIFT                     7
#define MT6359_RG_LDO_VIO28_DUMMY_LOAD_ADDR                   \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_DUMMY_LOAD_MASK                   0x3
#define MT6359_RG_LDO_VIO28_DUMMY_LOAD_SHIFT                  8
#define MT6359_RG_LDO_VIO28_OP_MODE_ADDR                      \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_OP_MODE_MASK                      0x7
#define MT6359_RG_LDO_VIO28_OP_MODE_SHIFT                     10
#define MT6359_RG_LDO_VIO28_CK_SW_MODE_ADDR                   \
	MT6359_LDO_VIO28_CON0
#define MT6359_RG_LDO_VIO28_CK_SW_MODE_MASK                   0x1
#define MT6359_RG_LDO_VIO28_CK_SW_MODE_SHIFT                  15
#define MT6359_DA_VIO28_B_EN_ADDR                             \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_B_EN_MASK                             0x1
#define MT6359_DA_VIO28_B_EN_SHIFT                            0
#define MT6359_DA_VIO28_B_STB_ADDR                            \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_B_STB_MASK                            0x1
#define MT6359_DA_VIO28_B_STB_SHIFT                           1
#define MT6359_DA_VIO28_B_LP_ADDR                             \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_B_LP_MASK                             0x1
#define MT6359_DA_VIO28_B_LP_SHIFT                            2
#define MT6359_DA_VIO28_L_EN_ADDR                             \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_L_EN_MASK                             0x1
#define MT6359_DA_VIO28_L_EN_SHIFT                            3
#define MT6359_DA_VIO28_L_STB_ADDR                            \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_L_STB_MASK                            0x1
#define MT6359_DA_VIO28_L_STB_SHIFT                           4
#define MT6359_DA_VIO28_OCFB_EN_ADDR                          \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_OCFB_EN_MASK                          0x1
#define MT6359_DA_VIO28_OCFB_EN_SHIFT                         5
#define MT6359_DA_VIO28_DUMMY_LOAD_ADDR                       \
	MT6359_LDO_VIO28_MON
#define MT6359_DA_VIO28_DUMMY_LOAD_MASK                       0x3
#define MT6359_DA_VIO28_DUMMY_LOAD_SHIFT                      6
#define MT6359_RG_LDO_VIO28_HW0_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW0_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW0_OP_EN_SHIFT                   0
#define MT6359_RG_LDO_VIO28_HW1_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW1_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW1_OP_EN_SHIFT                   1
#define MT6359_RG_LDO_VIO28_HW2_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW2_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW2_OP_EN_SHIFT                   2
#define MT6359_RG_LDO_VIO28_HW3_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW3_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW3_OP_EN_SHIFT                   3
#define MT6359_RG_LDO_VIO28_HW4_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW4_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW4_OP_EN_SHIFT                   4
#define MT6359_RG_LDO_VIO28_HW5_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW5_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW5_OP_EN_SHIFT                   5
#define MT6359_RG_LDO_VIO28_HW6_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW6_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW6_OP_EN_SHIFT                   6
#define MT6359_RG_LDO_VIO28_HW7_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW7_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW7_OP_EN_SHIFT                   7
#define MT6359_RG_LDO_VIO28_HW8_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW8_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW8_OP_EN_SHIFT                   8
#define MT6359_RG_LDO_VIO28_HW9_OP_EN_ADDR                    \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW9_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VIO28_HW9_OP_EN_SHIFT                   9
#define MT6359_RG_LDO_VIO28_HW10_OP_EN_ADDR                   \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW10_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW10_OP_EN_SHIFT                  10
#define MT6359_RG_LDO_VIO28_HW11_OP_EN_ADDR                   \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW11_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW11_OP_EN_SHIFT                  11
#define MT6359_RG_LDO_VIO28_HW12_OP_EN_ADDR                   \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW12_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW12_OP_EN_SHIFT                  12
#define MT6359_RG_LDO_VIO28_HW13_OP_EN_ADDR                   \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW13_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW13_OP_EN_SHIFT                  13
#define MT6359_RG_LDO_VIO28_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_HW14_OP_EN_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW14_OP_EN_SHIFT                  14
#define MT6359_RG_LDO_VIO28_SW_OP_EN_ADDR                     \
	MT6359_LDO_VIO28_OP_EN
#define MT6359_RG_LDO_VIO28_SW_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VIO28_SW_OP_EN_SHIFT                    15
#define MT6359_RG_LDO_VIO28_OP_EN_SET_ADDR                    \
	MT6359_LDO_VIO28_OP_EN_SET
#define MT6359_RG_LDO_VIO28_OP_EN_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VIO28_OP_EN_SET_SHIFT                   0
#define MT6359_RG_LDO_VIO28_OP_EN_CLR_ADDR                    \
	MT6359_LDO_VIO28_OP_EN_CLR
#define MT6359_RG_LDO_VIO28_OP_EN_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VIO28_OP_EN_CLR_SHIFT                   0
#define MT6359_RG_LDO_VIO28_HW0_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW0_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW0_OP_CFG_SHIFT                  0
#define MT6359_RG_LDO_VIO28_HW1_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW1_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW1_OP_CFG_SHIFT                  1
#define MT6359_RG_LDO_VIO28_HW2_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW2_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW2_OP_CFG_SHIFT                  2
#define MT6359_RG_LDO_VIO28_HW3_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW3_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW3_OP_CFG_SHIFT                  3
#define MT6359_RG_LDO_VIO28_HW4_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW4_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW4_OP_CFG_SHIFT                  4
#define MT6359_RG_LDO_VIO28_HW5_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW5_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW5_OP_CFG_SHIFT                  5
#define MT6359_RG_LDO_VIO28_HW6_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW6_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW6_OP_CFG_SHIFT                  6
#define MT6359_RG_LDO_VIO28_HW7_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW7_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW7_OP_CFG_SHIFT                  7
#define MT6359_RG_LDO_VIO28_HW8_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW8_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW8_OP_CFG_SHIFT                  8
#define MT6359_RG_LDO_VIO28_HW9_OP_CFG_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW9_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VIO28_HW9_OP_CFG_SHIFT                  9
#define MT6359_RG_LDO_VIO28_HW10_OP_CFG_ADDR                  \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW10_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO28_HW10_OP_CFG_SHIFT                 10
#define MT6359_RG_LDO_VIO28_HW11_OP_CFG_ADDR                  \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW11_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO28_HW11_OP_CFG_SHIFT                 11
#define MT6359_RG_LDO_VIO28_HW12_OP_CFG_ADDR                  \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW12_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO28_HW12_OP_CFG_SHIFT                 12
#define MT6359_RG_LDO_VIO28_HW13_OP_CFG_ADDR                  \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW13_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO28_HW13_OP_CFG_SHIFT                 13
#define MT6359_RG_LDO_VIO28_HW14_OP_CFG_ADDR                  \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_HW14_OP_CFG_MASK                  0x1
#define MT6359_RG_LDO_VIO28_HW14_OP_CFG_SHIFT                 14
#define MT6359_RG_LDO_VIO28_SW_OP_CFG_ADDR                    \
	MT6359_LDO_VIO28_OP_CFG
#define MT6359_RG_LDO_VIO28_SW_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VIO28_SW_OP_CFG_SHIFT                   15
#define MT6359_RG_LDO_VIO28_OP_CFG_SET_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG_SET
#define MT6359_RG_LDO_VIO28_OP_CFG_SET_MASK                   0xFFFF
#define MT6359_RG_LDO_VIO28_OP_CFG_SET_SHIFT                  0
#define MT6359_RG_LDO_VIO28_OP_CFG_CLR_ADDR                   \
	MT6359_LDO_VIO28_OP_CFG_CLR
#define MT6359_RG_LDO_VIO28_OP_CFG_CLR_MASK                   0xFFFF
#define MT6359_RG_LDO_VIO28_OP_CFG_CLR_SHIFT                  0
#define MT6359_LDO_GNR4_ANA_ID_ADDR                           \
	MT6359_LDO_GNR4_DSN_ID
#define MT6359_LDO_GNR4_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR4_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR4_DIG_ID_ADDR                           \
	MT6359_LDO_GNR4_DSN_ID
#define MT6359_LDO_GNR4_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR4_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR4_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR4_DSN_REV0
#define MT6359_LDO_GNR4_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR4_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR4_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR4_DSN_REV0
#define MT6359_LDO_GNR4_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR4_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR4_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR4_DSN_REV0
#define MT6359_LDO_GNR4_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR4_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR4_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR4_DSN_REV0
#define MT6359_LDO_GNR4_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR4_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR4_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR4_DSN_DBI
#define MT6359_LDO_GNR4_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR4_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR4_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR4_DSN_DBI
#define MT6359_LDO_GNR4_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR4_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR4_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR4_DSN_DBI
#define MT6359_LDO_GNR4_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR4_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR4_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR4_DSN_DXI
#define MT6359_LDO_GNR4_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR4_DSN_FPI_SHIFT                         0
#define MT6359_RG_LDO_VM18_EN_ADDR                            \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_EN_MASK                            0x1
#define MT6359_RG_LDO_VM18_EN_SHIFT                           0
#define MT6359_RG_LDO_VM18_LP_ADDR                            \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_LP_MASK                            0x1
#define MT6359_RG_LDO_VM18_LP_SHIFT                           1
#define MT6359_RG_LDO_VM18_STBTD_ADDR                         \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VM18_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VM18_ULP_ADDR                           \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_ULP_MASK                           0x1
#define MT6359_RG_LDO_VM18_ULP_SHIFT                          4
#define MT6359_RG_LDO_VM18_OCFB_EN_ADDR                       \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VM18_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VM18_OC_MODE_ADDR                       \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VM18_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VM18_OC_TSEL_ADDR                       \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VM18_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VM18_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VM18_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VM18_OP_MODE_ADDR                       \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VM18_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VM18_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VM18_CON0
#define MT6359_RG_LDO_VM18_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VM18_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VM18_B_EN_ADDR                              \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_B_EN_MASK                              0x1
#define MT6359_DA_VM18_B_EN_SHIFT                             0
#define MT6359_DA_VM18_B_STB_ADDR                             \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_B_STB_MASK                             0x1
#define MT6359_DA_VM18_B_STB_SHIFT                            1
#define MT6359_DA_VM18_B_LP_ADDR                              \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_B_LP_MASK                              0x1
#define MT6359_DA_VM18_B_LP_SHIFT                             2
#define MT6359_DA_VM18_L_EN_ADDR                              \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_L_EN_MASK                              0x1
#define MT6359_DA_VM18_L_EN_SHIFT                             3
#define MT6359_DA_VM18_L_STB_ADDR                             \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_L_STB_MASK                             0x1
#define MT6359_DA_VM18_L_STB_SHIFT                            4
#define MT6359_DA_VM18_OCFB_EN_ADDR                           \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_OCFB_EN_MASK                           0x1
#define MT6359_DA_VM18_OCFB_EN_SHIFT                          5
#define MT6359_DA_VM18_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VM18_MON
#define MT6359_DA_VM18_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VM18_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VM18_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VM18_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VM18_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VM18_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VM18_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VM18_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VM18_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VM18_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VM18_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VM18_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VM18_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VM18_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VM18_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VM18_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VM18_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VM18_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VM18_SW_OP_EN_ADDR                      \
	MT6359_LDO_VM18_OP_EN
#define MT6359_RG_LDO_VM18_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VM18_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VM18_OP_EN_SET_ADDR                     \
	MT6359_LDO_VM18_OP_EN_SET
#define MT6359_RG_LDO_VM18_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VM18_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VM18_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VM18_OP_EN_CLR
#define MT6359_RG_LDO_VM18_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VM18_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VM18_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VM18_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VM18_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VM18_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VM18_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VM18_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VM18_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VM18_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VM18_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VM18_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VM18_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VM18_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VM18_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VM18_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VM18_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VM18_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VM18_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VM18_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VM18_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VM18_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VM18_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VM18_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VM18_OP_CFG
#define MT6359_RG_LDO_VM18_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VM18_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VM18_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VM18_OP_CFG_SET
#define MT6359_RG_LDO_VM18_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VM18_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VM18_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VM18_OP_CFG_CLR
#define MT6359_RG_LDO_VM18_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VM18_OP_CFG_CLR_SHIFT                   0
#define MT6359_RG_LDO_VUFS_EN_ADDR                            \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_EN_MASK                            0x1
#define MT6359_RG_LDO_VUFS_EN_SHIFT                           0
#define MT6359_RG_LDO_VUFS_LP_ADDR                            \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_LP_MASK                            0x1
#define MT6359_RG_LDO_VUFS_LP_SHIFT                           1
#define MT6359_RG_LDO_VUFS_STBTD_ADDR                         \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_STBTD_MASK                         0x3
#define MT6359_RG_LDO_VUFS_STBTD_SHIFT                        2
#define MT6359_RG_LDO_VUFS_ULP_ADDR                           \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_ULP_MASK                           0x1
#define MT6359_RG_LDO_VUFS_ULP_SHIFT                          4
#define MT6359_RG_LDO_VUFS_OCFB_EN_ADDR                       \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_OCFB_EN_MASK                       0x1
#define MT6359_RG_LDO_VUFS_OCFB_EN_SHIFT                      5
#define MT6359_RG_LDO_VUFS_OC_MODE_ADDR                       \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_OC_MODE_MASK                       0x1
#define MT6359_RG_LDO_VUFS_OC_MODE_SHIFT                      6
#define MT6359_RG_LDO_VUFS_OC_TSEL_ADDR                       \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_OC_TSEL_MASK                       0x1
#define MT6359_RG_LDO_VUFS_OC_TSEL_SHIFT                      7
#define MT6359_RG_LDO_VUFS_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_DUMMY_LOAD_MASK                    0x3
#define MT6359_RG_LDO_VUFS_DUMMY_LOAD_SHIFT                   8
#define MT6359_RG_LDO_VUFS_OP_MODE_ADDR                       \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_OP_MODE_MASK                       0x7
#define MT6359_RG_LDO_VUFS_OP_MODE_SHIFT                      10
#define MT6359_RG_LDO_VUFS_CK_SW_MODE_ADDR                    \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_CK_SW_MODE_MASK                    0x1
#define MT6359_RG_LDO_VUFS_CK_SW_MODE_SHIFT                   15
#define MT6359_DA_VUFS_B_EN_ADDR                              \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_B_EN_MASK                              0x1
#define MT6359_DA_VUFS_B_EN_SHIFT                             0
#define MT6359_DA_VUFS_B_STB_ADDR                             \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_B_STB_MASK                             0x1
#define MT6359_DA_VUFS_B_STB_SHIFT                            1
#define MT6359_DA_VUFS_B_LP_ADDR                              \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_B_LP_MASK                              0x1
#define MT6359_DA_VUFS_B_LP_SHIFT                             2
#define MT6359_DA_VUFS_L_EN_ADDR                              \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_L_EN_MASK                              0x1
#define MT6359_DA_VUFS_L_EN_SHIFT                             3
#define MT6359_DA_VUFS_L_STB_ADDR                             \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_L_STB_MASK                             0x1
#define MT6359_DA_VUFS_L_STB_SHIFT                            4
#define MT6359_DA_VUFS_OCFB_EN_ADDR                           \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_OCFB_EN_MASK                           0x1
#define MT6359_DA_VUFS_OCFB_EN_SHIFT                          5
#define MT6359_DA_VUFS_DUMMY_LOAD_ADDR                        \
	MT6359_LDO_VUFS_MON
#define MT6359_DA_VUFS_DUMMY_LOAD_MASK                        0x3
#define MT6359_DA_VUFS_DUMMY_LOAD_SHIFT                       6
#define MT6359_RG_LDO_VUFS_HW0_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW0_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW0_OP_EN_SHIFT                    0
#define MT6359_RG_LDO_VUFS_HW1_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW1_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW1_OP_EN_SHIFT                    1
#define MT6359_RG_LDO_VUFS_HW2_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW2_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW2_OP_EN_SHIFT                    2
#define MT6359_RG_LDO_VUFS_HW3_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW3_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW3_OP_EN_SHIFT                    3
#define MT6359_RG_LDO_VUFS_HW4_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW4_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW4_OP_EN_SHIFT                    4
#define MT6359_RG_LDO_VUFS_HW5_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW5_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW5_OP_EN_SHIFT                    5
#define MT6359_RG_LDO_VUFS_HW6_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW6_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW6_OP_EN_SHIFT                    6
#define MT6359_RG_LDO_VUFS_HW7_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW7_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW7_OP_EN_SHIFT                    7
#define MT6359_RG_LDO_VUFS_HW8_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW8_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW8_OP_EN_SHIFT                    8
#define MT6359_RG_LDO_VUFS_HW9_OP_EN_ADDR                     \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW9_OP_EN_MASK                     0x1
#define MT6359_RG_LDO_VUFS_HW9_OP_EN_SHIFT                    9
#define MT6359_RG_LDO_VUFS_HW10_OP_EN_ADDR                    \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW10_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW10_OP_EN_SHIFT                   10
#define MT6359_RG_LDO_VUFS_HW11_OP_EN_ADDR                    \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW11_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW11_OP_EN_SHIFT                   11
#define MT6359_RG_LDO_VUFS_HW12_OP_EN_ADDR                    \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW12_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW12_OP_EN_SHIFT                   12
#define MT6359_RG_LDO_VUFS_HW13_OP_EN_ADDR                    \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW13_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW13_OP_EN_SHIFT                   13
#define MT6359_RG_LDO_VUFS_HW14_OP_EN_ADDR                    \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_HW14_OP_EN_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW14_OP_EN_SHIFT                   14
#define MT6359_RG_LDO_VUFS_SW_OP_EN_ADDR                      \
	MT6359_LDO_VUFS_OP_EN
#define MT6359_RG_LDO_VUFS_SW_OP_EN_MASK                      0x1
#define MT6359_RG_LDO_VUFS_SW_OP_EN_SHIFT                     15
#define MT6359_RG_LDO_VUFS_OP_EN_SET_ADDR                     \
	MT6359_LDO_VUFS_OP_EN_SET
#define MT6359_RG_LDO_VUFS_OP_EN_SET_MASK                     0xFFFF
#define MT6359_RG_LDO_VUFS_OP_EN_SET_SHIFT                    0
#define MT6359_RG_LDO_VUFS_OP_EN_CLR_ADDR                     \
	MT6359_LDO_VUFS_OP_EN_CLR
#define MT6359_RG_LDO_VUFS_OP_EN_CLR_MASK                     0xFFFF
#define MT6359_RG_LDO_VUFS_OP_EN_CLR_SHIFT                    0
#define MT6359_RG_LDO_VUFS_HW0_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW0_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW0_OP_CFG_SHIFT                   0
#define MT6359_RG_LDO_VUFS_HW1_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW1_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW1_OP_CFG_SHIFT                   1
#define MT6359_RG_LDO_VUFS_HW2_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW2_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW2_OP_CFG_SHIFT                   2
#define MT6359_RG_LDO_VUFS_HW3_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW3_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW3_OP_CFG_SHIFT                   3
#define MT6359_RG_LDO_VUFS_HW4_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW4_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW4_OP_CFG_SHIFT                   4
#define MT6359_RG_LDO_VUFS_HW5_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW5_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW5_OP_CFG_SHIFT                   5
#define MT6359_RG_LDO_VUFS_HW6_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW6_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW6_OP_CFG_SHIFT                   6
#define MT6359_RG_LDO_VUFS_HW7_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW7_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW7_OP_CFG_SHIFT                   7
#define MT6359_RG_LDO_VUFS_HW8_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW8_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW8_OP_CFG_SHIFT                   8
#define MT6359_RG_LDO_VUFS_HW9_OP_CFG_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW9_OP_CFG_MASK                    0x1
#define MT6359_RG_LDO_VUFS_HW9_OP_CFG_SHIFT                   9
#define MT6359_RG_LDO_VUFS_HW10_OP_CFG_ADDR                   \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW10_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUFS_HW10_OP_CFG_SHIFT                  10
#define MT6359_RG_LDO_VUFS_HW11_OP_CFG_ADDR                   \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW11_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUFS_HW11_OP_CFG_SHIFT                  11
#define MT6359_RG_LDO_VUFS_HW12_OP_CFG_ADDR                   \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW12_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUFS_HW12_OP_CFG_SHIFT                  12
#define MT6359_RG_LDO_VUFS_HW13_OP_CFG_ADDR                   \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW13_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUFS_HW13_OP_CFG_SHIFT                  13
#define MT6359_RG_LDO_VUFS_HW14_OP_CFG_ADDR                   \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_HW14_OP_CFG_MASK                   0x1
#define MT6359_RG_LDO_VUFS_HW14_OP_CFG_SHIFT                  14
#define MT6359_RG_LDO_VUFS_SW_OP_CFG_ADDR                     \
	MT6359_LDO_VUFS_OP_CFG
#define MT6359_RG_LDO_VUFS_SW_OP_CFG_MASK                     0x1
#define MT6359_RG_LDO_VUFS_SW_OP_CFG_SHIFT                    15
#define MT6359_RG_LDO_VUFS_OP_CFG_SET_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG_SET
#define MT6359_RG_LDO_VUFS_OP_CFG_SET_MASK                    0xFFFF
#define MT6359_RG_LDO_VUFS_OP_CFG_SET_SHIFT                   0
#define MT6359_RG_LDO_VUFS_OP_CFG_CLR_ADDR                    \
	MT6359_LDO_VUFS_OP_CFG_CLR
#define MT6359_RG_LDO_VUFS_OP_CFG_CLR_MASK                    0xFFFF
#define MT6359_RG_LDO_VUFS_OP_CFG_CLR_SHIFT                   0
#define MT6359_LDO_GNR5_ANA_ID_ADDR                           \
	MT6359_LDO_GNR5_DSN_ID
#define MT6359_LDO_GNR5_ANA_ID_MASK                           0xFF
#define MT6359_LDO_GNR5_ANA_ID_SHIFT                          0
#define MT6359_LDO_GNR5_DIG_ID_ADDR                           \
	MT6359_LDO_GNR5_DSN_ID
#define MT6359_LDO_GNR5_DIG_ID_MASK                           0xFF
#define MT6359_LDO_GNR5_DIG_ID_SHIFT                          8
#define MT6359_LDO_GNR5_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR5_DSN_REV0
#define MT6359_LDO_GNR5_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR5_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_GNR5_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR5_DSN_REV0
#define MT6359_LDO_GNR5_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR5_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_GNR5_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_GNR5_DSN_REV0
#define MT6359_LDO_GNR5_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_GNR5_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_GNR5_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_GNR5_DSN_REV0
#define MT6359_LDO_GNR5_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_GNR5_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_GNR5_DSN_CBS_ADDR                          \
	MT6359_LDO_GNR5_DSN_DBI
#define MT6359_LDO_GNR5_DSN_CBS_MASK                          0x3
#define MT6359_LDO_GNR5_DSN_CBS_SHIFT                         0
#define MT6359_LDO_GNR5_DSN_BIX_ADDR                          \
	MT6359_LDO_GNR5_DSN_DBI
#define MT6359_LDO_GNR5_DSN_BIX_MASK                          0x3
#define MT6359_LDO_GNR5_DSN_BIX_SHIFT                         2
#define MT6359_LDO_GNR5_DSN_ESP_ADDR                          \
	MT6359_LDO_GNR5_DSN_DBI
#define MT6359_LDO_GNR5_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_GNR5_DSN_ESP_SHIFT                         8
#define MT6359_LDO_GNR5_DSN_FPI_ADDR                          \
	MT6359_LDO_GNR5_DSN_DXI
#define MT6359_LDO_GNR5_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_GNR5_DSN_FPI_SHIFT                         0
#define MT6359_LDO_VSRAM0_ANA_ID_ADDR                         \
	MT6359_LDO_VSRAM0_DSN_ID
#define MT6359_LDO_VSRAM0_ANA_ID_MASK                         0xFF
#define MT6359_LDO_VSRAM0_ANA_ID_SHIFT                        0
#define MT6359_LDO_VSRAM0_DIG_ID_ADDR                         \
	MT6359_LDO_VSRAM0_DSN_ID
#define MT6359_LDO_VSRAM0_DIG_ID_MASK                         0xFF
#define MT6359_LDO_VSRAM0_DIG_ID_SHIFT                        8
#define MT6359_LDO_VSRAM0_ANA_MINOR_REV_ADDR                  \
	MT6359_LDO_VSRAM0_DSN_REV0
#define MT6359_LDO_VSRAM0_ANA_MINOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM0_ANA_MINOR_REV_SHIFT                 0
#define MT6359_LDO_VSRAM0_ANA_MAJOR_REV_ADDR                  \
	MT6359_LDO_VSRAM0_DSN_REV0
#define MT6359_LDO_VSRAM0_ANA_MAJOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM0_ANA_MAJOR_REV_SHIFT                 4
#define MT6359_LDO_VSRAM0_DIG_MINOR_REV_ADDR                  \
	MT6359_LDO_VSRAM0_DSN_REV0
#define MT6359_LDO_VSRAM0_DIG_MINOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM0_DIG_MINOR_REV_SHIFT                 8
#define MT6359_LDO_VSRAM0_DIG_MAJOR_REV_ADDR                  \
	MT6359_LDO_VSRAM0_DSN_REV0
#define MT6359_LDO_VSRAM0_DIG_MAJOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM0_DIG_MAJOR_REV_SHIFT                 12
#define MT6359_LDO_VSRAM0_DSN_CBS_ADDR                        \
	MT6359_LDO_VSRAM0_DSN_DBI
#define MT6359_LDO_VSRAM0_DSN_CBS_MASK                        0x3
#define MT6359_LDO_VSRAM0_DSN_CBS_SHIFT                       0
#define MT6359_LDO_VSRAM0_DSN_BIX_ADDR                        \
	MT6359_LDO_VSRAM0_DSN_DBI
#define MT6359_LDO_VSRAM0_DSN_BIX_MASK                        0x3
#define MT6359_LDO_VSRAM0_DSN_BIX_SHIFT                       2
#define MT6359_LDO_VSRAM0_DSN_ESP_ADDR                        \
	MT6359_LDO_VSRAM0_DSN_DBI
#define MT6359_LDO_VSRAM0_DSN_ESP_MASK                        0xFF
#define MT6359_LDO_VSRAM0_DSN_ESP_SHIFT                       8
#define MT6359_LDO_VSRAM0_DSN_FPI_ADDR                        \
	MT6359_LDO_VSRAM0_DSN_DXI
#define MT6359_LDO_VSRAM0_DSN_FPI_MASK                        0xFF
#define MT6359_LDO_VSRAM0_DSN_FPI_SHIFT                       0
#define MT6359_RG_LDO_VSRAM_PROC1_EN_ADDR                     \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_EN_MASK                     0x1
#define MT6359_RG_LDO_VSRAM_PROC1_EN_SHIFT                    0
#define MT6359_RG_LDO_VSRAM_PROC1_LP_ADDR                     \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_LP_MASK                     0x1
#define MT6359_RG_LDO_VSRAM_PROC1_LP_SHIFT                    1
#define MT6359_RG_LDO_VSRAM_PROC1_STBTD_ADDR                  \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_STBTD_MASK                  0x3
#define MT6359_RG_LDO_VSRAM_PROC1_STBTD_SHIFT                 2
#define MT6359_RG_LDO_VSRAM_PROC1_ULP_ADDR                    \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_ULP_MASK                    0x1
#define MT6359_RG_LDO_VSRAM_PROC1_ULP_SHIFT                   4
#define MT6359_RG_LDO_VSRAM_PROC1_OCFB_EN_ADDR                \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_OCFB_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC1_OCFB_EN_SHIFT               5
#define MT6359_RG_LDO_VSRAM_PROC1_OC_MODE_ADDR                \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_OC_MODE_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC1_OC_MODE_SHIFT               6
#define MT6359_RG_LDO_VSRAM_PROC1_OC_TSEL_ADDR                \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_OC_TSEL_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC1_OC_TSEL_SHIFT               7
#define MT6359_RG_LDO_VSRAM_PROC1_DUMMY_LOAD_ADDR             \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_DUMMY_LOAD_MASK             0x3
#define MT6359_RG_LDO_VSRAM_PROC1_DUMMY_LOAD_SHIFT            8
#define MT6359_RG_LDO_VSRAM_PROC1_OP_MODE_ADDR                \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_OP_MODE_MASK                0x7
#define MT6359_RG_LDO_VSRAM_PROC1_OP_MODE_SHIFT               10
#define MT6359_RG_LDO_VSRAM_PROC1_R2R_PDN_DIS_ADDR            \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_R2R_PDN_DIS_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_R2R_PDN_DIS_SHIFT           14
#define MT6359_RG_LDO_VSRAM_PROC1_CK_SW_MODE_ADDR             \
	MT6359_LDO_VSRAM_PROC1_CON0
#define MT6359_RG_LDO_VSRAM_PROC1_CK_SW_MODE_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_CK_SW_MODE_SHIFT            15
#define MT6359_DA_VSRAM_PROC1_B_EN_ADDR                       \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_B_EN_MASK                       0x1
#define MT6359_DA_VSRAM_PROC1_B_EN_SHIFT                      0
#define MT6359_DA_VSRAM_PROC1_B_STB_ADDR                      \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_B_STB_MASK                      0x1
#define MT6359_DA_VSRAM_PROC1_B_STB_SHIFT                     1
#define MT6359_DA_VSRAM_PROC1_B_LP_ADDR                       \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_B_LP_MASK                       0x1
#define MT6359_DA_VSRAM_PROC1_B_LP_SHIFT                      2
#define MT6359_DA_VSRAM_PROC1_L_EN_ADDR                       \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_L_EN_MASK                       0x1
#define MT6359_DA_VSRAM_PROC1_L_EN_SHIFT                      3
#define MT6359_DA_VSRAM_PROC1_L_STB_ADDR                      \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_L_STB_MASK                      0x1
#define MT6359_DA_VSRAM_PROC1_L_STB_SHIFT                     4
#define MT6359_DA_VSRAM_PROC1_OCFB_EN_ADDR                    \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_OCFB_EN_MASK                    0x1
#define MT6359_DA_VSRAM_PROC1_OCFB_EN_SHIFT                   5
#define MT6359_DA_VSRAM_PROC1_DUMMY_LOAD_ADDR                 \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_DUMMY_LOAD_MASK                 0x3
#define MT6359_DA_VSRAM_PROC1_DUMMY_LOAD_SHIFT                6
#define MT6359_DA_VSRAM_PROC1_VSLEEP_SEL_ADDR                 \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_VSLEEP_SEL_MASK                 0x1
#define MT6359_DA_VSRAM_PROC1_VSLEEP_SEL_SHIFT                8
#define MT6359_DA_VSRAM_PROC1_R2R_PDN_ADDR                    \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_R2R_PDN_MASK                    0x1
#define MT6359_DA_VSRAM_PROC1_R2R_PDN_SHIFT                   9
#define MT6359_DA_VSRAM_PROC1_TRACK_NDIS_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_MON
#define MT6359_DA_VSRAM_PROC1_TRACK_NDIS_EN_MASK              0x1
#define MT6359_DA_VSRAM_PROC1_TRACK_NDIS_EN_SHIFT             10
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_SLEEP_ADDR            \
	MT6359_LDO_VSRAM_PROC1_VOSEL0
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_SLEEP_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_SLEEP_SHIFT           0
#define MT6359_LDO_VSRAM_PROC1_WDTDBG_VOSEL_ADDR              \
	MT6359_LDO_VSRAM_PROC1_VOSEL0
#define MT6359_LDO_VSRAM_PROC1_WDTDBG_VOSEL_MASK              0x7F
#define MT6359_LDO_VSRAM_PROC1_WDTDBG_VOSEL_SHIFT             8
#define MT6359_DA_VSRAM_PROC1_VOSEL_GRAY_ADDR                 \
	MT6359_LDO_VSRAM_PROC1_VOSEL1
#define MT6359_DA_VSRAM_PROC1_VOSEL_GRAY_MASK                 0x7F
#define MT6359_DA_VSRAM_PROC1_VOSEL_GRAY_SHIFT                0
#define MT6359_DA_VSRAM_PROC1_VOSEL_ADDR                      \
	MT6359_LDO_VSRAM_PROC1_VOSEL1
#define MT6359_DA_VSRAM_PROC1_VOSEL_MASK                      0x7F
#define MT6359_DA_VSRAM_PROC1_VOSEL_SHIFT                     8
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FRATE_ADDR            \
	MT6359_LDO_VSRAM_PROC1_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FRATE_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FRATE_SHIFT           0
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FEN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FEN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_FEN_SHIFT             7
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_RRATE_ADDR            \
	MT6359_LDO_VSRAM_PROC1_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_RRATE_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_RRATE_SHIFT           8
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_REN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_REN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_SFCHG_REN_SHIFT             15
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_TD_ADDR           \
	MT6359_LDO_VSRAM_PROC1_DVS
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_TD_MASK           0x3
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_TD_SHIFT          0
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_CTRL_ADDR         \
	MT6359_LDO_VSRAM_PROC1_DVS
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_CTRL_MASK         0x3
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_CTRL_SHIFT        4
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_ONCE_ADDR         \
	MT6359_LDO_VSRAM_PROC1_DVS
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_ONCE_MASK         0x1
#define MT6359_RG_LDO_VSRAM_PROC1_DVS_TRANS_ONCE_SHIFT        6
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_EN_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_EN_SHIFT             1
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_EN_SHIFT             2
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_EN_SHIFT             3
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_EN_SHIFT             4
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_EN_SHIFT             5
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_EN_SHIFT             6
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_EN_SHIFT             7
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_EN_SHIFT             8
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_EN_SHIFT             9
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_EN_SHIFT            10
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_EN_SHIFT            11
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_EN_SHIFT            12
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_EN_SHIFT            13
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_EN_SHIFT            14
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_EN_ADDR               \
	MT6359_LDO_VSRAM_PROC1_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_EN_MASK               0x1
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_EN_SHIFT              15
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_SET_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN_SET
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_SET_MASK              0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_SET_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_CLR_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_EN_CLR
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_CLR_MASK              0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC1_OP_EN_CLR_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW0_OP_CFG_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW1_OP_CFG_SHIFT            1
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW2_OP_CFG_SHIFT            2
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW3_OP_CFG_SHIFT            3
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW4_OP_CFG_SHIFT            4
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW5_OP_CFG_SHIFT            5
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW6_OP_CFG_SHIFT            6
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW7_OP_CFG_SHIFT            7
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW8_OP_CFG_SHIFT            8
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW9_OP_CFG_SHIFT            9
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW10_OP_CFG_SHIFT           10
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW11_OP_CFG_SHIFT           11
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW12_OP_CFG_SHIFT           12
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW13_OP_CFG_SHIFT           13
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC1_HW14_OP_CFG_SHIFT           14
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_CFG_ADDR              \
	MT6359_LDO_VSRAM_PROC1_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_CFG_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC1_SW_OP_CFG_SHIFT             15
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_SET_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG_SET
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_SET_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_SET_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_CLR_ADDR             \
	MT6359_LDO_VSRAM_PROC1_OP_CFG_CLR
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_CLR_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC1_OP_CFG_CLR_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_EN_ADDR               \
	MT6359_LDO_VSRAM_PROC1_TRACK0
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_EN_MASK               0x1
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_EN_SHIFT              0
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_MODE_ADDR             \
	MT6359_LDO_VSRAM_PROC1_TRACK0
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_MODE_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC1_TRACK_MODE_SHIFT            1
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_DELTA_ADDR            \
	MT6359_LDO_VSRAM_PROC1_TRACK1
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_DELTA_MASK            0xF
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_DELTA_SHIFT           0
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_OFFSET_ADDR           \
	MT6359_LDO_VSRAM_PROC1_TRACK1
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_OFFSET_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_OFFSET_SHIFT          8
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LB_ADDR               \
	MT6359_LDO_VSRAM_PROC1_TRACK2
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LB_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_LB_SHIFT              0
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_HB_ADDR               \
	MT6359_LDO_VSRAM_PROC1_TRACK2
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_HB_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_PROC1_VOSEL_HB_SHIFT              8
#define MT6359_RG_LDO_VSRAM_PROC2_EN_ADDR                     \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_EN_MASK                     0x1
#define MT6359_RG_LDO_VSRAM_PROC2_EN_SHIFT                    0
#define MT6359_RG_LDO_VSRAM_PROC2_LP_ADDR                     \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_LP_MASK                     0x1
#define MT6359_RG_LDO_VSRAM_PROC2_LP_SHIFT                    1
#define MT6359_RG_LDO_VSRAM_PROC2_STBTD_ADDR                  \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_STBTD_MASK                  0x3
#define MT6359_RG_LDO_VSRAM_PROC2_STBTD_SHIFT                 2
#define MT6359_RG_LDO_VSRAM_PROC2_ULP_ADDR                    \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_ULP_MASK                    0x1
#define MT6359_RG_LDO_VSRAM_PROC2_ULP_SHIFT                   4
#define MT6359_RG_LDO_VSRAM_PROC2_OCFB_EN_ADDR                \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_OCFB_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC2_OCFB_EN_SHIFT               5
#define MT6359_RG_LDO_VSRAM_PROC2_OC_MODE_ADDR                \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_OC_MODE_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC2_OC_MODE_SHIFT               6
#define MT6359_RG_LDO_VSRAM_PROC2_OC_TSEL_ADDR                \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_OC_TSEL_MASK                0x1
#define MT6359_RG_LDO_VSRAM_PROC2_OC_TSEL_SHIFT               7
#define MT6359_RG_LDO_VSRAM_PROC2_DUMMY_LOAD_ADDR             \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_DUMMY_LOAD_MASK             0x3
#define MT6359_RG_LDO_VSRAM_PROC2_DUMMY_LOAD_SHIFT            8
#define MT6359_RG_LDO_VSRAM_PROC2_OP_MODE_ADDR                \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_OP_MODE_MASK                0x7
#define MT6359_RG_LDO_VSRAM_PROC2_OP_MODE_SHIFT               10
#define MT6359_RG_LDO_VSRAM_PROC2_R2R_PDN_DIS_ADDR            \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_R2R_PDN_DIS_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_R2R_PDN_DIS_SHIFT           14
#define MT6359_RG_LDO_VSRAM_PROC2_CK_SW_MODE_ADDR             \
	MT6359_LDO_VSRAM_PROC2_CON0
#define MT6359_RG_LDO_VSRAM_PROC2_CK_SW_MODE_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_CK_SW_MODE_SHIFT            15
#define MT6359_DA_VSRAM_PROC2_B_EN_ADDR                       \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_B_EN_MASK                       0x1
#define MT6359_DA_VSRAM_PROC2_B_EN_SHIFT                      0
#define MT6359_DA_VSRAM_PROC2_B_STB_ADDR                      \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_B_STB_MASK                      0x1
#define MT6359_DA_VSRAM_PROC2_B_STB_SHIFT                     1
#define MT6359_DA_VSRAM_PROC2_B_LP_ADDR                       \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_B_LP_MASK                       0x1
#define MT6359_DA_VSRAM_PROC2_B_LP_SHIFT                      2
#define MT6359_DA_VSRAM_PROC2_L_EN_ADDR                       \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_L_EN_MASK                       0x1
#define MT6359_DA_VSRAM_PROC2_L_EN_SHIFT                      3
#define MT6359_DA_VSRAM_PROC2_L_STB_ADDR                      \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_L_STB_MASK                      0x1
#define MT6359_DA_VSRAM_PROC2_L_STB_SHIFT                     4
#define MT6359_DA_VSRAM_PROC2_OCFB_EN_ADDR                    \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_OCFB_EN_MASK                    0x1
#define MT6359_DA_VSRAM_PROC2_OCFB_EN_SHIFT                   5
#define MT6359_DA_VSRAM_PROC2_DUMMY_LOAD_ADDR                 \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_DUMMY_LOAD_MASK                 0x3
#define MT6359_DA_VSRAM_PROC2_DUMMY_LOAD_SHIFT                6
#define MT6359_DA_VSRAM_PROC2_VSLEEP_SEL_ADDR                 \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_VSLEEP_SEL_MASK                 0x1
#define MT6359_DA_VSRAM_PROC2_VSLEEP_SEL_SHIFT                8
#define MT6359_DA_VSRAM_PROC2_R2R_PDN_ADDR                    \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_R2R_PDN_MASK                    0x1
#define MT6359_DA_VSRAM_PROC2_R2R_PDN_SHIFT                   9
#define MT6359_DA_VSRAM_PROC2_TRACK_NDIS_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_MON
#define MT6359_DA_VSRAM_PROC2_TRACK_NDIS_EN_MASK              0x1
#define MT6359_DA_VSRAM_PROC2_TRACK_NDIS_EN_SHIFT             10
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_SLEEP_ADDR            \
	MT6359_LDO_VSRAM_PROC2_VOSEL0
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_SLEEP_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_SLEEP_SHIFT           0
#define MT6359_LDO_VSRAM_PROC2_WDTDBG_VOSEL_ADDR              \
	MT6359_LDO_VSRAM_PROC2_VOSEL0
#define MT6359_LDO_VSRAM_PROC2_WDTDBG_VOSEL_MASK              0x7F
#define MT6359_LDO_VSRAM_PROC2_WDTDBG_VOSEL_SHIFT             8
#define MT6359_DA_VSRAM_PROC2_VOSEL_GRAY_ADDR                 \
	MT6359_LDO_VSRAM_PROC2_VOSEL1
#define MT6359_DA_VSRAM_PROC2_VOSEL_GRAY_MASK                 0x7F
#define MT6359_DA_VSRAM_PROC2_VOSEL_GRAY_SHIFT                0
#define MT6359_DA_VSRAM_PROC2_VOSEL_ADDR                      \
	MT6359_LDO_VSRAM_PROC2_VOSEL1
#define MT6359_DA_VSRAM_PROC2_VOSEL_MASK                      0x7F
#define MT6359_DA_VSRAM_PROC2_VOSEL_SHIFT                     8
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FRATE_ADDR            \
	MT6359_LDO_VSRAM_PROC2_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FRATE_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FRATE_SHIFT           0
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FEN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FEN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_FEN_SHIFT             7
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_RRATE_ADDR            \
	MT6359_LDO_VSRAM_PROC2_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_RRATE_MASK            0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_RRATE_SHIFT           8
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_REN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_SFCHG
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_REN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_SFCHG_REN_SHIFT             15
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_TD_ADDR           \
	MT6359_LDO_VSRAM_PROC2_DVS
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_TD_MASK           0x3
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_TD_SHIFT          0
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_CTRL_ADDR         \
	MT6359_LDO_VSRAM_PROC2_DVS
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_CTRL_MASK         0x3
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_CTRL_SHIFT        4
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_ONCE_ADDR         \
	MT6359_LDO_VSRAM_PROC2_DVS
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_ONCE_MASK         0x1
#define MT6359_RG_LDO_VSRAM_PROC2_DVS_TRANS_ONCE_SHIFT        6
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_EN_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_EN_SHIFT             1
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_EN_SHIFT             2
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_EN_SHIFT             3
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_EN_SHIFT             4
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_EN_SHIFT             5
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_EN_SHIFT             6
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_EN_SHIFT             7
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_EN_SHIFT             8
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_EN_SHIFT             9
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_EN_SHIFT            10
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_EN_SHIFT            11
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_EN_SHIFT            12
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_EN_SHIFT            13
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_EN_SHIFT            14
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_EN_ADDR               \
	MT6359_LDO_VSRAM_PROC2_OP_EN
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_EN_MASK               0x1
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_EN_SHIFT              15
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_SET_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN_SET
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_SET_MASK              0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_SET_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_CLR_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_EN_CLR
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_CLR_MASK              0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC2_OP_EN_CLR_SHIFT             0
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW0_OP_CFG_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW1_OP_CFG_SHIFT            1
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW2_OP_CFG_SHIFT            2
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW3_OP_CFG_SHIFT            3
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW4_OP_CFG_SHIFT            4
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW5_OP_CFG_SHIFT            5
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW6_OP_CFG_SHIFT            6
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW7_OP_CFG_SHIFT            7
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW8_OP_CFG_SHIFT            8
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW9_OP_CFG_SHIFT            9
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW10_OP_CFG_SHIFT           10
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW11_OP_CFG_SHIFT           11
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW12_OP_CFG_SHIFT           12
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW13_OP_CFG_SHIFT           13
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_PROC2_HW14_OP_CFG_SHIFT           14
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_CFG_ADDR              \
	MT6359_LDO_VSRAM_PROC2_OP_CFG
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_CFG_MASK              0x1
#define MT6359_RG_LDO_VSRAM_PROC2_SW_OP_CFG_SHIFT             15
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_SET_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG_SET
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_SET_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_SET_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_CLR_ADDR             \
	MT6359_LDO_VSRAM_PROC2_OP_CFG_CLR
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_CLR_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_PROC2_OP_CFG_CLR_SHIFT            0
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_EN_ADDR               \
	MT6359_LDO_VSRAM_PROC2_TRACK0
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_EN_MASK               0x1
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_EN_SHIFT              0
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_MODE_ADDR             \
	MT6359_LDO_VSRAM_PROC2_TRACK0
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_MODE_MASK             0x1
#define MT6359_RG_LDO_VSRAM_PROC2_TRACK_MODE_SHIFT            1
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_DELTA_ADDR            \
	MT6359_LDO_VSRAM_PROC2_TRACK1
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_DELTA_MASK            0xF
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_DELTA_SHIFT           0
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_OFFSET_ADDR           \
	MT6359_LDO_VSRAM_PROC2_TRACK1
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_OFFSET_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_OFFSET_SHIFT          8
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LB_ADDR               \
	MT6359_LDO_VSRAM_PROC2_TRACK2
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LB_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_LB_SHIFT              0
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_HB_ADDR               \
	MT6359_LDO_VSRAM_PROC2_TRACK2
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_HB_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_PROC2_VOSEL_HB_SHIFT              8
#define MT6359_LDO_VSRAM1_ANA_ID_ADDR                         \
	MT6359_LDO_VSRAM1_DSN_ID
#define MT6359_LDO_VSRAM1_ANA_ID_MASK                         0xFF
#define MT6359_LDO_VSRAM1_ANA_ID_SHIFT                        0
#define MT6359_LDO_VSRAM1_DIG_ID_ADDR                         \
	MT6359_LDO_VSRAM1_DSN_ID
#define MT6359_LDO_VSRAM1_DIG_ID_MASK                         0xFF
#define MT6359_LDO_VSRAM1_DIG_ID_SHIFT                        8
#define MT6359_LDO_VSRAM1_ANA_MINOR_REV_ADDR                  \
	MT6359_LDO_VSRAM1_DSN_REV0
#define MT6359_LDO_VSRAM1_ANA_MINOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM1_ANA_MINOR_REV_SHIFT                 0
#define MT6359_LDO_VSRAM1_ANA_MAJOR_REV_ADDR                  \
	MT6359_LDO_VSRAM1_DSN_REV0
#define MT6359_LDO_VSRAM1_ANA_MAJOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM1_ANA_MAJOR_REV_SHIFT                 4
#define MT6359_LDO_VSRAM1_DIG_MINOR_REV_ADDR                  \
	MT6359_LDO_VSRAM1_DSN_REV0
#define MT6359_LDO_VSRAM1_DIG_MINOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM1_DIG_MINOR_REV_SHIFT                 8
#define MT6359_LDO_VSRAM1_DIG_MAJOR_REV_ADDR                  \
	MT6359_LDO_VSRAM1_DSN_REV0
#define MT6359_LDO_VSRAM1_DIG_MAJOR_REV_MASK                  0xF
#define MT6359_LDO_VSRAM1_DIG_MAJOR_REV_SHIFT                 12
#define MT6359_LDO_VSRAM1_DSN_CBS_ADDR                        \
	MT6359_LDO_VSRAM1_DSN_DBI
#define MT6359_LDO_VSRAM1_DSN_CBS_MASK                        0x3
#define MT6359_LDO_VSRAM1_DSN_CBS_SHIFT                       0
#define MT6359_LDO_VSRAM1_DSN_BIX_ADDR                        \
	MT6359_LDO_VSRAM1_DSN_DBI
#define MT6359_LDO_VSRAM1_DSN_BIX_MASK                        0x3
#define MT6359_LDO_VSRAM1_DSN_BIX_SHIFT                       2
#define MT6359_LDO_VSRAM1_DSN_ESP_ADDR                        \
	MT6359_LDO_VSRAM1_DSN_DBI
#define MT6359_LDO_VSRAM1_DSN_ESP_MASK                        0xFF
#define MT6359_LDO_VSRAM1_DSN_ESP_SHIFT                       8
#define MT6359_LDO_VSRAM1_DSN_FPI_ADDR                        \
	MT6359_LDO_VSRAM1_DSN_DXI
#define MT6359_LDO_VSRAM1_DSN_FPI_MASK                        0xFF
#define MT6359_LDO_VSRAM1_DSN_FPI_SHIFT                       0
#define MT6359_RG_LDO_VSRAM_OTHERS_EN_ADDR                    \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_EN_MASK                    0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_EN_SHIFT                   0
#define MT6359_RG_LDO_VSRAM_OTHERS_LP_ADDR                    \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_LP_MASK                    0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_LP_SHIFT                   1
#define MT6359_RG_LDO_VSRAM_OTHERS_STBTD_ADDR                 \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_STBTD_MASK                 0x3
#define MT6359_RG_LDO_VSRAM_OTHERS_STBTD_SHIFT                2
#define MT6359_RG_LDO_VSRAM_OTHERS_ULP_ADDR                   \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_ULP_MASK                   0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_ULP_SHIFT                  4
#define MT6359_RG_LDO_VSRAM_OTHERS_OCFB_EN_ADDR               \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_OCFB_EN_MASK               0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_OCFB_EN_SHIFT              5
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_MODE_ADDR               \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_MODE_MASK               0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_MODE_SHIFT              6
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_TSEL_ADDR               \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_TSEL_MASK               0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_OC_TSEL_SHIFT              7
#define MT6359_RG_LDO_VSRAM_OTHERS_DUMMY_LOAD_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_DUMMY_LOAD_MASK            0x3
#define MT6359_RG_LDO_VSRAM_OTHERS_DUMMY_LOAD_SHIFT           8
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_MODE_ADDR               \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_MODE_MASK               0x7
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_MODE_SHIFT              10
#define MT6359_RG_LDO_VSRAM_OTHERS_R2R_PDN_DIS_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_R2R_PDN_DIS_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_R2R_PDN_DIS_SHIFT          14
#define MT6359_RG_LDO_VSRAM_OTHERS_CK_SW_MODE_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_CON0
#define MT6359_RG_LDO_VSRAM_OTHERS_CK_SW_MODE_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_CK_SW_MODE_SHIFT           15
#define MT6359_DA_VSRAM_OTHERS_B_EN_ADDR                      \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_B_EN_MASK                      0x1
#define MT6359_DA_VSRAM_OTHERS_B_EN_SHIFT                     0
#define MT6359_DA_VSRAM_OTHERS_B_STB_ADDR                     \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_B_STB_MASK                     0x1
#define MT6359_DA_VSRAM_OTHERS_B_STB_SHIFT                    1
#define MT6359_DA_VSRAM_OTHERS_B_LP_ADDR                      \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_B_LP_MASK                      0x1
#define MT6359_DA_VSRAM_OTHERS_B_LP_SHIFT                     2
#define MT6359_DA_VSRAM_OTHERS_L_EN_ADDR                      \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_L_EN_MASK                      0x1
#define MT6359_DA_VSRAM_OTHERS_L_EN_SHIFT                     3
#define MT6359_DA_VSRAM_OTHERS_L_STB_ADDR                     \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_L_STB_MASK                     0x1
#define MT6359_DA_VSRAM_OTHERS_L_STB_SHIFT                    4
#define MT6359_DA_VSRAM_OTHERS_OCFB_EN_ADDR                   \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_OCFB_EN_MASK                   0x1
#define MT6359_DA_VSRAM_OTHERS_OCFB_EN_SHIFT                  5
#define MT6359_DA_VSRAM_OTHERS_DUMMY_LOAD_ADDR                \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_DUMMY_LOAD_MASK                0x3
#define MT6359_DA_VSRAM_OTHERS_DUMMY_LOAD_SHIFT               6
#define MT6359_DA_VSRAM_OTHERS_VSLEEP_SEL_ADDR                \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_VSLEEP_SEL_MASK                0x1
#define MT6359_DA_VSRAM_OTHERS_VSLEEP_SEL_SHIFT               8
#define MT6359_DA_VSRAM_OTHERS_R2R_PDN_ADDR                   \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_R2R_PDN_MASK                   0x1
#define MT6359_DA_VSRAM_OTHERS_R2R_PDN_SHIFT                  9
#define MT6359_DA_VSRAM_OTHERS_TRACK_NDIS_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_MON
#define MT6359_DA_VSRAM_OTHERS_TRACK_NDIS_EN_MASK             0x1
#define MT6359_DA_VSRAM_OTHERS_TRACK_NDIS_EN_SHIFT            10
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_SLEEP_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_VOSEL0
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_SLEEP_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_SLEEP_SHIFT          0
#define MT6359_LDO_VSRAM_OTHERS_WDTDBG_VOSEL_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_VOSEL0
#define MT6359_LDO_VSRAM_OTHERS_WDTDBG_VOSEL_MASK             0x7F
#define MT6359_LDO_VSRAM_OTHERS_WDTDBG_VOSEL_SHIFT            8
#define MT6359_DA_VSRAM_OTHERS_VOSEL_GRAY_ADDR                \
	MT6359_LDO_VSRAM_OTHERS_VOSEL1
#define MT6359_DA_VSRAM_OTHERS_VOSEL_GRAY_MASK                0x7F
#define MT6359_DA_VSRAM_OTHERS_VOSEL_GRAY_SHIFT               0
#define MT6359_DA_VSRAM_OTHERS_VOSEL_ADDR                     \
	MT6359_LDO_VSRAM_OTHERS_VOSEL1
#define MT6359_DA_VSRAM_OTHERS_VOSEL_MASK                     0x7F
#define MT6359_DA_VSRAM_OTHERS_VOSEL_SHIFT                    8
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FRATE_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_SFCHG
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FRATE_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FRATE_SHIFT          0
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FEN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_SFCHG
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FEN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_FEN_SHIFT            7
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_RRATE_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_SFCHG
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_RRATE_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_RRATE_SHIFT          8
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_REN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_SFCHG
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_REN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SFCHG_REN_SHIFT            15
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_TD_ADDR          \
	MT6359_LDO_VSRAM_OTHERS_DVS
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_TD_MASK          0x3
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_TD_SHIFT         0
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_CTRL_ADDR        \
	MT6359_LDO_VSRAM_OTHERS_DVS
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_CTRL_MASK        0x3
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_CTRL_SHIFT       4
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_ONCE_ADDR        \
	MT6359_LDO_VSRAM_OTHERS_DVS
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_ONCE_MASK        0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_DVS_TRANS_ONCE_SHIFT       6
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_EN_SHIFT            0
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_EN_SHIFT            1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_EN_SHIFT            2
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_EN_SHIFT            3
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_EN_SHIFT            4
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_EN_SHIFT            5
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_EN_SHIFT            6
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_EN_SHIFT            7
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_EN_SHIFT            8
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_EN_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_EN_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_EN_SHIFT            9
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_EN_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_EN_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_EN_SHIFT           10
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_EN_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_EN_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_EN_SHIFT           11
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_EN_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_EN_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_EN_SHIFT           12
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_EN_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_EN_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_EN_SHIFT           13
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_EN_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_EN_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_EN_SHIFT           14
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_EN_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_OP_EN
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_EN_SHIFT             15
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_SET_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN_SET
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_SET_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_SET_SHIFT            0
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_CLR_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_EN_CLR
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_CLR_MASK             0xFFFF
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_EN_CLR_SHIFT            0
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW0_OP_CFG_SHIFT           0
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW1_OP_CFG_SHIFT           1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW2_OP_CFG_SHIFT           2
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW3_OP_CFG_SHIFT           3
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW4_OP_CFG_SHIFT           4
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW5_OP_CFG_SHIFT           5
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW6_OP_CFG_SHIFT           6
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW7_OP_CFG_SHIFT           7
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW8_OP_CFG_SHIFT           8
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_CFG_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_CFG_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW9_OP_CFG_SHIFT           9
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_CFG_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_CFG_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW10_OP_CFG_SHIFT          10
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_CFG_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_CFG_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW11_OP_CFG_SHIFT          11
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_CFG_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_CFG_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW12_OP_CFG_SHIFT          12
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_CFG_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_CFG_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW13_OP_CFG_SHIFT          13
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_CFG_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_CFG_MASK           0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_HW14_OP_CFG_SHIFT          14
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_CFG_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_CFG_MASK             0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SW_OP_CFG_SHIFT            15
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_SET_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG_SET
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_SET_MASK            0xFFFF
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_SET_SHIFT           0
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_CLR_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_OP_CFG_CLR
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_CLR_MASK            0xFFFF
#define MT6359_RG_LDO_VSRAM_OTHERS_OP_CFG_CLR_SHIFT           0
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_EN_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_TRACK0
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_EN_SHIFT             0
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_MODE_ADDR            \
	MT6359_LDO_VSRAM_OTHERS_TRACK0
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_MODE_MASK            0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_TRACK_MODE_SHIFT           1
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_DELTA_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_TRACK1
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_DELTA_MASK           0xF
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_DELTA_SHIFT          0
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_OFFSET_ADDR          \
	MT6359_LDO_VSRAM_OTHERS_TRACK1
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_OFFSET_MASK          0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_OFFSET_SHIFT         8
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LB_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_TRACK2
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LB_MASK              0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_LB_SHIFT             0
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_HB_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_TRACK2
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_HB_MASK              0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_VOSEL_HB_SHIFT             8
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_EN_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_SSHUB
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_EN_SHIFT             0
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_SSHUB
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SHIFT          1
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN_ADDR  \
	MT6359_LDO_VSRAM_OTHERS_SSHUB
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN_MASK  0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN_SHIFT 8
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP_ADDR     \
	MT6359_LDO_VSRAM_OTHERS_SSHUB
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP_MASK     0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP_SHIFT    9
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_EN_ADDR              \
	MT6359_LDO_VSRAM_OTHERS_BT
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_EN_MASK              0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_EN_SHIFT             0
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_VOSEL_ADDR           \
	MT6359_LDO_VSRAM_OTHERS_BT
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_VOSEL_MASK           0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_BT_LP_VOSEL_SHIFT          1
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_EN_ADDR                \
	MT6359_LDO_VSRAM_OTHERS_SPI
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_EN_SHIFT               0
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_VOSEL_ADDR             \
	MT6359_LDO_VSRAM_OTHERS_SPI
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_VOSEL_MASK             0x7F
#define MT6359_RG_LDO_VSRAM_OTHERS_SPI_VOSEL_SHIFT            1
#define MT6359_RG_LDO_VSRAM_MD_EN_ADDR                        \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_EN_MASK                        0x1
#define MT6359_RG_LDO_VSRAM_MD_EN_SHIFT                       0
#define MT6359_RG_LDO_VSRAM_MD_LP_ADDR                        \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_LP_MASK                        0x1
#define MT6359_RG_LDO_VSRAM_MD_LP_SHIFT                       1
#define MT6359_RG_LDO_VSRAM_MD_STBTD_ADDR                     \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_STBTD_MASK                     0x3
#define MT6359_RG_LDO_VSRAM_MD_STBTD_SHIFT                    2
#define MT6359_RG_LDO_VSRAM_MD_ULP_ADDR                       \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_ULP_MASK                       0x1
#define MT6359_RG_LDO_VSRAM_MD_ULP_SHIFT                      4
#define MT6359_RG_LDO_VSRAM_MD_OCFB_EN_ADDR                   \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_OCFB_EN_MASK                   0x1
#define MT6359_RG_LDO_VSRAM_MD_OCFB_EN_SHIFT                  5
#define MT6359_RG_LDO_VSRAM_MD_OC_MODE_ADDR                   \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_OC_MODE_MASK                   0x1
#define MT6359_RG_LDO_VSRAM_MD_OC_MODE_SHIFT                  6
#define MT6359_RG_LDO_VSRAM_MD_OC_TSEL_ADDR                   \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_OC_TSEL_MASK                   0x1
#define MT6359_RG_LDO_VSRAM_MD_OC_TSEL_SHIFT                  7
#define MT6359_RG_LDO_VSRAM_MD_DUMMY_LOAD_ADDR                \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_DUMMY_LOAD_MASK                0x3
#define MT6359_RG_LDO_VSRAM_MD_DUMMY_LOAD_SHIFT               8
#define MT6359_RG_LDO_VSRAM_MD_OP_MODE_ADDR                   \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_OP_MODE_MASK                   0x7
#define MT6359_RG_LDO_VSRAM_MD_OP_MODE_SHIFT                  10
#define MT6359_RG_LDO_VSRAM_MD_R2R_PDN_DIS_ADDR               \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_R2R_PDN_DIS_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_R2R_PDN_DIS_SHIFT              14
#define MT6359_RG_LDO_VSRAM_MD_CK_SW_MODE_ADDR                \
	MT6359_LDO_VSRAM_MD_CON0
#define MT6359_RG_LDO_VSRAM_MD_CK_SW_MODE_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_CK_SW_MODE_SHIFT               15
#define MT6359_DA_VSRAM_MD_B_EN_ADDR                          \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_B_EN_MASK                          0x1
#define MT6359_DA_VSRAM_MD_B_EN_SHIFT                         0
#define MT6359_DA_VSRAM_MD_B_STB_ADDR                         \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_B_STB_MASK                         0x1
#define MT6359_DA_VSRAM_MD_B_STB_SHIFT                        1
#define MT6359_DA_VSRAM_MD_B_LP_ADDR                          \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_B_LP_MASK                          0x1
#define MT6359_DA_VSRAM_MD_B_LP_SHIFT                         2
#define MT6359_DA_VSRAM_MD_L_EN_ADDR                          \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_L_EN_MASK                          0x1
#define MT6359_DA_VSRAM_MD_L_EN_SHIFT                         3
#define MT6359_DA_VSRAM_MD_L_STB_ADDR                         \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_L_STB_MASK                         0x1
#define MT6359_DA_VSRAM_MD_L_STB_SHIFT                        4
#define MT6359_DA_VSRAM_MD_OCFB_EN_ADDR                       \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_OCFB_EN_MASK                       0x1
#define MT6359_DA_VSRAM_MD_OCFB_EN_SHIFT                      5
#define MT6359_DA_VSRAM_MD_DUMMY_LOAD_ADDR                    \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_DUMMY_LOAD_MASK                    0x3
#define MT6359_DA_VSRAM_MD_DUMMY_LOAD_SHIFT                   6
#define MT6359_DA_VSRAM_MD_VSLEEP_SEL_ADDR                    \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_VSLEEP_SEL_MASK                    0x1
#define MT6359_DA_VSRAM_MD_VSLEEP_SEL_SHIFT                   8
#define MT6359_DA_VSRAM_MD_R2R_PDN_ADDR                       \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_R2R_PDN_MASK                       0x1
#define MT6359_DA_VSRAM_MD_R2R_PDN_SHIFT                      9
#define MT6359_DA_VSRAM_MD_TRACK_NDIS_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_MON
#define MT6359_DA_VSRAM_MD_TRACK_NDIS_EN_MASK                 0x1
#define MT6359_DA_VSRAM_MD_TRACK_NDIS_EN_SHIFT                10
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_SLEEP_ADDR               \
	MT6359_LDO_VSRAM_MD_VOSEL0
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_SLEEP_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_SLEEP_SHIFT              0
#define MT6359_LDO_VSRAM_MD_WDTDBG_VOSEL_ADDR                 \
	MT6359_LDO_VSRAM_MD_VOSEL0
#define MT6359_LDO_VSRAM_MD_WDTDBG_VOSEL_MASK                 0x7F
#define MT6359_LDO_VSRAM_MD_WDTDBG_VOSEL_SHIFT                8
#define MT6359_DA_VSRAM_MD_VOSEL_GRAY_ADDR                    \
	MT6359_LDO_VSRAM_MD_VOSEL1
#define MT6359_DA_VSRAM_MD_VOSEL_GRAY_MASK                    0x7F
#define MT6359_DA_VSRAM_MD_VOSEL_GRAY_SHIFT                   0
#define MT6359_DA_VSRAM_MD_VOSEL_ADDR                         \
	MT6359_LDO_VSRAM_MD_VOSEL1
#define MT6359_DA_VSRAM_MD_VOSEL_MASK                         0x7F
#define MT6359_DA_VSRAM_MD_VOSEL_SHIFT                        8
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FRATE_ADDR               \
	MT6359_LDO_VSRAM_MD_SFCHG
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FRATE_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FRATE_SHIFT              0
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FEN_ADDR                 \
	MT6359_LDO_VSRAM_MD_SFCHG
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FEN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_FEN_SHIFT                7
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_RRATE_ADDR               \
	MT6359_LDO_VSRAM_MD_SFCHG
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_RRATE_MASK               0x7F
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_RRATE_SHIFT              8
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_REN_ADDR                 \
	MT6359_LDO_VSRAM_MD_SFCHG
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_REN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_SFCHG_REN_SHIFT                15
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_TD_ADDR              \
	MT6359_LDO_VSRAM_MD_DVS
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_TD_MASK              0x3
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_TD_SHIFT             0
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_CTRL_ADDR            \
	MT6359_LDO_VSRAM_MD_DVS
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_CTRL_MASK            0x3
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_CTRL_SHIFT           4
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_ONCE_ADDR            \
	MT6359_LDO_VSRAM_MD_DVS
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_ONCE_MASK            0x1
#define MT6359_RG_LDO_VSRAM_MD_DVS_TRANS_ONCE_SHIFT           6
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_EN_SHIFT                0
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_EN_SHIFT                1
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_EN_SHIFT                2
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_EN_SHIFT                3
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_EN_SHIFT                4
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_EN_SHIFT                5
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_EN_SHIFT                6
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_EN_SHIFT                7
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_EN_SHIFT                8
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_EN_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_EN_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_EN_SHIFT                9
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_EN_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_EN_SHIFT               10
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_EN_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_EN_SHIFT               11
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_EN_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_EN_SHIFT               12
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_EN_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_EN_SHIFT               13
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_EN_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_EN_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_EN_SHIFT               14
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_EN_ADDR                  \
	MT6359_LDO_VSRAM_MD_OP_EN
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_EN_MASK                  0x1
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_EN_SHIFT                 15
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_SET_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN_SET
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_SET_MASK                 0xFFFF
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_SET_SHIFT                0
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_CLR_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_EN_CLR
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_CLR_MASK                 0xFFFF
#define MT6359_RG_LDO_VSRAM_MD_OP_EN_CLR_SHIFT                0
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW0_OP_CFG_SHIFT               0
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW1_OP_CFG_SHIFT               1
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW2_OP_CFG_SHIFT               2
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW3_OP_CFG_SHIFT               3
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW4_OP_CFG_SHIFT               4
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW5_OP_CFG_SHIFT               5
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW6_OP_CFG_SHIFT               6
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW7_OP_CFG_SHIFT               7
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW8_OP_CFG_SHIFT               8
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_CFG_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_CFG_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_HW9_OP_CFG_SHIFT               9
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_CFG_ADDR               \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_CFG_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_HW10_OP_CFG_SHIFT              10
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_CFG_ADDR               \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_CFG_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_HW11_OP_CFG_SHIFT              11
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_CFG_ADDR               \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_CFG_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_HW12_OP_CFG_SHIFT              12
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_CFG_ADDR               \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_CFG_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_HW13_OP_CFG_SHIFT              13
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_CFG_ADDR               \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_CFG_MASK               0x1
#define MT6359_RG_LDO_VSRAM_MD_HW14_OP_CFG_SHIFT              14
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_CFG_ADDR                 \
	MT6359_LDO_VSRAM_MD_OP_CFG
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_CFG_MASK                 0x1
#define MT6359_RG_LDO_VSRAM_MD_SW_OP_CFG_SHIFT                15
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_SET_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG_SET
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_SET_MASK                0xFFFF
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_SET_SHIFT               0
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_CLR_ADDR                \
	MT6359_LDO_VSRAM_MD_OP_CFG_CLR
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_CLR_MASK                0xFFFF
#define MT6359_RG_LDO_VSRAM_MD_OP_CFG_CLR_SHIFT               0
#define MT6359_RG_LDO_VSRAM_MD_TRACK_EN_ADDR                  \
	MT6359_LDO_VSRAM_MD_TRACK0
#define MT6359_RG_LDO_VSRAM_MD_TRACK_EN_MASK                  0x1
#define MT6359_RG_LDO_VSRAM_MD_TRACK_EN_SHIFT                 0
#define MT6359_RG_LDO_VSRAM_MD_TRACK_MODE_ADDR                \
	MT6359_LDO_VSRAM_MD_TRACK0
#define MT6359_RG_LDO_VSRAM_MD_TRACK_MODE_MASK                0x1
#define MT6359_RG_LDO_VSRAM_MD_TRACK_MODE_SHIFT               1
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_DELTA_ADDR               \
	MT6359_LDO_VSRAM_MD_TRACK1
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_DELTA_MASK               0xF
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_DELTA_SHIFT              0
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_OFFSET_ADDR              \
	MT6359_LDO_VSRAM_MD_TRACK1
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_OFFSET_MASK              0x7F
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_OFFSET_SHIFT             8
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LB_ADDR                  \
	MT6359_LDO_VSRAM_MD_TRACK2
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LB_MASK                  0x7F
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_LB_SHIFT                 0
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_HB_ADDR                  \
	MT6359_LDO_VSRAM_MD_TRACK2
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_HB_MASK                  0x7F
#define MT6359_RG_LDO_VSRAM_MD_VOSEL_HB_SHIFT                 8
#define MT6359_LDO_ANA0_ANA_ID_ADDR                           \
	MT6359_LDO_ANA0_DSN_ID
#define MT6359_LDO_ANA0_ANA_ID_MASK                           0xFF
#define MT6359_LDO_ANA0_ANA_ID_SHIFT                          0
#define MT6359_LDO_ANA0_DIG_ID_ADDR                           \
	MT6359_LDO_ANA0_DSN_ID
#define MT6359_LDO_ANA0_DIG_ID_MASK                           0xFF
#define MT6359_LDO_ANA0_DIG_ID_SHIFT                          8
#define MT6359_LDO_ANA0_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA0_DSN_REV0
#define MT6359_LDO_ANA0_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA0_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_ANA0_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA0_DSN_REV0
#define MT6359_LDO_ANA0_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA0_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_ANA0_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA0_DSN_REV0
#define MT6359_LDO_ANA0_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA0_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_ANA0_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA0_DSN_REV0
#define MT6359_LDO_ANA0_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA0_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_ANA0_DSN_CBS_ADDR                          \
	MT6359_LDO_ANA0_DSN_DBI
#define MT6359_LDO_ANA0_DSN_CBS_MASK                          0x3
#define MT6359_LDO_ANA0_DSN_CBS_SHIFT                         0
#define MT6359_LDO_ANA0_DSN_BIX_ADDR                          \
	MT6359_LDO_ANA0_DSN_DBI
#define MT6359_LDO_ANA0_DSN_BIX_MASK                          0x3
#define MT6359_LDO_ANA0_DSN_BIX_SHIFT                         2
#define MT6359_LDO_ANA0_DSN_ESP_ADDR                          \
	MT6359_LDO_ANA0_DSN_DBI
#define MT6359_LDO_ANA0_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_ANA0_DSN_ESP_SHIFT                         8
#define MT6359_LDO_ANA0_DSN_FPI_ADDR                          \
	MT6359_LDO_ANA0_DSN_FPI
#define MT6359_LDO_ANA0_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_ANA0_DSN_FPI_SHIFT                         0
#define MT6359_RG_VFE28_VOCAL_ADDR                            \
	MT6359_VFE28_ANA_CON0
#define MT6359_RG_VFE28_VOCAL_MASK                            0xF
#define MT6359_RG_VFE28_VOCAL_SHIFT                           0
#define MT6359_RG_VFE28_VOSEL_ADDR                            \
	MT6359_VFE28_ANA_CON0
#define MT6359_RG_VFE28_VOSEL_MASK                            0xF
#define MT6359_RG_VFE28_VOSEL_SHIFT                           8
#define MT6359_RG_VFE28_NDIS_EN_ADDR                          \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_NDIS_EN_MASK                          0x1
#define MT6359_RG_VFE28_NDIS_EN_SHIFT                         0
#define MT6359_RG_VFE28_RSV_1_ADDR                            \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_RSV_1_MASK                            0x1
#define MT6359_RG_VFE28_RSV_1_SHIFT                           2
#define MT6359_RG_VFE28_OC_LP_EN_ADDR                         \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VFE28_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VFE28_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VFE28_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VFE28_ULP_BIASX2_EN_ADDR                    \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VFE28_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VFE28_MEASURE_FT_EN_ADDR                    \
	MT6359_VFE28_ANA_CON1
#define MT6359_RG_VFE28_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VFE28_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VFE28_OC_STATUS_ADDR                       \
	MT6359_VFE28_ANA_CON1
#define MT6359_RGS_VFE28_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VFE28_OC_STATUS_SHIFT                      7
#define MT6359_RG_VAUX18_VOCAL_ADDR                           \
	MT6359_VAUX18_ANA_CON0
#define MT6359_RG_VAUX18_VOCAL_MASK                           0xF
#define MT6359_RG_VAUX18_VOCAL_SHIFT                          0
#define MT6359_RG_VAUX18_VOSEL_ADDR                           \
	MT6359_VAUX18_ANA_CON0
#define MT6359_RG_VAUX18_VOSEL_MASK                           0xF
#define MT6359_RG_VAUX18_VOSEL_SHIFT                          8
#define MT6359_RG_VAUX18_NDIS_EN_ADDR                         \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_NDIS_EN_MASK                         0x1
#define MT6359_RG_VAUX18_NDIS_EN_SHIFT                        0
#define MT6359_RG_VAUX18_RSV_1_ADDR                           \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_RSV_1_MASK                           0x1
#define MT6359_RG_VAUX18_RSV_1_SHIFT                          2
#define MT6359_RG_VAUX18_OC_LP_EN_ADDR                        \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_OC_LP_EN_MASK                        0x1
#define MT6359_RG_VAUX18_OC_LP_EN_SHIFT                       3
#define MT6359_RG_VAUX18_ULP_IQ_CLAMP_EN_ADDR                 \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_ULP_IQ_CLAMP_EN_MASK                 0x1
#define MT6359_RG_VAUX18_ULP_IQ_CLAMP_EN_SHIFT                4
#define MT6359_RG_VAUX18_ULP_BIASX2_EN_ADDR                   \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_ULP_BIASX2_EN_MASK                   0x1
#define MT6359_RG_VAUX18_ULP_BIASX2_EN_SHIFT                  5
#define MT6359_RG_VAUX18_MEASURE_FT_EN_ADDR                   \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RG_VAUX18_MEASURE_FT_EN_MASK                   0x1
#define MT6359_RG_VAUX18_MEASURE_FT_EN_SHIFT                  6
#define MT6359_RGS_VAUX18_OC_STATUS_ADDR                      \
	MT6359_VAUX18_ANA_CON1
#define MT6359_RGS_VAUX18_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VAUX18_OC_STATUS_SHIFT                     7
#define MT6359_RG_VUSB_VOCAL_ADDR                             \
	MT6359_VUSB_ANA_CON0
#define MT6359_RG_VUSB_VOCAL_MASK                             0xF
#define MT6359_RG_VUSB_VOCAL_SHIFT                            0
#define MT6359_RG_VUSB_VOSEL_ADDR                             \
	MT6359_VUSB_ANA_CON0
#define MT6359_RG_VUSB_VOSEL_MASK                             0xF
#define MT6359_RG_VUSB_VOSEL_SHIFT                            8
#define MT6359_RG_VUSB_NDIS_EN_ADDR                           \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_NDIS_EN_MASK                           0x1
#define MT6359_RG_VUSB_NDIS_EN_SHIFT                          0
#define MT6359_RG_VUSB_RSV_1_ADDR                             \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_RSV_1_MASK                             0x1
#define MT6359_RG_VUSB_RSV_1_SHIFT                            2
#define MT6359_RG_VUSB_OC_LP_EN_ADDR                          \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VUSB_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VUSB_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VUSB_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VUSB_ULP_BIASX2_EN_ADDR                     \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VUSB_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VUSB_MEASURE_FT_EN_ADDR                     \
	MT6359_VUSB_ANA_CON1
#define MT6359_RG_VUSB_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VUSB_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VUSB_OC_STATUS_ADDR                        \
	MT6359_VUSB_ANA_CON1
#define MT6359_RGS_VUSB_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VUSB_OC_STATUS_SHIFT                       7
#define MT6359_RG_VBIF28_VOCAL_ADDR                           \
	MT6359_VBIF28_ANA_CON0
#define MT6359_RG_VBIF28_VOCAL_MASK                           0xF
#define MT6359_RG_VBIF28_VOCAL_SHIFT                          0
#define MT6359_RG_VBIF28_VOSEL_ADDR                           \
	MT6359_VBIF28_ANA_CON0
#define MT6359_RG_VBIF28_VOSEL_MASK                           0xF
#define MT6359_RG_VBIF28_VOSEL_SHIFT                          8
#define MT6359_RG_VBIF28_NDIS_EN_ADDR                         \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_NDIS_EN_MASK                         0x1
#define MT6359_RG_VBIF28_NDIS_EN_SHIFT                        0
#define MT6359_RG_VBIF28_RSV_1_ADDR                           \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_RSV_1_MASK                           0x1
#define MT6359_RG_VBIF28_RSV_1_SHIFT                          2
#define MT6359_RG_VBIF28_OC_LP_EN_ADDR                        \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_OC_LP_EN_MASK                        0x1
#define MT6359_RG_VBIF28_OC_LP_EN_SHIFT                       3
#define MT6359_RG_VBIF28_ULP_IQ_CLAMP_EN_ADDR                 \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_ULP_IQ_CLAMP_EN_MASK                 0x1
#define MT6359_RG_VBIF28_ULP_IQ_CLAMP_EN_SHIFT                4
#define MT6359_RG_VBIF28_ULP_BIASX2_EN_ADDR                   \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_ULP_BIASX2_EN_MASK                   0x1
#define MT6359_RG_VBIF28_ULP_BIASX2_EN_SHIFT                  5
#define MT6359_RG_VBIF28_MEASURE_FT_EN_ADDR                   \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RG_VBIF28_MEASURE_FT_EN_MASK                   0x1
#define MT6359_RG_VBIF28_MEASURE_FT_EN_SHIFT                  6
#define MT6359_RGS_VBIF28_OC_STATUS_ADDR                      \
	MT6359_VBIF28_ANA_CON1
#define MT6359_RGS_VBIF28_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VBIF28_OC_STATUS_SHIFT                     7
#define MT6359_RG_VCN33_1_VOCAL_ADDR                          \
	MT6359_VCN33_1_ANA_CON0
#define MT6359_RG_VCN33_1_VOCAL_MASK                          0xF
#define MT6359_RG_VCN33_1_VOCAL_SHIFT                         0
#define MT6359_RG_VCN33_1_VOSEL_ADDR                          \
	MT6359_VCN33_1_ANA_CON0
#define MT6359_RG_VCN33_1_VOSEL_MASK                          0xF
#define MT6359_RG_VCN33_1_VOSEL_SHIFT                         8
#define MT6359_RG_VCN33_1_NDIS_EN_ADDR                        \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_NDIS_EN_MASK                        0x1
#define MT6359_RG_VCN33_1_NDIS_EN_SHIFT                       0
#define MT6359_RG_VCN33_1_RSV_0_ADDR                          \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_RSV_0_MASK                          0x3
#define MT6359_RG_VCN33_1_RSV_0_SHIFT                         2
#define MT6359_RG_VCN33_1_RSV_1_ADDR                          \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_RSV_1_MASK                          0x1
#define MT6359_RG_VCN33_1_RSV_1_SHIFT                         4
#define MT6359_RG_VCN33_1_OC_LP_EN_ADDR                       \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_OC_LP_EN_MASK                       0x1
#define MT6359_RG_VCN33_1_OC_LP_EN_SHIFT                      5
#define MT6359_RG_VCN33_1_ULP_IQ_CLAMP_EN_ADDR                \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_ULP_IQ_CLAMP_EN_MASK                0x1
#define MT6359_RG_VCN33_1_ULP_IQ_CLAMP_EN_SHIFT               6
#define MT6359_RG_VCN33_1_ULP_BIASX2_EN_ADDR                  \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_ULP_BIASX2_EN_MASK                  0x1
#define MT6359_RG_VCN33_1_ULP_BIASX2_EN_SHIFT                 7
#define MT6359_RG_VCN33_1_MEASURE_FT_EN_ADDR                  \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RG_VCN33_1_MEASURE_FT_EN_MASK                  0x1
#define MT6359_RG_VCN33_1_MEASURE_FT_EN_SHIFT                 8
#define MT6359_RGS_VCN33_1_OC_STATUS_ADDR                     \
	MT6359_VCN33_1_ANA_CON1
#define MT6359_RGS_VCN33_1_OC_STATUS_MASK                     0x1
#define MT6359_RGS_VCN33_1_OC_STATUS_SHIFT                    9
#define MT6359_RG_VCN33_2_VOCAL_ADDR                          \
	MT6359_VCN33_2_ANA_CON0
#define MT6359_RG_VCN33_2_VOCAL_MASK                          0xF
#define MT6359_RG_VCN33_2_VOCAL_SHIFT                         0
#define MT6359_RG_VCN33_2_VOSEL_ADDR                          \
	MT6359_VCN33_2_ANA_CON0
#define MT6359_RG_VCN33_2_VOSEL_MASK                          0xF
#define MT6359_RG_VCN33_2_VOSEL_SHIFT                         8
#define MT6359_RG_VCN33_2_NDIS_EN_ADDR                        \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_NDIS_EN_MASK                        0x1
#define MT6359_RG_VCN33_2_NDIS_EN_SHIFT                       0
#define MT6359_RG_VCN33_2_RSV_0_ADDR                          \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_RSV_0_MASK                          0x3
#define MT6359_RG_VCN33_2_RSV_0_SHIFT                         2
#define MT6359_RG_VCN33_2_RSV_1_ADDR                          \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_RSV_1_MASK                          0x1
#define MT6359_RG_VCN33_2_RSV_1_SHIFT                         4
#define MT6359_RG_VCN33_2_OC_LP_EN_ADDR                       \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_OC_LP_EN_MASK                       0x1
#define MT6359_RG_VCN33_2_OC_LP_EN_SHIFT                      5
#define MT6359_RG_VCN33_2_ULP_IQ_CLAMP_EN_ADDR                \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_ULP_IQ_CLAMP_EN_MASK                0x1
#define MT6359_RG_VCN33_2_ULP_IQ_CLAMP_EN_SHIFT               6
#define MT6359_RG_VCN33_2_ULP_BIASX2_EN_ADDR                  \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_ULP_BIASX2_EN_MASK                  0x1
#define MT6359_RG_VCN33_2_ULP_BIASX2_EN_SHIFT                 7
#define MT6359_RG_VCN33_2_MEASURE_FT_EN_ADDR                  \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RG_VCN33_2_MEASURE_FT_EN_MASK                  0x1
#define MT6359_RG_VCN33_2_MEASURE_FT_EN_SHIFT                 8
#define MT6359_RGS_VCN33_2_OC_STATUS_ADDR                     \
	MT6359_VCN33_2_ANA_CON1
#define MT6359_RGS_VCN33_2_OC_STATUS_MASK                     0x1
#define MT6359_RGS_VCN33_2_OC_STATUS_SHIFT                    9
#define MT6359_RG_VEMC_VOCAL_ADDR                             \
	MT6359_VEMC_ANA_CON0
#define MT6359_RG_VEMC_VOCAL_MASK                             0xF
#define MT6359_RG_VEMC_VOCAL_SHIFT                            0
#define MT6359_RG_VEMC_VOSEL_ADDR                             \
	MT6359_VEMC_ANA_CON0
#define MT6359_RG_VEMC_VOSEL_MASK                             0xF
#define MT6359_RG_VEMC_VOSEL_SHIFT                            8
#define MT6359_RG_VEMC_NDIS_EN_ADDR                           \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_NDIS_EN_MASK                           0x1
#define MT6359_RG_VEMC_NDIS_EN_SHIFT                          0
#define MT6359_RG_VEMC_RSV_0_ADDR                             \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_RSV_0_MASK                             0x3
#define MT6359_RG_VEMC_RSV_0_SHIFT                            2
#define MT6359_RG_VEMC_RSV_1_ADDR                             \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_RSV_1_MASK                             0x1
#define MT6359_RG_VEMC_RSV_1_SHIFT                            4
#define MT6359_RG_VEMC_OC_LP_EN_ADDR                          \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VEMC_OC_LP_EN_SHIFT                         5
#define MT6359_RG_VEMC_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VEMC_ULP_IQ_CLAMP_EN_SHIFT                  6
#define MT6359_RG_VEMC_ULP_BIASX2_EN_ADDR                     \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VEMC_ULP_BIASX2_EN_SHIFT                    7
#define MT6359_RG_VEMC_MEASURE_FT_EN_ADDR                     \
	MT6359_VEMC_ANA_CON1
#define MT6359_RG_VEMC_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VEMC_MEASURE_FT_EN_SHIFT                    8
#define MT6359_RGS_VEMC_OC_STATUS_ADDR                        \
	MT6359_VEMC_ANA_CON1
#define MT6359_RGS_VEMC_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VEMC_OC_STATUS_SHIFT                       9
#define MT6359_RG_VSIM1_VOCAL_ADDR                            \
	MT6359_VSIM1_ANA_CON0
#define MT6359_RG_VSIM1_VOCAL_MASK                            0xF
#define MT6359_RG_VSIM1_VOCAL_SHIFT                           0
#define MT6359_RG_VSIM1_VOSEL_ADDR                            \
	MT6359_VSIM1_ANA_CON0
#define MT6359_RG_VSIM1_VOSEL_MASK                            0xF
#define MT6359_RG_VSIM1_VOSEL_SHIFT                           8
#define MT6359_RG_VSIM1_NDIS_EN_ADDR                          \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_NDIS_EN_MASK                          0x1
#define MT6359_RG_VSIM1_NDIS_EN_SHIFT                         0
#define MT6359_RG_VSIM1_RSV_1_ADDR                            \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_RSV_1_MASK                            0x1
#define MT6359_RG_VSIM1_RSV_1_SHIFT                           2
#define MT6359_RG_VSIM1_OC_LP_EN_ADDR                         \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VSIM1_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VSIM1_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VSIM1_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VSIM1_ULP_BIASX2_EN_ADDR                    \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VSIM1_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VSIM1_MEASURE_FT_EN_ADDR                    \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RG_VSIM1_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VSIM1_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VSIM1_OC_STATUS_ADDR                       \
	MT6359_VSIM1_ANA_CON1
#define MT6359_RGS_VSIM1_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VSIM1_OC_STATUS_SHIFT                      7
#define MT6359_RG_VSIM2_VOCAL_ADDR                            \
	MT6359_VSIM2_ANA_CON0
#define MT6359_RG_VSIM2_VOCAL_MASK                            0xF
#define MT6359_RG_VSIM2_VOCAL_SHIFT                           0
#define MT6359_RG_VSIM2_VOSEL_ADDR                            \
	MT6359_VSIM2_ANA_CON0
#define MT6359_RG_VSIM2_VOSEL_MASK                            0xF
#define MT6359_RG_VSIM2_VOSEL_SHIFT                           8
#define MT6359_RG_VSIM2_NDIS_EN_ADDR                          \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_NDIS_EN_MASK                          0x1
#define MT6359_RG_VSIM2_NDIS_EN_SHIFT                         0
#define MT6359_RG_VSIM2_RSV_1_ADDR                            \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_RSV_1_MASK                            0x1
#define MT6359_RG_VSIM2_RSV_1_SHIFT                           2
#define MT6359_RG_VSIM2_OC_LP_EN_ADDR                         \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VSIM2_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VSIM2_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VSIM2_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VSIM2_ULP_BIASX2_EN_ADDR                    \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VSIM2_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VSIM2_MEASURE_FT_EN_ADDR                    \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RG_VSIM2_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VSIM2_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VSIM2_OC_STATUS_ADDR                       \
	MT6359_VSIM2_ANA_CON1
#define MT6359_RGS_VSIM2_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VSIM2_OC_STATUS_SHIFT                      7
#define MT6359_RG_VIO28_VOCAL_ADDR                            \
	MT6359_VIO28_ANA_CON0
#define MT6359_RG_VIO28_VOCAL_MASK                            0xF
#define MT6359_RG_VIO28_VOCAL_SHIFT                           0
#define MT6359_RG_VIO28_VOSEL_ADDR                            \
	MT6359_VIO28_ANA_CON0
#define MT6359_RG_VIO28_VOSEL_MASK                            0xF
#define MT6359_RG_VIO28_VOSEL_SHIFT                           8
#define MT6359_RG_VIO28_NDIS_EN_ADDR                          \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_NDIS_EN_MASK                          0x1
#define MT6359_RG_VIO28_NDIS_EN_SHIFT                         0
#define MT6359_RG_VIO28_RSV_1_ADDR                            \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_RSV_1_MASK                            0x1
#define MT6359_RG_VIO28_RSV_1_SHIFT                           2
#define MT6359_RG_VIO28_OC_LP_EN_ADDR                         \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VIO28_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VIO28_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VIO28_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VIO28_ULP_BIASX2_EN_ADDR                    \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VIO28_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VIO28_MEASURE_FT_EN_ADDR                    \
	MT6359_VIO28_ANA_CON1
#define MT6359_RG_VIO28_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VIO28_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VIO28_OC_STATUS_ADDR                       \
	MT6359_VIO28_ANA_CON1
#define MT6359_RGS_VIO28_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VIO28_OC_STATUS_SHIFT                      7
#define MT6359_RG_VIBR_VOCAL_ADDR                             \
	MT6359_VIBR_ANA_CON0
#define MT6359_RG_VIBR_VOCAL_MASK                             0xF
#define MT6359_RG_VIBR_VOCAL_SHIFT                            0
#define MT6359_RG_VIBR_VOSEL_ADDR                             \
	MT6359_VIBR_ANA_CON0
#define MT6359_RG_VIBR_VOSEL_MASK                             0xF
#define MT6359_RG_VIBR_VOSEL_SHIFT                            8
#define MT6359_RG_VIBR_NDIS_EN_ADDR                           \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_NDIS_EN_MASK                           0x1
#define MT6359_RG_VIBR_NDIS_EN_SHIFT                          0
#define MT6359_RG_VIBR_RSV_1_ADDR                             \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_RSV_1_MASK                             0x1
#define MT6359_RG_VIBR_RSV_1_SHIFT                            2
#define MT6359_RG_VIBR_OC_LP_EN_ADDR                          \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VIBR_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VIBR_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VIBR_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VIBR_ULP_BIASX2_EN_ADDR                     \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VIBR_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VIBR_MEASURE_FT_EN_ADDR                     \
	MT6359_VIBR_ANA_CON1
#define MT6359_RG_VIBR_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VIBR_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VIBR_OC_STATUS_ADDR                        \
	MT6359_VIBR_ANA_CON1
#define MT6359_RGS_VIBR_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VIBR_OC_STATUS_SHIFT                       7
#define MT6359_RG_ADLDO_RSV_ADDR                              \
	MT6359_ADLDO_ANA_CON0
#define MT6359_RG_ADLDO_RSV_MASK                              0x3F
#define MT6359_RG_ADLDO_RSV_SHIFT                             0
#define MT6359_LDO_ANA0_ELR_LEN_ADDR                          \
	MT6359_LDO_ANA0_ELR_NUM
#define MT6359_LDO_ANA0_ELR_LEN_MASK                          0xFF
#define MT6359_LDO_ANA0_ELR_LEN_SHIFT                         0
#define MT6359_RG_VFE28_VOTRIM_ADDR                           \
	MT6359_VFE28_ELR_0
#define MT6359_RG_VFE28_VOTRIM_MASK                           0xF
#define MT6359_RG_VFE28_VOTRIM_SHIFT                          0
#define MT6359_RG_VAUX18_VOTRIM_ADDR                          \
	MT6359_VFE28_ELR_0
#define MT6359_RG_VAUX18_VOTRIM_MASK                          0xF
#define MT6359_RG_VAUX18_VOTRIM_SHIFT                         4
#define MT6359_RG_VUSB_VOTRIM_ADDR                            \
	MT6359_VFE28_ELR_0
#define MT6359_RG_VUSB_VOTRIM_MASK                            0xF
#define MT6359_RG_VUSB_VOTRIM_SHIFT                           8
#define MT6359_RG_VBIF28_VOTRIM_ADDR                          \
	MT6359_VFE28_ELR_0
#define MT6359_RG_VBIF28_VOTRIM_MASK                          0xF
#define MT6359_RG_VBIF28_VOTRIM_SHIFT                         12
#define MT6359_RG_VCN33_1_VOTRIM_ADDR                         \
	MT6359_VFE28_ELR_1
#define MT6359_RG_VCN33_1_VOTRIM_MASK                         0xF
#define MT6359_RG_VCN33_1_VOTRIM_SHIFT                        0
#define MT6359_RG_VCN33_1_OC_TRIM_ADDR                        \
	MT6359_VFE28_ELR_1
#define MT6359_RG_VCN33_1_OC_TRIM_MASK                        0x7
#define MT6359_RG_VCN33_1_OC_TRIM_SHIFT                       4
#define MT6359_RG_VCN33_2_VOTRIM_ADDR                         \
	MT6359_VFE28_ELR_1
#define MT6359_RG_VCN33_2_VOTRIM_MASK                         0xF
#define MT6359_RG_VCN33_2_VOTRIM_SHIFT                        7
#define MT6359_RG_VCN33_2_OC_TRIM_ADDR                        \
	MT6359_VFE28_ELR_1
#define MT6359_RG_VCN33_2_OC_TRIM_MASK                        0x7
#define MT6359_RG_VCN33_2_OC_TRIM_SHIFT                       11
#define MT6359_RG_VEMC_VOTRIM_ADDR                            \
	MT6359_VFE28_ELR_2
#define MT6359_RG_VEMC_VOTRIM_MASK                            0xF
#define MT6359_RG_VEMC_VOTRIM_SHIFT                           0
#define MT6359_RG_VEMC_OC_TRIM_ADDR                           \
	MT6359_VFE28_ELR_2
#define MT6359_RG_VEMC_OC_TRIM_MASK                           0x7
#define MT6359_RG_VEMC_OC_TRIM_SHIFT                          4
#define MT6359_RG_VSIM1_VOTRIM_ADDR                           \
	MT6359_VFE28_ELR_2
#define MT6359_RG_VSIM1_VOTRIM_MASK                           0xF
#define MT6359_RG_VSIM1_VOTRIM_SHIFT                          7
#define MT6359_RG_VSIM1_OC_TRIM_ADDR                          \
	MT6359_VFE28_ELR_2
#define MT6359_RG_VSIM1_OC_TRIM_MASK                          0x7
#define MT6359_RG_VSIM1_OC_TRIM_SHIFT                         11
#define MT6359_RG_VSIM2_VOTRIM_ADDR                           \
	MT6359_VFE28_ELR_3
#define MT6359_RG_VSIM2_VOTRIM_MASK                           0xF
#define MT6359_RG_VSIM2_VOTRIM_SHIFT                          0
#define MT6359_RG_VSIM2_OC_TRIM_ADDR                          \
	MT6359_VFE28_ELR_3
#define MT6359_RG_VSIM2_OC_TRIM_MASK                          0x7
#define MT6359_RG_VSIM2_OC_TRIM_SHIFT                         4
#define MT6359_RG_VIO28_VOTRIM_ADDR                           \
	MT6359_VFE28_ELR_3
#define MT6359_RG_VIO28_VOTRIM_MASK                           0xF
#define MT6359_RG_VIO28_VOTRIM_SHIFT                          7
#define MT6359_RG_VIBR_VOTRIM_ADDR                            \
	MT6359_VFE28_ELR_3
#define MT6359_RG_VIBR_VOTRIM_MASK                            0xF
#define MT6359_RG_VIBR_VOTRIM_SHIFT                           11
#define MT6359_RG_VIBR_OC_TRIM_ADDR                           \
	MT6359_VFE28_ELR_4
#define MT6359_RG_VIBR_OC_TRIM_MASK                           0x7
#define MT6359_RG_VIBR_OC_TRIM_SHIFT                          0
#define MT6359_RG_VRTC28_BIAS_SEL_ADDR                        \
	MT6359_VFE28_ELR_4
#define MT6359_RG_VRTC28_BIAS_SEL_MASK                        0x1
#define MT6359_RG_VRTC28_BIAS_SEL_SHIFT                       3
#define MT6359_LDO_ANA1_ANA_ID_ADDR                           \
	MT6359_LDO_ANA1_DSN_ID
#define MT6359_LDO_ANA1_ANA_ID_MASK                           0xFF
#define MT6359_LDO_ANA1_ANA_ID_SHIFT                          0
#define MT6359_LDO_ANA1_DIG_ID_ADDR                           \
	MT6359_LDO_ANA1_DSN_ID
#define MT6359_LDO_ANA1_DIG_ID_MASK                           0xFF
#define MT6359_LDO_ANA1_DIG_ID_SHIFT                          8
#define MT6359_LDO_ANA1_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA1_DSN_REV0
#define MT6359_LDO_ANA1_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA1_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_ANA1_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA1_DSN_REV0
#define MT6359_LDO_ANA1_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA1_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_ANA1_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA1_DSN_REV0
#define MT6359_LDO_ANA1_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA1_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_ANA1_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA1_DSN_REV0
#define MT6359_LDO_ANA1_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA1_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_ANA1_DSN_CBS_ADDR                          \
	MT6359_LDO_ANA1_DSN_DBI
#define MT6359_LDO_ANA1_DSN_CBS_MASK                          0x3
#define MT6359_LDO_ANA1_DSN_CBS_SHIFT                         0
#define MT6359_LDO_ANA1_DSN_BIX_ADDR                          \
	MT6359_LDO_ANA1_DSN_DBI
#define MT6359_LDO_ANA1_DSN_BIX_MASK                          0x3
#define MT6359_LDO_ANA1_DSN_BIX_SHIFT                         2
#define MT6359_LDO_ANA1_DSN_ESP_ADDR                          \
	MT6359_LDO_ANA1_DSN_DBI
#define MT6359_LDO_ANA1_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_ANA1_DSN_ESP_SHIFT                         8
#define MT6359_LDO_ANA1_DSN_FPI_ADDR                          \
	MT6359_LDO_ANA1_DSN_FPI
#define MT6359_LDO_ANA1_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_ANA1_DSN_FPI_SHIFT                         0
#define MT6359_RG_VRF18_VOCAL_ADDR                            \
	MT6359_VRF18_ANA_CON0
#define MT6359_RG_VRF18_VOCAL_MASK                            0xF
#define MT6359_RG_VRF18_VOCAL_SHIFT                           0
#define MT6359_RG_VRF18_VOSEL_ADDR                            \
	MT6359_VRF18_ANA_CON0
#define MT6359_RG_VRF18_VOSEL_MASK                            0xF
#define MT6359_RG_VRF18_VOSEL_SHIFT                           8
#define MT6359_RG_VRF18_NDIS_EN_ADDR                          \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_NDIS_EN_MASK                          0x1
#define MT6359_RG_VRF18_NDIS_EN_SHIFT                         0
#define MT6359_RG_VRF18_RSV_0_ADDR                            \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_RSV_0_MASK                            0x3
#define MT6359_RG_VRF18_RSV_0_SHIFT                           2
#define MT6359_RG_VRF18_RSV_1_ADDR                            \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_RSV_1_MASK                            0x1
#define MT6359_RG_VRF18_RSV_1_SHIFT                           4
#define MT6359_RG_VRF18_OC_LP_EN_ADDR                         \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VRF18_OC_LP_EN_SHIFT                        5
#define MT6359_RG_VRF18_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VRF18_ULP_IQ_CLAMP_EN_SHIFT                 6
#define MT6359_RG_VRF18_ULP_BIASX2_EN_ADDR                    \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VRF18_ULP_BIASX2_EN_SHIFT                   7
#define MT6359_RG_VRF18_MEASURE_FT_EN_ADDR                    \
	MT6359_VRF18_ANA_CON1
#define MT6359_RG_VRF18_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VRF18_MEASURE_FT_EN_SHIFT                   8
#define MT6359_RGS_VRF18_OC_STATUS_ADDR                       \
	MT6359_VRF18_ANA_CON1
#define MT6359_RGS_VRF18_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VRF18_OC_STATUS_SHIFT                      9
#define MT6359_RG_VEFUSE_VOCAL_ADDR                           \
	MT6359_VEFUSE_ANA_CON0
#define MT6359_RG_VEFUSE_VOCAL_MASK                           0xF
#define MT6359_RG_VEFUSE_VOCAL_SHIFT                          0
#define MT6359_RG_VEFUSE_VOSEL_ADDR                           \
	MT6359_VEFUSE_ANA_CON0
#define MT6359_RG_VEFUSE_VOSEL_MASK                           0xF
#define MT6359_RG_VEFUSE_VOSEL_SHIFT                          8
#define MT6359_RG_VEFUSE_NDIS_EN_ADDR                         \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_NDIS_EN_MASK                         0x1
#define MT6359_RG_VEFUSE_NDIS_EN_SHIFT                        0
#define MT6359_RG_VEFUSE_RSV_1_ADDR                           \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_RSV_1_MASK                           0x1
#define MT6359_RG_VEFUSE_RSV_1_SHIFT                          2
#define MT6359_RG_VEFUSE_OC_LP_EN_ADDR                        \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_OC_LP_EN_MASK                        0x1
#define MT6359_RG_VEFUSE_OC_LP_EN_SHIFT                       3
#define MT6359_RG_VEFUSE_ULP_IQ_CLAMP_EN_ADDR                 \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_ULP_IQ_CLAMP_EN_MASK                 0x1
#define MT6359_RG_VEFUSE_ULP_IQ_CLAMP_EN_SHIFT                4
#define MT6359_RG_VEFUSE_ULP_BIASX2_EN_ADDR                   \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_ULP_BIASX2_EN_MASK                   0x1
#define MT6359_RG_VEFUSE_ULP_BIASX2_EN_SHIFT                  5
#define MT6359_RG_VEFUSE_MEASURE_FT_EN_ADDR                   \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RG_VEFUSE_MEASURE_FT_EN_MASK                   0x1
#define MT6359_RG_VEFUSE_MEASURE_FT_EN_SHIFT                  6
#define MT6359_RGS_VEFUSE_OC_STATUS_ADDR                      \
	MT6359_VEFUSE_ANA_CON1
#define MT6359_RGS_VEFUSE_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VEFUSE_OC_STATUS_SHIFT                     7
#define MT6359_RG_VCN18_VOCAL_ADDR                            \
	MT6359_VCN18_ANA_CON0
#define MT6359_RG_VCN18_VOCAL_MASK                            0xF
#define MT6359_RG_VCN18_VOCAL_SHIFT                           0
#define MT6359_RG_VCN18_VOSEL_ADDR                            \
	MT6359_VCN18_ANA_CON0
#define MT6359_RG_VCN18_VOSEL_MASK                            0xF
#define MT6359_RG_VCN18_VOSEL_SHIFT                           8
#define MT6359_RG_VCN18_NDIS_EN_ADDR                          \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_NDIS_EN_MASK                          0x1
#define MT6359_RG_VCN18_NDIS_EN_SHIFT                         0
#define MT6359_RG_VCN18_RSV_1_ADDR                            \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_RSV_1_MASK                            0x1
#define MT6359_RG_VCN18_RSV_1_SHIFT                           2
#define MT6359_RG_VCN18_OC_LP_EN_ADDR                         \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VCN18_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VCN18_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VCN18_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VCN18_ULP_BIASX2_EN_ADDR                    \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VCN18_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VCN18_MEASURE_FT_EN_ADDR                    \
	MT6359_VCN18_ANA_CON1
#define MT6359_RG_VCN18_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VCN18_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VCN18_OC_STATUS_ADDR                       \
	MT6359_VCN18_ANA_CON1
#define MT6359_RGS_VCN18_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VCN18_OC_STATUS_SHIFT                      7
#define MT6359_RG_VCAMIO_VOCAL_ADDR                           \
	MT6359_VCAMIO_ANA_CON0
#define MT6359_RG_VCAMIO_VOCAL_MASK                           0xF
#define MT6359_RG_VCAMIO_VOCAL_SHIFT                          0
#define MT6359_RG_VCAMIO_VOSEL_ADDR                           \
	MT6359_VCAMIO_ANA_CON0
#define MT6359_RG_VCAMIO_VOSEL_MASK                           0xF
#define MT6359_RG_VCAMIO_VOSEL_SHIFT                          8
#define MT6359_RG_VCAMIO_NDIS_EN_ADDR                         \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_NDIS_EN_MASK                         0x1
#define MT6359_RG_VCAMIO_NDIS_EN_SHIFT                        0
#define MT6359_RG_VCAMIO_RSV_1_ADDR                           \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_RSV_1_MASK                           0x1
#define MT6359_RG_VCAMIO_RSV_1_SHIFT                          2
#define MT6359_RG_VCAMIO_OC_LP_EN_ADDR                        \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_OC_LP_EN_MASK                        0x1
#define MT6359_RG_VCAMIO_OC_LP_EN_SHIFT                       3
#define MT6359_RG_VCAMIO_ULP_IQ_CLAMP_EN_ADDR                 \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_ULP_IQ_CLAMP_EN_MASK                 0x1
#define MT6359_RG_VCAMIO_ULP_IQ_CLAMP_EN_SHIFT                4
#define MT6359_RG_VCAMIO_ULP_BIASX2_EN_ADDR                   \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_ULP_BIASX2_EN_MASK                   0x1
#define MT6359_RG_VCAMIO_ULP_BIASX2_EN_SHIFT                  5
#define MT6359_RG_VCAMIO_MEASURE_FT_EN_ADDR                   \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RG_VCAMIO_MEASURE_FT_EN_MASK                   0x1
#define MT6359_RG_VCAMIO_MEASURE_FT_EN_SHIFT                  6
#define MT6359_RGS_VCAMIO_OC_STATUS_ADDR                      \
	MT6359_VCAMIO_ANA_CON1
#define MT6359_RGS_VCAMIO_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VCAMIO_OC_STATUS_SHIFT                     7
#define MT6359_RG_VAUD18_VOCAL_ADDR                           \
	MT6359_VAUD18_ANA_CON0
#define MT6359_RG_VAUD18_VOCAL_MASK                           0xF
#define MT6359_RG_VAUD18_VOCAL_SHIFT                          0
#define MT6359_RG_VAUD18_VOSEL_ADDR                           \
	MT6359_VAUD18_ANA_CON0
#define MT6359_RG_VAUD18_VOSEL_MASK                           0xF
#define MT6359_RG_VAUD18_VOSEL_SHIFT                          8
#define MT6359_RG_VAUD18_NDIS_EN_ADDR                         \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_NDIS_EN_MASK                         0x1
#define MT6359_RG_VAUD18_NDIS_EN_SHIFT                        0
#define MT6359_RG_VAUD18_RSV_1_ADDR                           \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_RSV_1_MASK                           0x1
#define MT6359_RG_VAUD18_RSV_1_SHIFT                          2
#define MT6359_RG_VAUD18_OC_LP_EN_ADDR                        \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_OC_LP_EN_MASK                        0x1
#define MT6359_RG_VAUD18_OC_LP_EN_SHIFT                       3
#define MT6359_RG_VAUD18_ULP_IQ_CLAMP_EN_ADDR                 \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_ULP_IQ_CLAMP_EN_MASK                 0x1
#define MT6359_RG_VAUD18_ULP_IQ_CLAMP_EN_SHIFT                4
#define MT6359_RG_VAUD18_ULP_BIASX2_EN_ADDR                   \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_ULP_BIASX2_EN_MASK                   0x1
#define MT6359_RG_VAUD18_ULP_BIASX2_EN_SHIFT                  5
#define MT6359_RG_VAUD18_MEASURE_FT_EN_ADDR                   \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RG_VAUD18_MEASURE_FT_EN_MASK                   0x1
#define MT6359_RG_VAUD18_MEASURE_FT_EN_SHIFT                  6
#define MT6359_RGS_VAUD18_OC_STATUS_ADDR                      \
	MT6359_VAUD18_ANA_CON1
#define MT6359_RGS_VAUD18_OC_STATUS_MASK                      0x1
#define MT6359_RGS_VAUD18_OC_STATUS_SHIFT                     7
#define MT6359_RG_VIO18_VOCAL_ADDR                            \
	MT6359_VIO18_ANA_CON0
#define MT6359_RG_VIO18_VOCAL_MASK                            0xF
#define MT6359_RG_VIO18_VOCAL_SHIFT                           0
#define MT6359_RG_VIO18_VOSEL_ADDR                            \
	MT6359_VIO18_ANA_CON0
#define MT6359_RG_VIO18_VOSEL_MASK                            0xF
#define MT6359_RG_VIO18_VOSEL_SHIFT                           8
#define MT6359_RG_VIO18_NDIS_EN_ADDR                          \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_NDIS_EN_MASK                          0x1
#define MT6359_RG_VIO18_NDIS_EN_SHIFT                         0
#define MT6359_RG_VIO18_RSV_1_ADDR                            \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_RSV_1_MASK                            0x1
#define MT6359_RG_VIO18_RSV_1_SHIFT                           2
#define MT6359_RG_VIO18_OC_LP_EN_ADDR                         \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VIO18_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VIO18_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VIO18_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VIO18_ULP_BIASX2_EN_ADDR                    \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VIO18_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VIO18_MEASURE_FT_EN_ADDR                    \
	MT6359_VIO18_ANA_CON1
#define MT6359_RG_VIO18_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VIO18_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VIO18_OC_STATUS_ADDR                       \
	MT6359_VIO18_ANA_CON1
#define MT6359_RGS_VIO18_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VIO18_OC_STATUS_SHIFT                      7
#define MT6359_RG_VM18_VOCAL_ADDR                             \
	MT6359_VM18_ANA_CON0
#define MT6359_RG_VM18_VOCAL_MASK                             0xF
#define MT6359_RG_VM18_VOCAL_SHIFT                            0
#define MT6359_RG_VM18_VOSEL_ADDR                             \
	MT6359_VM18_ANA_CON0
#define MT6359_RG_VM18_VOSEL_MASK                             0xF
#define MT6359_RG_VM18_VOSEL_SHIFT                            8
#define MT6359_RG_VM18_NDIS_EN_ADDR                           \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_NDIS_EN_MASK                           0x1
#define MT6359_RG_VM18_NDIS_EN_SHIFT                          0
#define MT6359_RG_VM18_RSV_1_ADDR                             \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_RSV_1_MASK                             0x1
#define MT6359_RG_VM18_RSV_1_SHIFT                            2
#define MT6359_RG_VM18_OC_LP_EN_ADDR                          \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VM18_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VM18_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VM18_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VM18_ULP_BIASX2_EN_ADDR                     \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VM18_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VM18_MEASURE_FT_EN_ADDR                     \
	MT6359_VM18_ANA_CON1
#define MT6359_RG_VM18_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VM18_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VM18_OC_STATUS_ADDR                        \
	MT6359_VM18_ANA_CON1
#define MT6359_RGS_VM18_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VM18_OC_STATUS_SHIFT                       7
#define MT6359_RG_VUFS_VOCAL_ADDR                             \
	MT6359_VUFS_ANA_CON0
#define MT6359_RG_VUFS_VOCAL_MASK                             0xF
#define MT6359_RG_VUFS_VOCAL_SHIFT                            0
#define MT6359_RG_VUFS_VOSEL_ADDR                             \
	MT6359_VUFS_ANA_CON0
#define MT6359_RG_VUFS_VOSEL_MASK                             0xF
#define MT6359_RG_VUFS_VOSEL_SHIFT                            8
#define MT6359_RG_VUFS_NDIS_EN_ADDR                           \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_NDIS_EN_MASK                           0x1
#define MT6359_RG_VUFS_NDIS_EN_SHIFT                          0
#define MT6359_RG_VUFS_RSV_1_ADDR                             \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_RSV_1_MASK                             0x1
#define MT6359_RG_VUFS_RSV_1_SHIFT                            2
#define MT6359_RG_VUFS_OC_LP_EN_ADDR                          \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VUFS_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VUFS_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VUFS_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VUFS_ULP_BIASX2_EN_ADDR                     \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VUFS_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VUFS_MEASURE_FT_EN_ADDR                     \
	MT6359_VUFS_ANA_CON1
#define MT6359_RG_VUFS_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VUFS_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VUFS_OC_STATUS_ADDR                        \
	MT6359_VUFS_ANA_CON1
#define MT6359_RGS_VUFS_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VUFS_OC_STATUS_SHIFT                       7
#define MT6359_RG_SLDO20_RSV_ADDR                             \
	MT6359_SLDO20_ANA_CON0
#define MT6359_RG_SLDO20_RSV_MASK                             0x3F
#define MT6359_RG_SLDO20_RSV_SHIFT                            0
#define MT6359_RG_VRF12_VOCAL_ADDR                            \
	MT6359_VRF12_ANA_CON0
#define MT6359_RG_VRF12_VOCAL_MASK                            0xF
#define MT6359_RG_VRF12_VOCAL_SHIFT                           0
#define MT6359_RG_VRF12_VOSEL_ADDR                            \
	MT6359_VRF12_ANA_CON0
#define MT6359_RG_VRF12_VOSEL_MASK                            0xF
#define MT6359_RG_VRF12_VOSEL_SHIFT                           8
#define MT6359_RG_VRF12_NDIS_EN_ADDR                          \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_NDIS_EN_MASK                          0x1
#define MT6359_RG_VRF12_NDIS_EN_SHIFT                         0
#define MT6359_RG_VRF12_RSV_0_ADDR                            \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_RSV_0_MASK                            0x3
#define MT6359_RG_VRF12_RSV_0_SHIFT                           2
#define MT6359_RG_VRF12_RSV_1_ADDR                            \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_RSV_1_MASK                            0x1
#define MT6359_RG_VRF12_RSV_1_SHIFT                           4
#define MT6359_RG_VRF12_OC_LP_EN_ADDR                         \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VRF12_OC_LP_EN_SHIFT                        5
#define MT6359_RG_VRF12_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VRF12_ULP_IQ_CLAMP_EN_SHIFT                 6
#define MT6359_RG_VRF12_ULP_BIASX2_EN_ADDR                    \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VRF12_ULP_BIASX2_EN_SHIFT                   7
#define MT6359_RG_VRF12_MEASURE_FT_EN_ADDR                    \
	MT6359_VRF12_ANA_CON1
#define MT6359_RG_VRF12_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VRF12_MEASURE_FT_EN_SHIFT                   8
#define MT6359_RGS_VRF12_OC_STATUS_ADDR                       \
	MT6359_VRF12_ANA_CON1
#define MT6359_RGS_VRF12_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VRF12_OC_STATUS_SHIFT                      9
#define MT6359_RG_VCN13_VOCAL_ADDR                            \
	MT6359_VCN13_ANA_CON0
#define MT6359_RG_VCN13_VOCAL_MASK                            0xF
#define MT6359_RG_VCN13_VOCAL_SHIFT                           0
#define MT6359_RG_VCN13_VOSEL_ADDR                            \
	MT6359_VCN13_ANA_CON0
#define MT6359_RG_VCN13_VOSEL_MASK                            0xF
#define MT6359_RG_VCN13_VOSEL_SHIFT                           8
#define MT6359_RG_VCN13_NDIS_EN_ADDR                          \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_NDIS_EN_MASK                          0x1
#define MT6359_RG_VCN13_NDIS_EN_SHIFT                         0
#define MT6359_RG_VCN13_RSV_0_ADDR                            \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_RSV_0_MASK                            0x3
#define MT6359_RG_VCN13_RSV_0_SHIFT                           2
#define MT6359_RG_VCN13_RSV_1_ADDR                            \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_RSV_1_MASK                            0x1
#define MT6359_RG_VCN13_RSV_1_SHIFT                           4
#define MT6359_RG_VCN13_OC_LP_EN_ADDR                         \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VCN13_OC_LP_EN_SHIFT                        5
#define MT6359_RG_VCN13_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VCN13_ULP_IQ_CLAMP_EN_SHIFT                 6
#define MT6359_RG_VCN13_ULP_BIASX2_EN_ADDR                    \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VCN13_ULP_BIASX2_EN_SHIFT                   7
#define MT6359_RG_VCN13_MEASURE_FT_EN_ADDR                    \
	MT6359_VCN13_ANA_CON1
#define MT6359_RG_VCN13_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VCN13_MEASURE_FT_EN_SHIFT                   8
#define MT6359_RGS_VCN13_OC_STATUS_ADDR                       \
	MT6359_VCN13_ANA_CON1
#define MT6359_RGS_VCN13_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VCN13_OC_STATUS_SHIFT                      9
#define MT6359_RG_VA09_VOCAL_ADDR                             \
	MT6359_VA09_ANA_CON0
#define MT6359_RG_VA09_VOCAL_MASK                             0xF
#define MT6359_RG_VA09_VOCAL_SHIFT                            0
#define MT6359_RG_VA09_VOSEL_ADDR                             \
	MT6359_VA09_ANA_CON0
#define MT6359_RG_VA09_VOSEL_MASK                             0xF
#define MT6359_RG_VA09_VOSEL_SHIFT                            8
#define MT6359_RG_VA09_NDIS_EN_ADDR                           \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_NDIS_EN_MASK                           0x1
#define MT6359_RG_VA09_NDIS_EN_SHIFT                          0
#define MT6359_RG_VA09_RSV_1_ADDR                             \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_RSV_1_MASK                             0x1
#define MT6359_RG_VA09_RSV_1_SHIFT                            2
#define MT6359_RG_VA09_OC_LP_EN_ADDR                          \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VA09_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VA09_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VA09_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VA09_ULP_BIASX2_EN_ADDR                     \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VA09_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VA09_MEASURE_FT_EN_ADDR                     \
	MT6359_VA09_ANA_CON1
#define MT6359_RG_VA09_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VA09_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VA09_OC_STATUS_ADDR                        \
	MT6359_VA09_ANA_CON1
#define MT6359_RGS_VA09_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VA09_OC_STATUS_SHIFT                       7
#define MT6359_RG_VA12_VOCAL_ADDR                             \
	MT6359_VA12_ANA_CON0
#define MT6359_RG_VA12_VOCAL_MASK                             0xF
#define MT6359_RG_VA12_VOCAL_SHIFT                            0
#define MT6359_RG_VA12_VOSEL_ADDR                             \
	MT6359_VA12_ANA_CON0
#define MT6359_RG_VA12_VOSEL_MASK                             0xF
#define MT6359_RG_VA12_VOSEL_SHIFT                            8
#define MT6359_RG_VA12_NDIS_EN_ADDR                           \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_NDIS_EN_MASK                           0x1
#define MT6359_RG_VA12_NDIS_EN_SHIFT                          0
#define MT6359_RG_VA12_RSV_1_ADDR                             \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_RSV_1_MASK                             0x1
#define MT6359_RG_VA12_RSV_1_SHIFT                            2
#define MT6359_RG_VA12_OC_LP_EN_ADDR                          \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_OC_LP_EN_MASK                          0x1
#define MT6359_RG_VA12_OC_LP_EN_SHIFT                         3
#define MT6359_RG_VA12_ULP_IQ_CLAMP_EN_ADDR                   \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_ULP_IQ_CLAMP_EN_MASK                   0x1
#define MT6359_RG_VA12_ULP_IQ_CLAMP_EN_SHIFT                  4
#define MT6359_RG_VA12_ULP_BIASX2_EN_ADDR                     \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_ULP_BIASX2_EN_MASK                     0x1
#define MT6359_RG_VA12_ULP_BIASX2_EN_SHIFT                    5
#define MT6359_RG_VA12_MEASURE_FT_EN_ADDR                     \
	MT6359_VA12_ANA_CON1
#define MT6359_RG_VA12_MEASURE_FT_EN_MASK                     0x1
#define MT6359_RG_VA12_MEASURE_FT_EN_SHIFT                    6
#define MT6359_RGS_VA12_OC_STATUS_ADDR                        \
	MT6359_VA12_ANA_CON1
#define MT6359_RGS_VA12_OC_STATUS_MASK                        0x1
#define MT6359_RGS_VA12_OC_STATUS_SHIFT                       7
#define MT6359_RG_VSRAM_PROC1_NDIS_EN_ADDR                    \
	MT6359_VSRAM_PROC1_ANA_CON0
#define MT6359_RG_VSRAM_PROC1_NDIS_EN_MASK                    0x1
#define MT6359_RG_VSRAM_PROC1_NDIS_EN_SHIFT                   4
#define MT6359_RG_VSRAM_PROC1_NDIS_PLCUR_ADDR                 \
	MT6359_VSRAM_PROC1_ANA_CON0
#define MT6359_RG_VSRAM_PROC1_NDIS_PLCUR_MASK                 0x3
#define MT6359_RG_VSRAM_PROC1_NDIS_PLCUR_SHIFT                5
#define MT6359_RG_VSRAM_PROC1_OC_LP_EN_ADDR                   \
	MT6359_VSRAM_PROC1_ANA_CON0
#define MT6359_RG_VSRAM_PROC1_OC_LP_EN_MASK                   0x1
#define MT6359_RG_VSRAM_PROC1_OC_LP_EN_SHIFT                  7
#define MT6359_RG_VSRAM_PROC1_RSV_H_ADDR                      \
	MT6359_VSRAM_PROC1_ANA_CON0
#define MT6359_RG_VSRAM_PROC1_RSV_H_MASK                      0xF
#define MT6359_RG_VSRAM_PROC1_RSV_H_SHIFT                     8
#define MT6359_RG_VSRAM_PROC1_RSV_L_ADDR                      \
	MT6359_VSRAM_PROC1_ANA_CON0
#define MT6359_RG_VSRAM_PROC1_RSV_L_MASK                      0xF
#define MT6359_RG_VSRAM_PROC1_RSV_L_SHIFT                     12
#define MT6359_RG_VSRAM_PROC1_ULP_IQ_CLAMP_EN_ADDR            \
	MT6359_VSRAM_PROC1_ANA_CON1
#define MT6359_RG_VSRAM_PROC1_ULP_IQ_CLAMP_EN_MASK            0x1
#define MT6359_RG_VSRAM_PROC1_ULP_IQ_CLAMP_EN_SHIFT           0
#define MT6359_RG_VSRAM_PROC1_ULP_BIASX2_EN_ADDR              \
	MT6359_VSRAM_PROC1_ANA_CON1
#define MT6359_RG_VSRAM_PROC1_ULP_BIASX2_EN_MASK              0x1
#define MT6359_RG_VSRAM_PROC1_ULP_BIASX2_EN_SHIFT             1
#define MT6359_RG_VSRAM_PROC1_MEASURE_FT_EN_ADDR              \
	MT6359_VSRAM_PROC1_ANA_CON1
#define MT6359_RG_VSRAM_PROC1_MEASURE_FT_EN_MASK              0x1
#define MT6359_RG_VSRAM_PROC1_MEASURE_FT_EN_SHIFT             2
#define MT6359_RGS_VSRAM_PROC1_OC_STATUS_ADDR                 \
	MT6359_VSRAM_PROC1_ANA_CON1
#define MT6359_RGS_VSRAM_PROC1_OC_STATUS_MASK                 0x1
#define MT6359_RGS_VSRAM_PROC1_OC_STATUS_SHIFT                3
#define MT6359_RG_VSRAM_PROC2_NDIS_EN_ADDR                    \
	MT6359_VSRAM_PROC2_ANA_CON0
#define MT6359_RG_VSRAM_PROC2_NDIS_EN_MASK                    0x1
#define MT6359_RG_VSRAM_PROC2_NDIS_EN_SHIFT                   4
#define MT6359_RG_VSRAM_PROC2_NDIS_PLCUR_ADDR                 \
	MT6359_VSRAM_PROC2_ANA_CON0
#define MT6359_RG_VSRAM_PROC2_NDIS_PLCUR_MASK                 0x3
#define MT6359_RG_VSRAM_PROC2_NDIS_PLCUR_SHIFT                5
#define MT6359_RG_VSRAM_PROC2_OC_LP_EN_ADDR                   \
	MT6359_VSRAM_PROC2_ANA_CON0
#define MT6359_RG_VSRAM_PROC2_OC_LP_EN_MASK                   0x1
#define MT6359_RG_VSRAM_PROC2_OC_LP_EN_SHIFT                  7
#define MT6359_RG_VSRAM_PROC2_RSV_H_ADDR                      \
	MT6359_VSRAM_PROC2_ANA_CON0
#define MT6359_RG_VSRAM_PROC2_RSV_H_MASK                      0xF
#define MT6359_RG_VSRAM_PROC2_RSV_H_SHIFT                     8
#define MT6359_RG_VSRAM_PROC2_RSV_L_ADDR                      \
	MT6359_VSRAM_PROC2_ANA_CON0
#define MT6359_RG_VSRAM_PROC2_RSV_L_MASK                      0xF
#define MT6359_RG_VSRAM_PROC2_RSV_L_SHIFT                     12
#define MT6359_RG_VSRAM_PROC2_ULP_IQ_CLAMP_EN_ADDR            \
	MT6359_VSRAM_PROC2_ANA_CON1
#define MT6359_RG_VSRAM_PROC2_ULP_IQ_CLAMP_EN_MASK            0x1
#define MT6359_RG_VSRAM_PROC2_ULP_IQ_CLAMP_EN_SHIFT           0
#define MT6359_RG_VSRAM_PROC2_ULP_BIASX2_EN_ADDR              \
	MT6359_VSRAM_PROC2_ANA_CON1
#define MT6359_RG_VSRAM_PROC2_ULP_BIASX2_EN_MASK              0x1
#define MT6359_RG_VSRAM_PROC2_ULP_BIASX2_EN_SHIFT             1
#define MT6359_RG_VSRAM_PROC2_MEASURE_FT_EN_ADDR              \
	MT6359_VSRAM_PROC2_ANA_CON1
#define MT6359_RG_VSRAM_PROC2_MEASURE_FT_EN_MASK              0x1
#define MT6359_RG_VSRAM_PROC2_MEASURE_FT_EN_SHIFT             2
#define MT6359_RGS_VSRAM_PROC2_OC_STATUS_ADDR                 \
	MT6359_VSRAM_PROC2_ANA_CON1
#define MT6359_RGS_VSRAM_PROC2_OC_STATUS_MASK                 0x1
#define MT6359_RGS_VSRAM_PROC2_OC_STATUS_SHIFT                3
#define MT6359_RG_VSRAM_OTHERS_NDIS_EN_ADDR                   \
	MT6359_VSRAM_OTHERS_ANA_CON0
#define MT6359_RG_VSRAM_OTHERS_NDIS_EN_MASK                   0x1
#define MT6359_RG_VSRAM_OTHERS_NDIS_EN_SHIFT                  4
#define MT6359_RG_VSRAM_OTHERS_NDIS_PLCUR_ADDR                \
	MT6359_VSRAM_OTHERS_ANA_CON0
#define MT6359_RG_VSRAM_OTHERS_NDIS_PLCUR_MASK                0x3
#define MT6359_RG_VSRAM_OTHERS_NDIS_PLCUR_SHIFT               5
#define MT6359_RG_VSRAM_OTHERS_OC_LP_EN_ADDR                  \
	MT6359_VSRAM_OTHERS_ANA_CON0
#define MT6359_RG_VSRAM_OTHERS_OC_LP_EN_MASK                  0x1
#define MT6359_RG_VSRAM_OTHERS_OC_LP_EN_SHIFT                 7
#define MT6359_RG_VSRAM_OTHERS_RSV_H_ADDR                     \
	MT6359_VSRAM_OTHERS_ANA_CON0
#define MT6359_RG_VSRAM_OTHERS_RSV_H_MASK                     0xF
#define MT6359_RG_VSRAM_OTHERS_RSV_H_SHIFT                    8
#define MT6359_RG_VSRAM_OTHERS_RSV_L_ADDR                     \
	MT6359_VSRAM_OTHERS_ANA_CON0
#define MT6359_RG_VSRAM_OTHERS_RSV_L_MASK                     0xF
#define MT6359_RG_VSRAM_OTHERS_RSV_L_SHIFT                    12
#define MT6359_RG_VSRAM_OTHERS_ULP_IQ_CLAMP_EN_ADDR           \
	MT6359_VSRAM_OTHERS_ANA_CON1
#define MT6359_RG_VSRAM_OTHERS_ULP_IQ_CLAMP_EN_MASK           0x1
#define MT6359_RG_VSRAM_OTHERS_ULP_IQ_CLAMP_EN_SHIFT          0
#define MT6359_RG_VSRAM_OTHERS_ULP_BIASX2_EN_ADDR             \
	MT6359_VSRAM_OTHERS_ANA_CON1
#define MT6359_RG_VSRAM_OTHERS_ULP_BIASX2_EN_MASK             0x1
#define MT6359_RG_VSRAM_OTHERS_ULP_BIASX2_EN_SHIFT            1
#define MT6359_RG_VSRAM_OTHERS_MEASURE_FT_EN_ADDR             \
	MT6359_VSRAM_OTHERS_ANA_CON1
#define MT6359_RG_VSRAM_OTHERS_MEASURE_FT_EN_MASK             0x1
#define MT6359_RG_VSRAM_OTHERS_MEASURE_FT_EN_SHIFT            2
#define MT6359_RGS_VSRAM_OTHERS_OC_STATUS_ADDR                \
	MT6359_VSRAM_OTHERS_ANA_CON1
#define MT6359_RGS_VSRAM_OTHERS_OC_STATUS_MASK                0x1
#define MT6359_RGS_VSRAM_OTHERS_OC_STATUS_SHIFT               3
#define MT6359_RG_VSRAM_MD_NDIS_EN_ADDR                       \
	MT6359_VSRAM_MD_ANA_CON0
#define MT6359_RG_VSRAM_MD_NDIS_EN_MASK                       0x1
#define MT6359_RG_VSRAM_MD_NDIS_EN_SHIFT                      4
#define MT6359_RG_VSRAM_MD_NDIS_PLCUR_ADDR                    \
	MT6359_VSRAM_MD_ANA_CON0
#define MT6359_RG_VSRAM_MD_NDIS_PLCUR_MASK                    0x3
#define MT6359_RG_VSRAM_MD_NDIS_PLCUR_SHIFT                   5
#define MT6359_RG_VSRAM_MD_OC_LP_EN_ADDR                      \
	MT6359_VSRAM_MD_ANA_CON0
#define MT6359_RG_VSRAM_MD_OC_LP_EN_MASK                      0x1
#define MT6359_RG_VSRAM_MD_OC_LP_EN_SHIFT                     7
#define MT6359_RG_VSRAM_MD_RSV_H_ADDR                         \
	MT6359_VSRAM_MD_ANA_CON0
#define MT6359_RG_VSRAM_MD_RSV_H_MASK                         0xF
#define MT6359_RG_VSRAM_MD_RSV_H_SHIFT                        8
#define MT6359_RG_VSRAM_MD_RSV_L_ADDR                         \
	MT6359_VSRAM_MD_ANA_CON0
#define MT6359_RG_VSRAM_MD_RSV_L_MASK                         0xF
#define MT6359_RG_VSRAM_MD_RSV_L_SHIFT                        12
#define MT6359_RG_VSRAM_MD_ULP_IQ_CLAMP_EN_ADDR               \
	MT6359_VSRAM_MD_ANA_CON1
#define MT6359_RG_VSRAM_MD_ULP_IQ_CLAMP_EN_MASK               0x1
#define MT6359_RG_VSRAM_MD_ULP_IQ_CLAMP_EN_SHIFT              0
#define MT6359_RG_VSRAM_MD_ULP_BIASX2_EN_ADDR                 \
	MT6359_VSRAM_MD_ANA_CON1
#define MT6359_RG_VSRAM_MD_ULP_BIASX2_EN_MASK                 0x1
#define MT6359_RG_VSRAM_MD_ULP_BIASX2_EN_SHIFT                1
#define MT6359_RG_VSRAM_MD_MEASURE_FT_EN_ADDR                 \
	MT6359_VSRAM_MD_ANA_CON1
#define MT6359_RG_VSRAM_MD_MEASURE_FT_EN_MASK                 0x1
#define MT6359_RG_VSRAM_MD_MEASURE_FT_EN_SHIFT                2
#define MT6359_RGS_VSRAM_MD_OC_STATUS_ADDR                    \
	MT6359_VSRAM_MD_ANA_CON1
#define MT6359_RGS_VSRAM_MD_OC_STATUS_MASK                    0x1
#define MT6359_RGS_VSRAM_MD_OC_STATUS_SHIFT                   3
#define MT6359_RG_SLDO14_RSV_ADDR                             \
	MT6359_SLDO14_ANA_CON0
#define MT6359_RG_SLDO14_RSV_MASK                             0x3F
#define MT6359_RG_SLDO14_RSV_SHIFT                            0
#define MT6359_LDO_ANA1_ELR_LEN_ADDR                          \
	MT6359_LDO_ANA1_ELR_NUM
#define MT6359_LDO_ANA1_ELR_LEN_MASK                          0xFF
#define MT6359_LDO_ANA1_ELR_LEN_SHIFT                         0
#define MT6359_RG_VRF18_VOTRIM_ADDR                           \
	MT6359_VRF18_ELR_0
#define MT6359_RG_VRF18_VOTRIM_MASK                           0xF
#define MT6359_RG_VRF18_VOTRIM_SHIFT                          0
#define MT6359_RG_VEFUSE_VOTRIM_ADDR                          \
	MT6359_VRF18_ELR_0
#define MT6359_RG_VEFUSE_VOTRIM_MASK                          0xF
#define MT6359_RG_VEFUSE_VOTRIM_SHIFT                         4
#define MT6359_RG_VCN18_VOTRIM_ADDR                           \
	MT6359_VRF18_ELR_0
#define MT6359_RG_VCN18_VOTRIM_MASK                           0xF
#define MT6359_RG_VCN18_VOTRIM_SHIFT                          8
#define MT6359_RG_VCN18_OC_TRIM_ADDR                          \
	MT6359_VRF18_ELR_0
#define MT6359_RG_VCN18_OC_TRIM_MASK                          0x7
#define MT6359_RG_VCN18_OC_TRIM_SHIFT                         12
#define MT6359_RG_VCAMIO_VOTRIM_ADDR                          \
	MT6359_VRF18_ELR_1
#define MT6359_RG_VCAMIO_VOTRIM_MASK                          0xF
#define MT6359_RG_VCAMIO_VOTRIM_SHIFT                         0
#define MT6359_RG_VAUD18_VOTRIM_ADDR                          \
	MT6359_VRF18_ELR_1
#define MT6359_RG_VAUD18_VOTRIM_MASK                          0xF
#define MT6359_RG_VAUD18_VOTRIM_SHIFT                         4
#define MT6359_RG_VIO18_VOTRIM_ADDR                           \
	MT6359_VRF18_ELR_1
#define MT6359_RG_VIO18_VOTRIM_MASK                           0xF
#define MT6359_RG_VIO18_VOTRIM_SHIFT                          8
#define MT6359_RG_VM18_VOTRIM_ADDR                            \
	MT6359_VRF18_ELR_1
#define MT6359_RG_VM18_VOTRIM_MASK                            0xF
#define MT6359_RG_VM18_VOTRIM_SHIFT                           12
#define MT6359_RG_VUFS_VOTRIM_ADDR                            \
	MT6359_VRF18_ELR_2
#define MT6359_RG_VUFS_VOTRIM_MASK                            0xF
#define MT6359_RG_VUFS_VOTRIM_SHIFT                           0
#define MT6359_RG_VUFS_OC_TRIM_ADDR                           \
	MT6359_VRF18_ELR_2
#define MT6359_RG_VUFS_OC_TRIM_MASK                           0x7
#define MT6359_RG_VUFS_OC_TRIM_SHIFT                          4
#define MT6359_RG_VRF12_VOTRIM_ADDR                           \
	MT6359_VRF18_ELR_2
#define MT6359_RG_VRF12_VOTRIM_MASK                           0xF
#define MT6359_RG_VRF12_VOTRIM_SHIFT                          7
#define MT6359_RG_VCN13_VOTRIM_ADDR                           \
	MT6359_VRF18_ELR_2
#define MT6359_RG_VCN13_VOTRIM_MASK                           0xF
#define MT6359_RG_VCN13_VOTRIM_SHIFT                          11
#define MT6359_RG_VA09_VOTRIM_ADDR                            \
	MT6359_VRF18_ELR_3
#define MT6359_RG_VA09_VOTRIM_MASK                            0xF
#define MT6359_RG_VA09_VOTRIM_SHIFT                           0
#define MT6359_RG_VA12_VOTRIM_ADDR                            \
	MT6359_VRF18_ELR_3
#define MT6359_RG_VA12_VOTRIM_MASK                            0xF
#define MT6359_RG_VA12_VOTRIM_SHIFT                           4
#define MT6359_LDO_ANA2_ANA_ID_ADDR                           \
	MT6359_LDO_ANA2_DSN_ID
#define MT6359_LDO_ANA2_ANA_ID_MASK                           0xFF
#define MT6359_LDO_ANA2_ANA_ID_SHIFT                          0
#define MT6359_LDO_ANA2_DIG_ID_ADDR                           \
	MT6359_LDO_ANA2_DSN_ID
#define MT6359_LDO_ANA2_DIG_ID_MASK                           0xFF
#define MT6359_LDO_ANA2_DIG_ID_SHIFT                          8
#define MT6359_LDO_ANA2_ANA_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA2_DSN_REV0
#define MT6359_LDO_ANA2_ANA_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA2_ANA_MINOR_REV_SHIFT                   0
#define MT6359_LDO_ANA2_ANA_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA2_DSN_REV0
#define MT6359_LDO_ANA2_ANA_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA2_ANA_MAJOR_REV_SHIFT                   4
#define MT6359_LDO_ANA2_DIG_MINOR_REV_ADDR                    \
	MT6359_LDO_ANA2_DSN_REV0
#define MT6359_LDO_ANA2_DIG_MINOR_REV_MASK                    0xF
#define MT6359_LDO_ANA2_DIG_MINOR_REV_SHIFT                   8
#define MT6359_LDO_ANA2_DIG_MAJOR_REV_ADDR                    \
	MT6359_LDO_ANA2_DSN_REV0
#define MT6359_LDO_ANA2_DIG_MAJOR_REV_MASK                    0xF
#define MT6359_LDO_ANA2_DIG_MAJOR_REV_SHIFT                   12
#define MT6359_LDO_ANA2_DSN_CBS_ADDR                          \
	MT6359_LDO_ANA2_DSN_DBI
#define MT6359_LDO_ANA2_DSN_CBS_MASK                          0x3
#define MT6359_LDO_ANA2_DSN_CBS_SHIFT                         0
#define MT6359_LDO_ANA2_DSN_BIX_ADDR                          \
	MT6359_LDO_ANA2_DSN_DBI
#define MT6359_LDO_ANA2_DSN_BIX_MASK                          0x3
#define MT6359_LDO_ANA2_DSN_BIX_SHIFT                         2
#define MT6359_LDO_ANA2_DSN_ESP_ADDR                          \
	MT6359_LDO_ANA2_DSN_DBI
#define MT6359_LDO_ANA2_DSN_ESP_MASK                          0xFF
#define MT6359_LDO_ANA2_DSN_ESP_SHIFT                         8
#define MT6359_LDO_ANA2_DSN_FPI_ADDR                          \
	MT6359_LDO_ANA2_DSN_FPI
#define MT6359_LDO_ANA2_DSN_FPI_MASK                          0xFF
#define MT6359_LDO_ANA2_DSN_FPI_SHIFT                         0
#define MT6359_RG_VXO22_VOCAL_ADDR                            \
	MT6359_VXO22_ANA_CON0
#define MT6359_RG_VXO22_VOCAL_MASK                            0xF
#define MT6359_RG_VXO22_VOCAL_SHIFT                           0
#define MT6359_RG_VXO22_VOSEL_ADDR                            \
	MT6359_VXO22_ANA_CON0
#define MT6359_RG_VXO22_VOSEL_MASK                            0xF
#define MT6359_RG_VXO22_VOSEL_SHIFT                           8
#define MT6359_RG_VXO22_RSV_1_ADDR                            \
	MT6359_VXO22_ANA_CON1
#define MT6359_RG_VXO22_RSV_1_MASK                            0x1
#define MT6359_RG_VXO22_RSV_1_SHIFT                           2
#define MT6359_RG_VXO22_OC_LP_EN_ADDR                         \
	MT6359_VXO22_ANA_CON1
#define MT6359_RG_VXO22_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VXO22_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VXO22_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VXO22_ANA_CON1
#define MT6359_RG_VXO22_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VXO22_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VXO22_ULP_BIASX2_EN_ADDR                    \
	MT6359_VXO22_ANA_CON1
#define MT6359_RG_VXO22_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VXO22_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VXO22_MEASURE_FT_EN_ADDR                    \
	MT6359_VXO22_ANA_CON1
#define MT6359_RG_VXO22_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VXO22_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VXO22_OC_STATUS_ADDR                       \
	MT6359_VXO22_ANA_CON1
#define MT6359_RGS_VXO22_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VXO22_OC_STATUS_SHIFT                      7
#define MT6359_RG_VRFCK_VOCAL_ADDR                            \
	MT6359_VRFCK_ANA_CON0
#define MT6359_RG_VRFCK_VOCAL_MASK                            0xF
#define MT6359_RG_VRFCK_VOCAL_SHIFT                           0
#define MT6359_RG_VRFCK_VOSEL_ADDR                            \
	MT6359_VRFCK_ANA_CON0
#define MT6359_RG_VRFCK_VOSEL_MASK                            0xF
#define MT6359_RG_VRFCK_VOSEL_SHIFT                           8
#define MT6359_RG_VRFCK_RSV_1_ADDR                            \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RG_VRFCK_RSV_1_MASK                            0x1
#define MT6359_RG_VRFCK_RSV_1_SHIFT                           2
#define MT6359_RG_VRFCK_OC_LP_EN_ADDR                         \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RG_VRFCK_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VRFCK_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VRFCK_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RG_VRFCK_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VRFCK_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VRFCK_ULP_BIASX2_EN_ADDR                    \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RG_VRFCK_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VRFCK_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VRFCK_MEASURE_FT_EN_ADDR                    \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RG_VRFCK_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VRFCK_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VRFCK_OC_STATUS_ADDR                       \
	MT6359_VRFCK_ANA_CON1
#define MT6359_RGS_VRFCK_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VRFCK_OC_STATUS_SHIFT                      7
#define MT6359_RG_VRFCK_1_VOCAL_ADDR                          \
	MT6359_VRFCK_1_ANA_CON0
#define MT6359_RG_VRFCK_1_VOCAL_MASK                          0xF
#define MT6359_RG_VRFCK_1_VOCAL_SHIFT                         0
#define MT6359_RG_VRFCK_1_VOSEL_ADDR                          \
	MT6359_VRFCK_1_ANA_CON0
#define MT6359_RG_VRFCK_1_VOSEL_MASK                          0xF
#define MT6359_RG_VRFCK_1_VOSEL_SHIFT                         8
#define MT6359_RG_VRFCK_1_RSV_1_ADDR                          \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RG_VRFCK_1_RSV_1_MASK                          0x1
#define MT6359_RG_VRFCK_1_RSV_1_SHIFT                         2
#define MT6359_RG_VRFCK_1_OC_LP_EN_ADDR                       \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RG_VRFCK_1_OC_LP_EN_MASK                       0x1
#define MT6359_RG_VRFCK_1_OC_LP_EN_SHIFT                      3
#define MT6359_RG_VRFCK_1_ULP_IQ_CLAMP_EN_ADDR                \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RG_VRFCK_1_ULP_IQ_CLAMP_EN_MASK                0x1
#define MT6359_RG_VRFCK_1_ULP_IQ_CLAMP_EN_SHIFT               4
#define MT6359_RG_VRFCK_1_ULP_BIASX2_EN_ADDR                  \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RG_VRFCK_1_ULP_BIASX2_EN_MASK                  0x1
#define MT6359_RG_VRFCK_1_ULP_BIASX2_EN_SHIFT                 5
#define MT6359_RG_VRFCK_1_MEASURE_FT_EN_ADDR                  \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RG_VRFCK_1_MEASURE_FT_EN_MASK                  0x1
#define MT6359_RG_VRFCK_1_MEASURE_FT_EN_SHIFT                 6
#define MT6359_RGS_VRFCK_1_OC_STATUS_ADDR                     \
	MT6359_VRFCK_1_ANA_CON1
#define MT6359_RGS_VRFCK_1_OC_STATUS_MASK                     0x1
#define MT6359_RGS_VRFCK_1_OC_STATUS_SHIFT                    7
#define MT6359_RG_VBBCK_VOCAL_ADDR                            \
	MT6359_VBBCK_ANA_CON0
#define MT6359_RG_VBBCK_VOCAL_MASK                            0xF
#define MT6359_RG_VBBCK_VOCAL_SHIFT                           0
#define MT6359_RG_VBBCK_VOSEL_ADDR                            \
	MT6359_VBBCK_ANA_CON0
#define MT6359_RG_VBBCK_VOSEL_MASK                            0xF
#define MT6359_RG_VBBCK_VOSEL_SHIFT                           8
#define MT6359_RG_VBBCK_RSV_1_ADDR                            \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RG_VBBCK_RSV_1_MASK                            0x1
#define MT6359_RG_VBBCK_RSV_1_SHIFT                           2
#define MT6359_RG_VBBCK_OC_LP_EN_ADDR                         \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RG_VBBCK_OC_LP_EN_MASK                         0x1
#define MT6359_RG_VBBCK_OC_LP_EN_SHIFT                        3
#define MT6359_RG_VBBCK_ULP_IQ_CLAMP_EN_ADDR                  \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RG_VBBCK_ULP_IQ_CLAMP_EN_MASK                  0x1
#define MT6359_RG_VBBCK_ULP_IQ_CLAMP_EN_SHIFT                 4
#define MT6359_RG_VBBCK_ULP_BIASX2_EN_ADDR                    \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RG_VBBCK_ULP_BIASX2_EN_MASK                    0x1
#define MT6359_RG_VBBCK_ULP_BIASX2_EN_SHIFT                   5
#define MT6359_RG_VBBCK_MEASURE_FT_EN_ADDR                    \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RG_VBBCK_MEASURE_FT_EN_MASK                    0x1
#define MT6359_RG_VBBCK_MEASURE_FT_EN_SHIFT                   6
#define MT6359_RGS_VBBCK_OC_STATUS_ADDR                       \
	MT6359_VBBCK_ANA_CON1
#define MT6359_RGS_VBBCK_OC_STATUS_MASK                       0x1
#define MT6359_RGS_VBBCK_OC_STATUS_SHIFT                      7
#define MT6359_LDO_ANA2_ELR_LEN_ADDR                          \
	MT6359_LDO_ANA2_ELR_NUM
#define MT6359_LDO_ANA2_ELR_LEN_MASK                          0xFF
#define MT6359_LDO_ANA2_ELR_LEN_SHIFT                         0
#define MT6359_RG_VXO22_VOTRIM_ADDR                           \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VXO22_VOTRIM_MASK                           0xF
#define MT6359_RG_VXO22_VOTRIM_SHIFT                          0
#define MT6359_RG_VXO22_NDIS_EN_ADDR                          \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VXO22_NDIS_EN_MASK                          0x1
#define MT6359_RG_VXO22_NDIS_EN_SHIFT                         4
#define MT6359_RG_VRFCK_VOTRIM_ADDR                           \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VRFCK_VOTRIM_MASK                           0xF
#define MT6359_RG_VRFCK_VOTRIM_SHIFT                          5
#define MT6359_RG_VRFCK_NDIS_EN_ADDR                          \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VRFCK_NDIS_EN_MASK                          0x1
#define MT6359_RG_VRFCK_NDIS_EN_SHIFT                         9
#define MT6359_RG_VRFCK_1_VOTRIM_ADDR                         \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VRFCK_1_VOTRIM_MASK                         0xF
#define MT6359_RG_VRFCK_1_VOTRIM_SHIFT                        10
#define MT6359_RG_VRFCK_1_NDIS_EN_ADDR                        \
	MT6359_DCXO_ADLDO_BIAS_ELR_0
#define MT6359_RG_VRFCK_1_NDIS_EN_MASK                        0x1
#define MT6359_RG_VRFCK_1_NDIS_EN_SHIFT                       14
#define MT6359_RG_VBBCK_VOTRIM_ADDR                           \
	MT6359_DCXO_ADLDO_BIAS_ELR_1
#define MT6359_RG_VBBCK_VOTRIM_MASK                           0xF
#define MT6359_RG_VBBCK_VOTRIM_SHIFT                          0
#define MT6359_RG_VBBCK_NDIS_EN_ADDR                          \
	MT6359_DCXO_ADLDO_BIAS_ELR_1
#define MT6359_RG_VBBCK_NDIS_EN_MASK                          0x1
#define MT6359_RG_VBBCK_NDIS_EN_SHIFT                         4
#define MT6359_DUMMYLOAD_ANA_ID_ADDR                          \
	MT6359_DUMMYLOAD_DSN_ID
#define MT6359_DUMMYLOAD_ANA_ID_MASK                          0xFF
#define MT6359_DUMMYLOAD_ANA_ID_SHIFT                         0
#define MT6359_DUMMYLOAD_DIG_ID_ADDR                          \
	MT6359_DUMMYLOAD_DSN_ID
#define MT6359_DUMMYLOAD_DIG_ID_MASK                          0xFF
#define MT6359_DUMMYLOAD_DIG_ID_SHIFT                         8
#define MT6359_DUMMYLOAD_ANA_MINOR_REV_ADDR                   \
	MT6359_DUMMYLOAD_DSN_REV0
#define MT6359_DUMMYLOAD_ANA_MINOR_REV_MASK                   0xF
#define MT6359_DUMMYLOAD_ANA_MINOR_REV_SHIFT                  0
#define MT6359_DUMMYLOAD_ANA_MAJOR_REV_ADDR                   \
	MT6359_DUMMYLOAD_DSN_REV0
#define MT6359_DUMMYLOAD_ANA_MAJOR_REV_MASK                   0xF
#define MT6359_DUMMYLOAD_ANA_MAJOR_REV_SHIFT                  4
#define MT6359_DUMMYLOAD_DIG_MINOR_REV_ADDR                   \
	MT6359_DUMMYLOAD_DSN_REV0
#define MT6359_DUMMYLOAD_DIG_MINOR_REV_MASK                   0xF
#define MT6359_DUMMYLOAD_DIG_MINOR_REV_SHIFT                  8
#define MT6359_DUMMYLOAD_DIG_MAJOR_REV_ADDR                   \
	MT6359_DUMMYLOAD_DSN_REV0
#define MT6359_DUMMYLOAD_DIG_MAJOR_REV_MASK                   0xF
#define MT6359_DUMMYLOAD_DIG_MAJOR_REV_SHIFT                  12
#define MT6359_DUMMYLOAD_DSN_CBS_ADDR                         \
	MT6359_DUMMYLOAD_DSN_DBI
#define MT6359_DUMMYLOAD_DSN_CBS_MASK                         0x3
#define MT6359_DUMMYLOAD_DSN_CBS_SHIFT                        0
#define MT6359_DUMMYLOAD_DSN_BIX_ADDR                         \
	MT6359_DUMMYLOAD_DSN_DBI
#define MT6359_DUMMYLOAD_DSN_BIX_MASK                         0x3
#define MT6359_DUMMYLOAD_DSN_BIX_SHIFT                        2
#define MT6359_DUMMYLOAD_DSN_ESP_ADDR                         \
	MT6359_DUMMYLOAD_DSN_DBI
#define MT6359_DUMMYLOAD_DSN_ESP_MASK                         0xFF
#define MT6359_DUMMYLOAD_DSN_ESP_SHIFT                        8
#define MT6359_DUMMYLOAD_DSN_FPI_ADDR                         \
	MT6359_DUMMYLOAD_DSN_FPI
#define MT6359_DUMMYLOAD_DSN_FPI_MASK                         0xFF
#define MT6359_DUMMYLOAD_DSN_FPI_SHIFT                        0
#define MT6359_RG_ISINK_TRIM_EN_ADDR                          \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK_TRIM_EN_MASK                          0x1
#define MT6359_RG_ISINK_TRIM_EN_SHIFT                         0
#define MT6359_RG_ISINK_TRIM_SEL_ADDR                         \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK_TRIM_SEL_MASK                         0x7
#define MT6359_RG_ISINK_TRIM_SEL_SHIFT                        1
#define MT6359_RG_ISINK_RSV_ADDR                              \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK_RSV_MASK                              0xF
#define MT6359_RG_ISINK_RSV_SHIFT                             4
#define MT6359_RG_ISINK0_CHOP_EN_ADDR                         \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK0_CHOP_EN_MASK                         0x1
#define MT6359_RG_ISINK0_CHOP_EN_SHIFT                        8
#define MT6359_RG_ISINK1_CHOP_EN_ADDR                         \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK1_CHOP_EN_MASK                         0x1
#define MT6359_RG_ISINK1_CHOP_EN_SHIFT                        9
#define MT6359_RG_ISINK0_DOUBLE_ADDR                          \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK0_DOUBLE_MASK                          0x1
#define MT6359_RG_ISINK0_DOUBLE_SHIFT                         12
#define MT6359_RG_ISINK1_DOUBLE_ADDR                          \
	MT6359_DUMMYLOAD_ANA_CON0
#define MT6359_RG_ISINK1_DOUBLE_MASK                          0x1
#define MT6359_RG_ISINK1_DOUBLE_SHIFT                         13
#define MT6359_ISINK0_RSV1_ADDR                               \
	MT6359_ISINK0_CON1
#define MT6359_ISINK0_RSV1_MASK                               0xF
#define MT6359_ISINK0_RSV1_SHIFT                              0
#define MT6359_ISINK0_RSV0_ADDR                               \
	MT6359_ISINK0_CON1
#define MT6359_ISINK0_RSV0_MASK                               0x7
#define MT6359_ISINK0_RSV0_SHIFT                              4
#define MT6359_ISINK_CH0_STEP_ADDR                            \
	MT6359_ISINK0_CON1
#define MT6359_ISINK_CH0_STEP_MASK                            0x7
#define MT6359_ISINK_CH0_STEP_SHIFT                           12
#define MT6359_ISINK1_RSV1_ADDR                               \
	MT6359_ISINK1_CON1
#define MT6359_ISINK1_RSV1_MASK                               0xF
#define MT6359_ISINK1_RSV1_SHIFT                              0
#define MT6359_ISINK1_RSV0_ADDR                               \
	MT6359_ISINK1_CON1
#define MT6359_ISINK1_RSV0_MASK                               0x7
#define MT6359_ISINK1_RSV0_SHIFT                              4
#define MT6359_ISINK_CH1_STEP_ADDR                            \
	MT6359_ISINK1_CON1
#define MT6359_ISINK_CH1_STEP_MASK                            0x7
#define MT6359_ISINK_CH1_STEP_SHIFT                           12
#define MT6359_AD_ISINK0_STATUS_ADDR                          \
	MT6359_ISINK_ANA1_SMPL
#define MT6359_AD_ISINK0_STATUS_MASK                          0x1
#define MT6359_AD_ISINK0_STATUS_SHIFT                         0
#define MT6359_AD_ISINK1_STATUS_ADDR                          \
	MT6359_ISINK_ANA1_SMPL
#define MT6359_AD_ISINK1_STATUS_MASK                          0x1
#define MT6359_AD_ISINK1_STATUS_SHIFT                         1
#define MT6359_ISINK_CH1_EN_ADDR                              \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CH1_EN_MASK                              0x1
#define MT6359_ISINK_CH1_EN_SHIFT                             0
#define MT6359_ISINK_CH0_EN_ADDR                              \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CH0_EN_MASK                              0x1
#define MT6359_ISINK_CH0_EN_SHIFT                             1
#define MT6359_ISINK_CHOP1_EN_ADDR                            \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CHOP1_EN_MASK                            0x1
#define MT6359_ISINK_CHOP1_EN_SHIFT                           4
#define MT6359_ISINK_CHOP0_EN_ADDR                            \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CHOP0_EN_MASK                            0x1
#define MT6359_ISINK_CHOP0_EN_SHIFT                           5
#define MT6359_ISINK_CH1_BIAS_EN_ADDR                         \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CH1_BIAS_EN_MASK                         0x1
#define MT6359_ISINK_CH1_BIAS_EN_SHIFT                        8
#define MT6359_ISINK_CH0_BIAS_EN_ADDR                         \
	MT6359_ISINK_EN_CTRL_SMPL
#define MT6359_ISINK_CH0_BIAS_EN_MASK                         0x1
#define MT6359_ISINK_CH0_BIAS_EN_SHIFT                        9
#define MT6359_DUMMYLOAD_ELR_LEN_ADDR                         \
	MT6359_DUMMYLOAD_ELR_NUM
#define MT6359_DUMMYLOAD_ELR_LEN_MASK                         0xFF
#define MT6359_DUMMYLOAD_ELR_LEN_SHIFT                        0
#define MT6359_RG_ISINK_TRIM_BIAS_ADDR                        \
	MT6359_DUMMYLOAD_ELR_0
#define MT6359_RG_ISINK_TRIM_BIAS_MASK                        0x7
#define MT6359_RG_ISINK_TRIM_BIAS_SHIFT                       0
#define MT6359_RGS_CHRDET_ADDR                                \
	MT6359_CHR_CON0
#define MT6359_RGS_CHRDET_MASK                                0x1
#define MT6359_RGS_CHRDET_SHIFT                               0
#define MT6359_RG_UVLO_VTHL_ADDR                              \
	MT6359_PCHR_VREF_ANA_CON1
#define MT6359_RG_UVLO_VTHL_MASK                              0x1F
#define MT6359_RG_UVLO_VTHL_SHIFT                             2
#define MT6359_RG_LDO_VUFS_LP_ADDR                            \
	MT6359_LDO_VUFS_CON0
#define MT6359_RG_LDO_VUFS_LP_MASK                            0x1
#define MT6359_RG_LDO_VUFS_LP_SHIFT                           1

#endif /* __MFD_MT6359_REGISTERS_H__ */
