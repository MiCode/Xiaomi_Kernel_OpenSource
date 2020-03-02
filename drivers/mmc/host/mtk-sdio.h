/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
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

#define MAX_BD_NUM          1024

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_BUS_1BITS          0x0
#define MSDC_BUS_4BITS          0x1
#define MSDC_BUS_8BITS          0x2

#define MSDC_BURST_64B          0x6

/*--------------------------------------------------------------------------*/
/* Register Offset                                                          */
/*--------------------------------------------------------------------------*/
#define MSDC_CFG         0x0
#define MSDC_IOCON       0x04
#define MSDC_PS          0x08
#define MSDC_INT         0x0c
#define MSDC_INTEN       0x10
#define MSDC_FIFOCS      0x14
#define MSDC_TXDATA      0x18
#define MSDC_RXDATA      0x1c
#define SDC_CFG          0x30
#define SDC_CMD          0x34
#define SDC_ARG          0x38
#define SDC_STS          0x3c
#define SDC_RESP0        0x40
#define SDC_RESP1        0x44
#define SDC_RESP2        0x48
#define SDC_RESP3        0x4c
#define SDC_BLK_NUM      0x50
#define EMMC_IOCON       0x7c
#define SDC_ACMD_RESP    0x80
#define MSDC_DMA_SA      0x90
#define MSDC_DMA_CTRL    0x98
#define MSDC_DMA_CFG     0x9c
#define MSDC_DBG_SEL     0xa0
#define MSDC_DBG_OUT	 0xa4
#define MSDC_DMA_LEN	 0xa8
#define MSDC_PATCH_BIT0	 0xb0
#define MSDC_PATCH_BIT1	 0xb4
#define MSDC_PATCH_BIT2	 0xb8
#define DAT0_TUNE_CRC	 0xc0
#define DAT1_TUNE_CRC	 0xc4
#define DAT2_TUNE_CRC	 0xc8
#define DAT3_TUNE_CRC	 0xcc
#define CMD_TUNE_CRC	 0xd0
#define SDIO_TUNE_WIND	 0xd4
#define MSDC_PAD_TUNE0	 0xf0
#define MSDC_PAD_TUNE1	 0xf4
#define MSDC_DAT_RDDLY0	 0xf8
#define MSDC_DAT_RDDLY1	 0xfc
#define MSDC_DAT_RDDLY2	 0x100
#define MSDC_DAT_RDDLY3	 0x104
#define MSDC_HW_DBG	 0x110
#define MSDC_VERSION		0x114
#define MSDC_ECO_VER		0x118
#define EMMC50_PAD_CTL0		0x180
#define EMMC50_PAD_DS_CTL0	0x184
#define EMMC50_PAD_DS_TUNE	0x188
#define EMMC50_PAD_CMD_TUNE	0x18c
#define EMMC50_PAD_DAT01_TUNE	0x190
#define EMMC50_PAD_DAT23_TUNE	0x194
#define EMMC50_PAD_DAT45_TUNE	0x198
#define EMMC50_PAD_DAT67_TUNE	0x19c
#define EMMC51_CFG0		0x204
#define EMMC50_CFG0		0x208
#define EMMC50_CFG1		0x20c
#define EMMC50_CFG2		0x21c
#define EMMC50_CFG3		0x220
#define EMMC50_CFG4		0x224
#define MSDC_SDC_FIFO_CFG	0x228

#define MAX_REGISTER_ADDR	0x228

/*--------------------------------------------------------------------------*/
/*Top Register Offset                                                       */
/*--------------------------------------------------------------------------*/
#define MSDC_TOP_CONTROL	(0x00)
#define MSDC_TOP_CMD		(0x04)
#define MSDC_TOP_PAD_CTRL0	(0x08)
#define MSDC_TOP_PAD_DS_TUNE	(0x0c)
#define MSDC_TOP_PAD_DAT0_TUNE	(0x10)
#define MSDC_TOP_PAD_DAT1_TUNE	(0x14)
#define MSDC_TOP_PAD_DAT2_TUNE	(0x18)
#define MSDC_TOP_PAD_DAT3_TUNE	(0x1c)

/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

