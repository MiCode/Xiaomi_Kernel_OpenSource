/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Shi.Ma <shi.ma@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_MMC_MT65XX_H
#define _DT_BINDINGS_MMC_MT65XX_H

#define MSDC_EMMC	(0)
#define MSDC_SD		(1)
#define MSDC_SDIO	(2)

#define MSDC_CD_HIGH        (1)
#define MSDC_CD_LOW         (0)

/* each PLL have different gears for select
 * software can used mux interface from clock management module to select */
#define MSDC50_CLKSRC_26MHZ        (0)
#define MSDC50_CLKSRC_800MHZ         (1)
#define MSDC50_CLKSRC_400MHZ        (2)
#define MSDC50_CLKSRC_200MHZ        (3)
#define MSDC50_CLKSRC_182MHZ        (4)
#define MSDC50_CLKSRC_136MHZ         (5)
#define MSDC50_CLKSRC_156MHZ        (6)
#define MSDC50_CLKSRC_416MHZ        (7)
#define MSDC50_CLKSRC_48MHZ        (8)
#define MSDC50_CLKSRC_91MHZ       (9)
#define MSDC50_CLKSRC_624MHZ      (10)

#define MSDC30_CLKSRC_26MHZ       (0)
#define MSDC30_CLKSRC_208MHZ       (1)
#define MSDC30_CLKSRC_200MHZ      (2)
#define MSDC30_CLKSRC_182MHZ       (3)
#define MSDC30_CLKSRC_136MHZ       (4)
#define MSDC30_CLKSRC_156MHZ       (5)
#define MSDC30_CLKSRC_48MHZ        (6)
#define MSDC30_CLKSRC_91MHZ      (7)

/* #define HOST_MAX_MCLK       (200000000) */ /*(104000000) */

#define MMC_VDD_165_195	0x00000080	/* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21		0x00000100	/* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22		0x00000200	/* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23		0x00000400	/* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24		0x00000800	/* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25		0x00001000	/* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26		0x00002000	/* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27		0x00004000	/* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28		0x00008000	/* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29		0x00010000	/* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30		0x00020000	/* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31		0x00040000	/* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32		0x00080000	/* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33		0x00100000	/* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34		0x00200000	/* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35		0x00400000	/* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36		0x00800000	/* VDD voltage 3.5 ~ 3.6 */

#define MSDC_SMPL_RISING    (0)
#define MSDC_SMPL_FALLING   (1)

/*msdc register address*/
#define OFFSET_MSDC_PATCH_BIT0					0x000000B0
#define OFFSET_MSDC_PATCH_BIT1					0x000000B4
#define OFFSET_MSDC_IOCON								0x00000004
#define OFFSET_MSDC_PAD_TUNE0						0x000000F0
#define OFFSET_MSDC_DAT_RDDLY0					0x000000F8
#define OFFSET_EMMC50_PAD_DS_TUNE				0x00000188