/* MSDC_CFG mask */
#define MSDC_CFG_MODE           (0x1 << 0)	/* RW */
#define MSDC_CFG_CKPDN          (0x1 << 1)	/* RW */
#define MSDC_CFG_RST            (0x1 << 2)	/* RW */
#define MSDC_CFG_PIO            (0x1 << 3)	/* RW */
#define MSDC_CFG_CKDRVEN        (0x1 << 4)	/* RW */
#define MSDC_CFG_BV18SDT        (0x1 << 5)	/* RW */
#define MSDC_CFG_BV18PSS        (0x1 << 6)	/* R  */
#define MSDC_CFG_CKSTB          (0x1 << 7)	/* R  */
#define MSDC_CFG_CKDIV          (0xfff << 8)	/* RW */
#define MSDC_CFG_CKDIV_BITS             (12)
#define MSDC_CFG_CKMOD          (0x3 << 20)	/* RW */
#define MSDC_CFG_CKMOD_BITS             (2)
#define MSDC_CFG_HS400_CK_MODE  (0x1 << 22)	/* RW */
#define MSDC_CFG_START_BIT              (0x3  << 23)    /* RW */
#define MSDC_CFG_SCLK_STOP_DDR          (0x1  << 25)    /* RW */
#define MSDC_CFG_DVFS_EN                (0x1  << 30)    /* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1 << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1 << 1)	/* RW */
#define MSDC_IOCON_R_D_SMPL             (0x1  << 2)     /* RW */
#define MSDC_IOCON_DDLSEL               (0x1  << 3)     /* RW */
#define MSDC_IOCON_DDR50CKD             (0x1  << 4)     /* RW */
#define MSDC_IOCON_R_D_SMPL_SEL         (0x1  << 5)     /* RW */
#define MSDC_IOCON_W_D_SMPL             (0x1  << 8)     /* RW */
#define MSDC_IOCON_W_D_SMPL_SEL         (0x1  << 9)     /* RW */
#define MSDC_IOCON_W_D0SPL              (0x1  << 10)    /* RW */
#define MSDC_IOCON_W_D1SPL              (0x1  << 11)    /* RW */
#define MSDC_IOCON_W_D2SPL              (0x1  << 12)    /* RW */
#define MSDC_IOCON_W_D3SPL              (0x1  << 13)    /* RW */
#define MSDC_IOCON_R_D0SPL              (0x1  << 16)    /* RW */
#define MSDC_IOCON_R_D1SPL              (0x1  << 17)    /* RW */
#define MSDC_IOCON_R_D2SPL              (0x1  << 18)    /* RW */
#define MSDC_IOCON_R_D3SPL              (0x1  << 19)    /* RW */
#define MSDC_IOCON_R_D4SPL              (0x1  << 20)    /* RW */
#define MSDC_IOCON_R_D5SPL              (0x1  << 21)    /* RW */
#define MSDC_IOCON_R_D6SPL              (0x1  << 22)    /* RW */
#define MSDC_IOCON_R_D7SPL              (0x1  << 23)    /* RW */

/* MSDC_PS mask */
#define MSDC_PS_CDEN            (0x1 << 0)	/* RW */
#define MSDC_PS_CDSTS           (0x1 << 1)	/* R  */
#define MSDC_PS_CDDEBOUNCE      (0xf << 12)	/* RW */
#define MSDC_PS_DAT             (0xff << 16)	/* R  */
#define MSDC_PS_DATA1           (0x1 << 17)	/* R  */
#define MSDC_PS_CMD             (0x1 << 24)	/* R  */
#define MSDC_PS_WP              (0x1 << 31)	/* R  */

/* MSDC_INT mask */
#define MSDC_INT_MMCIRQ         (0x1 << 0)	/* W1C */
#define MSDC_INT_CDSC           (0x1 << 1)	/* W1C */
#define MSDC_INT_ACMDRDY        (0x1 << 3)	/* W1C */
#define MSDC_INT_ACMDTMO        (0x1 << 4)	/* W1C */
#define MSDC_INT_ACMDCRCERR     (0x1 << 5)	/* W1C */
#define MSDC_INT_DMAQ_EMPTY     (0x1 << 6)	/* W1C */
#define MSDC_INT_SDIOIRQ        (0x1 << 7)	/* W1C */
#define MSDC_INT_CMDRDY         (0x1 << 8)	/* W1C */
#define MSDC_INT_CMDTMO         (0x1 << 9)	/* W1C */
#define MSDC_INT_RSPCRCERR      (0x1 << 10)	/* W1C */
#define MSDC_INT_CSTA           (0x1 << 11)	/* R */
#define MSDC_INT_XFER_COMPL     (0x1 << 12)	/* W1C */
#define MSDC_INT_DXFER_DONE     (0x1 << 13)	/* W1C */
#define MSDC_INT_DATTMO         (0x1 << 14)	/* W1C */
#define MSDC_INT_DATCRCERR      (0x1 << 15)	/* W1C */
#define MSDC_INT_ACMD19_DONE    (0x1 << 16)	/* W1C */
#define MSDC_INT_DMA_BDCSERR    (0x1 << 17)	/* W1C */
#define MSDC_INT_DMA_GPDCSERR   (0x1 << 18)	/* W1C */
#define MSDC_INT_DMA_PROTECT    (0x1 << 19)	/* W1C */

/* MSDC_INTEN mask */
#define MSDC_INTEN_MMCIRQ       (0x1 << 0)	/* RW */
#define MSDC_INTEN_CDSC         (0x1 << 1)	/* RW */
#define MSDC_INTEN_ACMDRDY      (0x1 << 3)	/* RW */
#define MSDC_INTEN_ACMDTMO      (0x1 << 4)	/* RW */
#define MSDC_INTEN_ACMDCRCERR   (0x1 << 5)	/* RW */
#define MSDC_INTEN_DMAQ_EMPTY   (0x1 << 6)	/* RW */
#define MSDC_INTEN_SDIOIRQ      (0x1 << 7)	/* RW */
#define MSDC_INTEN_CMDRDY       (0x1 << 8)	/* RW */
#define MSDC_INTEN_CMDTMO       (0x1 << 9)	/* RW */
#define MSDC_INTEN_RSPCRCERR    (0x1 << 10)	/* RW */
#define MSDC_INTEN_CSTA         (0x1 << 11)	/* RW */
#define MSDC_INTEN_XFER_COMPL   (0x1 << 12)	/* RW */
#define MSDC_INTEN_DXFER_DONE   (0x1 << 13)	/* RW */
#define MSDC_INTEN_DATTMO       (0x1 << 14)	/* RW */
#define MSDC_INTEN_DATCRCERR    (0x1 << 15)	/* RW */
#define MSDC_INTEN_ACMD19_DONE  (0x1 << 16)	/* RW */
#define MSDC_INTEN_DMA_BDCSERR  (0x1 << 17)	/* RW */
#define MSDC_INTEN_DMA_GPDCSERR (0x1 << 18)	/* RW */
#define MSDC_INTEN_DMA_PROTECT  (0x1 << 19)	/* RW */

/* MSDC_FIFOCS mask */
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R */
#define MSDC_FIFOCS_CLR         (0x1 << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1 << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1 << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3 << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1 << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1 << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1 << 21)	/* RW */
#define SDC_CFG_DTOC            (0xff << 24)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (0x1 << 0)	/* RW */
#define SDC_STS_CMDBUSY         (0x1 << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (0x1 << 31)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (0x1 << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (0x1 << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (0x1 << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (0x1 << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (0x1 << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7 << 12)	/* RW */

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (0x1 << 0)	/* R */
#define MSDC_DMA_CFG_DECSEN     (0x1 << 1)	/* RW */
#define MSDC_DMA_CFG_AHBHPROT2  (0x2 << 8)	/* RW */
#define MSDC_DMA_CFG_ACTIVEEN   (0x2 << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B16B   (0x1 << 16)	/* RW */

/* MSDC_PATCH_BIT0 mask */
#define MSDC_PB0_RESV1                  (0x1 << 0)
#define MSDC_PB0_EN_8BITSUP             (0x1 << 1)
#define MSDC_PB0_DIS_RECMDWR            (0x1 << 2)
#define MSDC_PB0_RD_DAT_SEL             (0x1 << 3)
#define MSDC_PB0_RESV2                  (0x3 << 4)
#define MSDC_PB0_DESCUP                 (0x1 << 6)
#define MSDC_PB0_INT_DAT_LATCH_CK_SEL   (0x7 << 7)
#define MSDC_PB0_CKGEN_MSDC_DLY_SEL     (0x1F<<10)
#define MSDC_PB0_FIFORD_DIS             (0x1 << 15)
#define MSDC_PB0_BLKNUM_SEL             (0x1 << 16)
#define MSDC_PB0_SDIO_INTCSEL           (0x1 << 17)
#define MSDC_PB0_SDC_BSYDLY             (0xf << 18)
#define MSDC_PB0_SDC_WDOD               (0xf << 22)
#define MSDC_PB0_CMDIDRTSEL             (0x1 << 26)
#define MSDC_PB0_CMDFAILSEL             (0x1 << 27)
#define MSDC_PB0_SDIO_INTDLYSEL         (0x1 << 28)
#define MSDC_PB0_SPCPUSH                (0x1 << 29)
#define MSDC_PB0_DETWR_CRCTMO           (0x1 << 30)
#define MSDC_PB0_EN_DRVRSP              (0x1UL << 31)

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PB1_WRDAT_CRCS_TA_CNTR     (0x7 << 0)
#define MSDC_PB1_CMD_RSP_TA_CNTR        (0x7 << 3)
#define MSDC_PB1_GET_BUSY_MA            (0x1 << 6)
#define MSDC_PB1_GET_CRC_MA             (0x1 << 7)
#define MSDC_PB1_STOP_DLY_SEL           (0xf << 8)
#define MSDC_PB1_BIAS_EN18IO_28NM       (0x1 << 12)
#define MSDC_PB1_BIAS_EXT_28NM          (0x1 << 13)
#define MSDC_PB1_RESV2                  (0x1 << 14)
#define MSDC_PB1_RESET_GDMA             (0x1 << 15)
#define MSDC_PB1_SINGLE_BURST           (0x1 << 16)
#define MSDC_PB1_FROCE_STOP             (0x1 << 17)
#define MSDC_PB1_POP_MARK_WATER         (0x1 << 19)
#define MSDC_PB1_STATE_CLEAR            (0x1 << 20)
#define MSDC_PB1_DCM_EN                 (0x1 << 21)
#define MSDC_PB1_AXI_WRAP_CKEN          (0x1 << 22)
#define MSDC_PB1_CKCLK_GDMA_EN          (0x1 << 23)
#define MSDC_PB1_CKSPCEN                (0x1 << 24)
#define MSDC_PB1_CKPSCEN                (0x1 << 25)
#define MSDC_PB1_CKVOLDETEN             (0x1 << 26)
#define MSDC_PB1_CKACMDEN               (0x1 << 27)
#define MSDC_PB1_CKSDEN                 (0x1 << 28)
#define MSDC_PB1_CKWCTLEN               (0x1 << 29)
#define MSDC_PB1_CKRCTLEN               (0x1 << 30)
#define MSDC_PB1_CKSHBFFEN              (0x1UL << 31)