/*bit mask*/
/* MSDC_PATCH_BIT0 mask */
#define MSDC_PB0_RESV1           (0x1 << 0)
#define MSDC_PB0_EN_8BITSUP      (0x1 << 1)
#define MSDC_PB0_DIS_RECMDWR     (0x1 << 2)
#define MSDC_PB0_RESV2           (0x7 << 3)
#define MSDC_PB0_DESCUP          (0x1 << 6)
#define MSDC_PB0_INT_DAT_LATCH_CK_SEL (0x7 << 7)
#define MSDC_PB0_CKGEN_MSDC_DLY_SEL   (0x1F<<10)
#define MSDC_PB0_FIFORD_DIS      (0x1 << 15)
#define MSDC_PB0_BLKNUM_SEL      (0x1 << 16)
#define MSDC_PB0_SDIO_INTCSEL    (0x1 << 17)
#define MSDC_PB0_SDC_BSYDLY      (0xf << 18)
#define MSDC_PB0_SDC_WDOD        (0xf << 22)
#define MSDC_PB0_CMDIDRTSEL      (0x1 << 26)
#define MSDC_PB0_CMDFAILSEL      (0x1 << 27)
#define MSDC_PB0_SDIO_INTDLYSEL  (0x1 << 28)
#define MSDC_PB0_SPCPUSH         (0x1 << 29)
#define MSDC_PB0_DETWR_CRCTMO    (0x1 << 30)
#define MSDC_PB0_EN_DRVRSP       (0x1UL << 31)

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PB1_WRDAT_CRCS_TA_CNTR   (0x7 << 0)
#define MSDC_PB1_CMD_RSP_TA_CNTR      (0x7 << 3)
#define MSDC_PB1_GET_BUSY_MA     (0x1 << 6)
#define MSDC_PB1_GET_CRC_MA      (0x1 << 7)
#define MSDC_PB1_BIAS_TUNE_28NM  (0xf << 8)
#define MSDC_PB1_BIAS_EN18IO_28NM (0x1 << 12)
#define MSDC_PB1_BIAS_EXT_28NM   (0x1 << 13)
#define MSDC_PB1_RESV2           (0x1 << 14)
#define MSDC_PB1_RESET_GDMA      (0x1 << 15)
#define MSDC_PB1_SINGLEBURST     (0x1 << 16)
#define MSDC_PB1_FROCE_STOP      (0x1 << 17)
#define MSDC_PB1_DCM_DEV_SEL2    (0x3 << 18)
#define MSDC_PB1_DCM_DEV_SEL1    (0x1 << 20)
#define MSDC_PB1_DCM_EN          (0x1 << 21)
#define MSDC_PB1_AXI_WRAP_CKEN   (0x1 << 22)
#define MSDC_PB1_AHBCKEN         (0x1 << 23)
#define MSDC_PB1_CKSPCEN         (0x1 << 24)
#define MSDC_PB1_CKPSCEN         (0x1 << 25)
#define MSDC_PB1_CKVOLDETEN      (0x1 << 26)
#define MSDC_PB1_CKACMDEN        (0x1 << 27)
#define MSDC_PB1_CKSDEN          (0x1 << 28)
#define MSDC_PB1_CKWCTLEN        (0x1 << 29)
#define MSDC_PB1_CKRCTLEN        (0x1 << 30)
#define MSDC_PB1_CKSHBFFEN       (0x1UL << 31)

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1  << 0)     /* RW */
#define MSDC_IOCON_RSPL         (0x1  << 1)     /* RW */
#define MSDC_IOCON_R_D_SMPL     (0x1  << 2)     /* RW */
#define MSDC_IOCON_DDLSEL       (0x1  << 3)     /* RW */
#define MSDC_IOCON_DDR50CKD     (0x1  << 4)     /* RW */
#define MSDC_IOCON_R_D_SMPL_SEL (0x1  << 5)     /* RW */
#define MSDC_IOCON_W_D_SMPL     (0x1  << 8)     /* RW */
#define MSDC_IOCON_W_D_SMPL_SEL (0x1  << 9)     /* RW */
#define MSDC_IOCON_W_D0SPL      (0x1  << 10)    /* RW */
#define MSDC_IOCON_W_D1SPL      (0x1  << 11)    /* RW */
#define MSDC_IOCON_W_D2SPL      (0x1  << 12)    /* RW */
#define MSDC_IOCON_W_D3SPL      (0x1  << 13)    /* RW */
#define MSDC_IOCON_R_D0SPL      (0x1  << 16)    /* RW */
#define MSDC_IOCON_R_D1SPL      (0x1  << 17)    /* RW */
#define MSDC_IOCON_R_D2SPL      (0x1  << 18)    /* RW */
#define MSDC_IOCON_R_D3SPL      (0x1  << 19)    /* RW */
#define MSDC_IOCON_R_D4SPL      (0x1  << 20)    /* RW */
#define MSDC_IOCON_R_D5SPL      (0x1  << 21)    /* RW */
#define MSDC_IOCON_R_D6SPL      (0x1  << 22)    /* RW */
#define MSDC_IOCON_R_D7SPL      (0x1  << 23)    /* RW */

/* MSDC_PAD_TUNE0 mask */
#define MSDC_PAD_TUNE0_DATWRDLY      (0x1F <<  0)     /* RW */
#define MSDC_PAD_TUNE0_DELAYEN       (0x1  <<  7)     /* RW */
#define MSDC_PAD_TUNE0_DATRRDLY      (0x1F <<  8)     /* RW */
#define MSDC_PAD_TUNE0_DATRRDLYSEL   (0x1  << 13)     /* RW */
#define MSDC_PAD_TUNE0_RXDLYSEL      (0x1  << 15)     /* RW */
#define MSDC_PAD_TUNE0_CMDRDLY       (0x1F << 16)     /* RW */
#define MSDC_PAD_TUNE0_CMDRRDLYSEL   (0x1  << 21)     /* RW */
#define MSDC_PAD_TUNE0_CMDRRDLY      (0x1FUL << 22)   /* RW */
#define MSDC_PAD_TUNE0_CLKTXDLY      (0x1FUL << 27)   /* RW */

/* MSDC_DAT_RDDLY0/1/2/3 mask */
#define MSDC_DAT_RDDLY0_D3      (0x1F << 0)     /* RW */
#define MSDC_DAT_RDDLY0_D2      (0x1F << 8)     /* RW */
#define MSDC_DAT_RDDLY0_D1      (0x1F << 16)    /* RW */
#define MSDC_DAT_RDDLY0_D0      (0x1FUL << 24)  /* RW */

/* EMMC50_PAD_DS_TUNE mask */
#define MSDC_EMMC50_PAD_DS_TUNE_DLYSEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL (0x1 << 1)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY1    (0x1f << 2)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2    (0x1f << 7)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY3    (0x1F << 12)

#endif /* _DT_BINDINGS_MMC_MT65XX_H */