/* MSDC_PATCH_BIT2 mask */
#define MSDC_PB2_ENHANCEGPD             (0x1 << 0)
#define MSDC_PB2_SUPPORT64G             (0x1 << 1)
#define MSDC_PB2_RESPWAITCNT            (0x3 << 2)
#define MSDC_PB2_CFGRDATCNT             (0x1f << 4)
#define MSDC_PB2_CFGRDAT                (0x1 << 9)
#define MSDC_PB2_INTCRESPSEL            (0x1 << 11)
#define MSDC_PB2_CFGRESPCNT             (0x7 << 12)
#define MSDC_PB2_CFGRESP                (0x1 << 15)
#define MSDC_PB2_RESPSTENSEL            (0x7 << 16)
#define MSDC_PB2_POPENCNT               (0xf << 20)
#define MSDC_PB2_CFG_CRCSTS_SEL         (0x1 << 24)
#define MSDC_PB2_CFGCRCSTSEDGE          (0x1 << 25)
#define MSDC_PB2_CFGCRCSTSCNT           (0x3 << 26)
#define MSDC_PB2_CFGCRCSTS              (0x1 << 28)
#define MSDC_PB2_CRCSTSENSEL            (0x7UL << 29)

#define MSDC_MASK_ACMD53_CRC_ERR_INTR   (0x1<<4)
#define MSDC_ACMD53_FAIL_ONE_SHOT       (0X1<<5)

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE0_DATWRDLY         (0x1F <<  0)     /* RW */
#define MSDC_PAD_TUNE0_DELAYEN          (0x1  <<  7)     /* RW */
#define MSDC_PAD_TUNE0_DATRRDLY         (0x1F <<  8)     /* RW */
#define MSDC_PAD_TUNE0_DATRRDLYSEL      (0x1  << 13)     /* RW */
#define MSDC_PAD_TUNE0_RXDLYSEL         (0x1  << 15)     /* RW */
#define MSDC_PAD_TUNE0_CMDRDLY          (0x1F << 16)     /* RW */
#define MSDC_PAD_TUNE0_CMDRRDLYSEL      (0x1  << 21)     /* RW */
#define MSDC_PAD_TUNE0_CMDRRDLY         (0x1FUL << 22)   /* RW */
#define MSDC_PAD_TUNE0_CLKTXDLY         (0x1FUL << 27)   /* RW */

/* MSDC_PAD_TUNE1 mask */
#define MSDC_PAD_TUNE1_DATRRDLY2        (0x1F <<  8)     /* RW */
#define MSDC_PAD_TUNE1_DATRRDLY2SEL     (0x1  << 13)     /* RW */
#define MSDC_PAD_TUNE1_CMDRDLY2         (0x1F << 16)     /* RW */
#define MSDC_PAD_TUNE1_CMDRRDLY2SEL     (0x1  << 21)     /* RW */

/* MSDC_DAT_RDDLY0/1/2/3 mask */
#define MSDC_DAT_RDDLY0_D3              (0x1F << 0)     /* RW */
#define MSDC_DAT_RDDLY0_D2              (0x1F << 8)     /* RW */
#define MSDC_DAT_RDDLY0_D1              (0x1F << 16)    /* RW */
#define MSDC_DAT_RDDLY0_D0              (0x1FUL << 24)  /* RW */

#define MSDC_DAT_RDDLY1_D7              (0x1F << 0)     /* RW */
#define MSDC_DAT_RDDLY1_D6              (0x1F << 8)     /* RW */
#define MSDC_DAT_RDDLY1_D5              (0x1F << 16)    /* RW */
#define MSDC_DAT_RDDLY1_D4              (0x1FUL << 24)  /* RW */

#define MSDC_DAT_RDDLY2_D3              (0x1F << 0)     /* RW */
#define MSDC_DAT_RDDLY2_D2              (0x1F << 8)     /* RW */
#define MSDC_DAT_RDDLY2_D1              (0x1F << 16)    /* RW */
#define MSDC_DAT_RDDLY2_D0              (0x1FUL << 24)  /* RW */

#define MSDC_DAT_RDDLY3_D7              (0x1F << 0)     /* RW */
#define MSDC_DAT_RDDLY3_D6              (0x1F << 8)     /* RW */
#define MSDC_DAT_RDDLY3_D5              (0x1F << 16)    /* RW */
#define MSDC_DAT_RDDLY3_D4              (0x1FUL << 24)  /* RW */

/* MSDC_HW_DBG_SEL mask */
#define MSDC_HW_DBG0_SEL                (0xFF << 0)
#define MSDC_HW_DBG1_SEL                (0x3F << 8)
#define MSDC_HW_DBG2_SEL                (0xFF << 16)
#define MSDC_HW_DBG3_SEL                (0x3F << 24)
#define MSDC_HW_DBG_WRAPTYPE_SEL        (0x1  << 30)

/* MSDC_PATCH_BIT mask */
#define MSDC_PATCH_BIT_ODDSUPP    (0x1 <<  1)	/* RW */
#define MSDC_INT_DAT_LATCH_CK_SEL (0x7 <<  7)
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1f << 10)
#define MSDC_PATCH_BIT_IODSSEL    (0x1 << 16)	/* RW */
#define MSDC_PATCH_BIT_IOINTSEL   (0x1 << 17)	/* RW */
#define MSDC_PATCH_BIT_BUSYDLY    (0xf << 18)	/* RW */
#define MSDC_PATCH_BIT_WDOD       (0xf << 22)	/* RW */
#define MSDC_PATCH_BIT_IDRTSEL    (0x1 << 26)	/* RW */
#define MSDC_PATCH_BIT_CMDFSEL    (0x1 << 27)	/* RW */
#define MSDC_PATCH_BIT_INTDLSEL   (0x1 << 28)	/* RW */
#define MSDC_PATCH_BIT_SPCPUSH    (0x1 << 29)	/* RW */
#define MSDC_PATCH_BIT_DECRCTMO   (0x1 << 30)	/* RW */

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PATCH_BIT1_WRDAT_CRCS  (0x7 << 0)
#define MSDC_PATCH_BIT1_CMD_RSP     (0x7 << 3)

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY  (0x1f << 0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY  (0x1f << 8)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY   (0x1f << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY  (0x1f << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTXDLY  (0x1f << 27)	/* RW */

#define PAD_DS_TUNE_DLY1          (0x1f << 2)   /* RW */
#define PAD_DS_TUNE_DLY2          (0x1f << 7)   /* RW */
#define PAD_DS_TUNE_DLY3          (0x1f << 12)  /* RW */

/* MSDC_EMMC50_PAD_CTL0 mask*/
#define MSDC_EMMC50_PAD_CTL0_DCCSEL     (0x1 << 0)
#define MSDC_EMMC50_PAD_CTL0_HLSEL      (0x1 << 1)
#define MSDC_EMMC50_PAD_CTL0_DLP0       (0x3 << 2)
#define MSDC_EMMC50_PAD_CTL0_DLN0       (0x3 << 4)
#define MSDC_EMMC50_PAD_CTL0_DLP1       (0x3 << 6)
#define MSDC_EMMC50_PAD_CTL0_DLN1       (0x3 << 8)

/* MSDC_EMMC50_PAD_DS_CTL0 mask */
#define MSDC_EMMC50_PAD_DS_CTL0_SR      (0x1 << 0)
#define MSDC_EMMC50_PAD_DS_CTL0_R0      (0x1 << 1)
#define MSDC_EMMC50_PAD_DS_CTL0_R1      (0x1 << 2)
#define MSDC_EMMC50_PAD_DS_CTL0_PUPD    (0x1 << 3)
#define MSDC_EMMC50_PAD_DS_CTL0_IES     (0x1 << 4)
#define MSDC_EMMC50_PAD_DS_CTL0_SMT     (0x1 << 5)
#define MSDC_EMMC50_PAD_DS_CTL0_RDSEL   (0x3F << 6)
#define MSDC_EMMC50_PAD_DS_CTL0_TDSEL   (0xf << 12)
#define MSDC_EMMC50_PAD_DS_CTL0_DRV     (0x7 << 16)

/* EMMC50_PAD_DS_TUNE mask */
#define MSDC_EMMC50_PAD_DS_TUNE_DLYSEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL (0x1 << 1)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY1    (0x1F << 2)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2    (0x1F << 7)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY3    (0x1F << 12)

/* EMMC50_PAD_CMD_TUNE mask */
#define MSDC_EMMC50_PAD_CMD_TUNE_DLY3SEL (0x1 << 0)
#define MSDC_EMMC50_PAD_CMD_TUNE_RXDLY3 (0x1F << 1)
#define MSDC_EMMC50_PAD_CMD_TUNE_TXDLY  (0x1F << 6)

/* EMMC50_PAD_DAT01_TUNE mask */
#define MSDC_EMMC50_PAD_DAT0_RXDLY3SEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT0_RXDLY3     (0x1F << 1)
#define MSDC_EMMC50_PAD_DAT0_TXDLY      (0x1F << 6)
#define MSDC_EMMC50_PAD_DAT1_RXDLY3SEL  (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT1_RXDLY3     (0x1F << 17)
#define MSDC_EMMC50_PAD_DAT1_TXDLY      (0x1F << 22)

/* EMMC50_PAD_DAT23_TUNE mask */
#define MSDC_EMMC50_PAD_DAT2_RXDLY3SEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT2_RXDLY3     (0x1F << 1)
#define MSDC_EMMC50_PAD_DAT2_TXDLY      (0x1F << 6)
#define MSDC_EMMC50_PAD_DAT3_RXDLY3SEL  (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT3_RXDLY3     (0x1F << 17)
#define MSDC_EMMC50_PAD_DAT3_TXDLY      (0x1F << 22)

/* EMMC50_PAD_DAT45_TUNE mask */
#define MSDC_EMMC50_PAD_DAT4_RXDLY3SEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT4_RXDLY3     (0x1F << 1)
#define MSDC_EMMC50_PAD_DAT4_TXDLY      (0x1F << 6)
#define MSDC_EMMC50_PAD_DAT5_RXDLY3SEL  (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT5_RXDLY3     (0x1F << 17)
#define MSDC_EMMC50_PAD_DAT5_TXDLY      (0x1F << 22)

/* EMMC50_PAD_DAT67_TUNE mask */
#define MSDC_EMMC50_PAD_DAT6_RXDLY3SEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT6_RXDLY3     (0x1F << 1)
#define MSDC_EMMC50_PAD_DAT6_TXDLY      (0x1F << 6)
#define MSDC_EMMC50_PAD_DAT7_RXDLY3SEL  (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT7_RXDLY3     (0x1F << 17)
#define MSDC_EMMC50_PAD_DAT7_TXDLY      (0x1F << 22)

/* EMMC51_CFG0 mask */
#define MSDC_EMMC51_CFG_CMDQEN          (0x1    <<  0)
#define MSDC_EMMC51_CFG_NUM             (0x3F   <<  1)
#define MSDC_EMMC51_CFG_RSPTYPE         (0x7    <<  7)
#define MSDC_EMMC51_CFG_DTYPE           (0x3    << 10)
#define MSDC_EMMC51_CFG_RDATCNT         (0x3FF  << 12)
#define MSDC_EMMC51_CFG_WDATCNT         (0x3FF  << 22)

/* EMMC50_CFG0 mask */
#define MSDC_EMMC50_CFG_PADCMD_LATCHCK  (0x1 << 0)
#define MSDC_EMMC50_CFG_CRC_STS_CNT     (0x3 << 1)
#define MSDC_EMMC50_CFG_CRC_STS_EDGE    (0x1 << 3)
#define MSDC_EMMC50_CFG_CRC_STS_SEL     (0x1 << 4)
#define MSDC_EMMC50_CFG_END_BIT_CHK_CNT (0xf << 5)
#define MSDC_EMMC50_CFG_CMD_RESP_SEL    (0x1 << 9)
#define MSDC_EMMC50_CFG_CMD_EDGE_SEL    (0x1 << 10)
#define MSDC_EMMC50_CFG_ENDBIT_CNT      (0x3FF << 11)
#define MSDC_EMMC50_CFG_READ_DAT_CNT    (0x7 << 21)
#define MSDC_EMMC50_CFG_EMMC50_MON_SEL  (0x1 << 24)
#define MSDC_EMMC50_CFG_MSDC_WR_VALID   (0x1 << 25)
#define MSDC_EMMC50_CFG_MSDC_RD_VALID   (0x1 << 26)
#define MSDC_EMMC50_CFG_MSDC_WR_VALID_SEL (0x1 << 27)
#define MSDC_EMMC50_CFG_MSDC_RD_VALID_SEL (0x1 << 28)
#define MSDC_EMMC50_CFG_TXSKEW_SEL      (0x1 << 29)

/* EMMC50_CFG1 mask */
#define MSDC_EMMC50_CFG1_WRPTR_MARGIN   (0xFF << 0)
#define MSDC_EMMC50_CFG1_CKSWITCH_CNT   (0x7  << 8)
#define MSDC_EMMC50_CFG1_RDDAT_STOP     (0x1  << 11)
#define MSDC_EMMC50_CFG1_WAITCLK_CNT    (0xF  << 12)
#define MSDC_EMMC50_CFG1_DBG_SEL        (0xFF << 16)
#define MSDC_EMMC50_CFG1_PSHCNT         (0x7  << 24)
#define MSDC_EMMC50_CFG1_PSHPSSEL       (0x1  << 27)
#define MSDC_EMMC50_CFG1_DSCFG          (0x1  << 28)
#define MSDC_EMMC50_CFG1_SPARE1         (0x7UL << 29)

/* EMMC50_CFG2_mask */
/*#define MSDC_EMMC50_CFG2_AXI_GPD_UP             (0x1 << 0)*/
#define MSDC_EMMC50_CFG2_AXI_IOMMU_WR_EMI       (0x1 << 1)
#define MSDC_EMMC50_CFG2_AXI_SHARE_EN_WR_EMI    (0x1 << 2)
#define MSDC_EMMC50_CFG2_AXI_IOMMU_RD_EMI       (0x1 << 7)
#define MSDC_EMMC50_CFG2_AXI_SHARE_EN_RD_EMI    (0x1 << 8)
#define MSDC_EMMC50_CFG2_AXI_BOUND_128B         (0x1 << 13)
#define MSDC_EMMC50_CFG2_AXI_BOUND_256B         (0x1 << 14)
#define MSDC_EMMC50_CFG2_AXI_BOUND_512B         (0x1 << 15)
#define MSDC_EMMC50_CFG2_AXI_BOUND_1K           (0x1 << 16)
#define MSDC_EMMC50_CFG2_AXI_BOUND_2K           (0x1 << 17)
#define MSDC_EMMC50_CFG2_AXI_BOUND_4K           (0x1 << 18)
#define MSDC_EMMC50_CFG2_AXI_RD_OUTS_NUM        (0x1F << 19)
#define MSDC_EMMC50_CFG2_AXI_SET_LEN            (0xf << 24)
#define MSDC_EMMC50_CFG2_AXI_RESP_ERR_TYPE      (0x3 << 28)
#define MSDC_EMMC50_CFG2_AXI_BUSY               (0x1 << 30)

/* EMMC50_CFG3_mask */
#define MSDC_EMMC50_CFG3_OUTS_WR                (0x1F << 0)
#define MSDC_EMMC50_CFG3_ULTRA_SET_WR           (0x3F << 5)
#define MSDC_EMMC50_CFG3_PREULTRA_SET_WR        (0x3F << 11)
#define MSDC_EMMC50_CFG3_ULTRA_SET_RD           (0x3F << 17)
#define MSDC_EMMC50_CFG3_PREULTRA_SET_RD        (0x3F << 23)

/* EMMC50_CFG4_mask */
#define MSDC_EMMC50_CFG4_IMPR_ULTRA_SET_WR      (0xFF << 0)
#define MSDC_EMMC50_CFG4_IMPR_ULTRA_SET_RD      (0xFF << 8)
#define MSDC_EMMC50_CFG4_ULTRA_EN               (0x3  << 16)
#define MSDC_EMMC50_CFG4_AXI_WRAP_DBG_SEL       (0x1F << 18)

/* EMMC50_BLOCK_LENGTH mask */
#define MSDC_EMMC50_BLOCK_LENGTH_MASK           (0x1FF << 0)

#define EMMC50_CFG_PADCMD_LATCHCK (0x1 << 0)   /* RW */
#define EMMC50_CFG_CRCSTS_EDGE    (0x1 << 3)   /* RW */
#define EMMC50_CFG_CFCSTS_SEL     (0x1 << 4)   /* RW */

/* EMMC_TOP_CONTROL mask */
#define PAD_RXDLY_SEL           (0x1 << 0)      /* RW */
#define PAD_DAT_RD_RXDLY2       (0x1F << 2)     /* RW */
#define PAD_DAT_RD_RXDLY        (0x1F << 7)     /* RW */
#define PAD_DAT_RD_RXDLY2_SEL   (0x1 << 12)     /* RW */
#define PAD_DAT_RD_RXDLY_SEL    (0x1 << 13)     /* RW */
#define DATA_K_VALUE_SEL        (0x1 << 14)     /* RW */

/* EMMC_TOP_CMD mask */
#define PAD_CMD_RXDLY2          (0x1F << 0)     /* RW */
#define PAD_CMD_RXDLY           (0x1F << 5)     /* RW */
#define PAD_CMD_RD_RXDLY2_SEL   (0x1 << 10)     /* RW */
#define PAD_CMD_RD_RXDLY_SEL    (0x1 << 11)     /* RW */

/* TOP_EMMC50_PAD_CTL0 mask */
#define MSDC_PAD_CLK_TXDLY           (0x1F << 10)    /* RW */

/* TOP_EMMC50_PAD_DS_TUNE mask */
#define PAD_DS_DLY3             (0x1F << 0)     /* RW */
#define PAD_DS_DLY2             (0x1F << 5)     /* RW */
#define PAD_DS_DLY1             (0x1F << 10)    /* RW */
#define PAD_DS_DLY2_SEL         (0x1 << 15)     /* RW */
#define PAD_DS_DLY_SEL          (0x1 << 16)     /* RW */

#ifdef CONFIG_MMC_MTK_SDIO
#define SUPPORT_LEGACY_SDIO
#endif

#ifdef SUPPORT_LEGACY_SDIO
#define SDIO_USE_PORT0 0
#define SDIO_USE_PORT1 1
#define SDIO_USE_PORT2 2
#define SDIO_USE_PORT3 3
#define SDIO_USE_PORT SDIO_USE_PORT2

typedef void (*sdio_irq_handler_t)(void *);  /* external irq handler */
typedef void (*pm_callback_t)(pm_message_t state, void *data);

struct sdio_ops {
	void (*sdio_request_eirq)(sdio_irq_handler_t irq_handler, void *data);
	void (*sdio_enable_eirq)(void);
	void (*sdio_disable_eirq)(void);
	void (*sdio_register_pm)(pm_callback_t pm_cb, void *data);
};
extern struct sdio_ops mt_sdio_ops[4];
#endif

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define MSDC_PREPARE_FLAG (0x1 << 0)
#define MSDC_ASYNC_FLAG (0x1 << 1)
#define MSDC_MMAP_FLAG (0x1 << 2)

#define MTK_MMC_AUTOSUSPEND_DELAY	50
#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */

#define PAD_DELAY_MAX	32 /* PAD delay cells */

#define AUTOK_RECOVERABLE_ERROR		-1
#define AUTOK_NONE_RECOVERABLE_ERROR	-2

/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 gpd_info;
#define GPDMA_DESC_HWO		(0x1 << 0)
#define GPDMA_DESC_BDP		(0x1 << 1)
#define GPDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define GPDMA_DESC_INT		(0x1 << 16)
	u32 next;
	u32 ptr;
	u32 gpd_data_len;
#define GPDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
#define GPDMA_DESC_EXTLEN	(0xff << 16) /* bit16 ~ bit23 */
	u32 arg;
	u32 blknum;
	u32 cmd;
};

struct mt_bdma_desc {
	u32 bd_info;
#define BDMA_DESC_EOL		(0x1 << 0)
#define BDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define BDMA_DESC_BLKPAD	(0x1 << 17)
#define BDMA_DESC_DWPAD		(0x1 << 18)
	u32 next;
	u32 ptr;
	u32 bd_data_len;
#define BDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
};

struct msdc_dma {
	struct scatterlist *sg;	/* I/O scatter list */
	struct mt_gpdma_desc *gpd;		/* pointer to gpd array */
	struct mt_bdma_desc *bd;		/* pointer to bd array */
	dma_addr_t gpd_addr;	/* the physical address of gpd array */
	dma_addr_t bd_addr;	/* the physical address of bd array */
};

struct msdc_save_para {
	u32 msdc_cfg;
	u32 iocon;
	u32 sdc_cfg;
	u32 pad_tune0;
	u32 pad_tune1;
	u32 patch_bit0;
	u32 patch_bit1;
	u32 patch_bit2;
	u32 pad_ds_tune;
	u32 emmc50_cfg0;
	u32 msdc_inten;
};

struct msdc_tune_para {
	u32 iocon;
	u32 pad_tune0;
	u32 pad_tune1;
};

struct msdc_delay_phase {
	u8 maxlen;
	u8 start;
	u8 final_phase;
};

struct mt81xx_sdio_compatible {
	bool v3_plus;
	bool top_reg;
};

struct msdc_host {
	struct device *dev;
	struct mmc_host *mmc;	/* mmc structure */
	int cmd_rsp;

	spinlock_t lock;
	spinlock_t irqlock;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	int error;

	void __iomem *base;		/* host base address */
	void __iomem *base_top;		/* host top base address */
	void __iomem *base_gpio;		/* host top base address */
	void __iomem *infra_reset;      /* infra reset 0x10001030 */

	struct msdc_dma dma;	/* dma channel */
	u64 dma_mask;

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */
	u32 tune_latch_ck_cnt;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_uhs;
	struct pinctrl_state *pins_dat1;
	struct pinctrl_state *pins_dat1_eint;
	struct delayed_work req_timeout;
	int irq;		/* host interrupt */
	int eint_irq;
	int sdio_clk_cnt;
	int sdio_irq_cnt; /* irq enable cnt */
	bool irq_thread_alive;

	struct clk *src_clk;	/* msdc source clock */
	struct clk *h_clk;      /* msdc bus_clk */
	struct clk *src_clk_cg;
	u32 mclk;		/* mmc subsystem clock frequency */
	u32 src_clk_freq;	/* source clock frequency */
	u32 sclk;		/* SD/MS bus clock frequency */
	bool clock_on;
	unsigned char timing;
	bool vqmmc_enabled;
	u32 hs400_ds_delay;
	u32 module_reset_bit;
	bool hs400_mode;	/* current eMMC will run at hs400 mode */
	struct msdc_save_para save_para; /* used when gate HCLK */
	struct msdc_tune_para def_tune_para; /* default tune setting */
	struct msdc_tune_para saved_tune_para; /* tune result of CMD21/CMD19 */
	bool autok_done;
	int autok_error;

#ifdef SUPPORT_LEGACY_SDIO
	bool cap_eirq;
	int suspend;
	/* external sdio irq operations */
	void (*request_sdio_eirq)(sdio_irq_handler_t sdio_irq_handler,
				  void *data);
	void (*enable_sdio_eirq)(void);
	void (*disable_sdio_eirq)(void);

	/* power management callback for external module */
	void (*register_pm)(pm_callback_t pm_cb, void *data);
#endif
	const struct mt81xx_sdio_compatible *dev_comp;
};
