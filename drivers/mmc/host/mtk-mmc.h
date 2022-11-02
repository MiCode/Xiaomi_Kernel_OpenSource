/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
 */

#ifndef _MTK_MMC_H_
#define _MTK_MMC_H_

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interconnect.h>
#include <linux/interrupt.h>
#include <linux/reset.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>

#include "cqhci.h"
#include "mtk-mmc-autok.h"

#define MAX_BD_NUM          1024
#define MSDC_NR_CLOCKS      3
#define MSDC_EMMC           0
#define MSDC_SD             1
#define MSDC_SDIO           2
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
#define MSDC_TXDATA      0X18
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
#define SDC_VOL_CHG      0x54
#define SDC_ADV_CFG0     0x64
#define EMMC_IOCON       0x7c
#define SDC_ACMD_RESP    0x80
#define DMA_SA_H4BIT     0x8c
#define MSDC_DMA_SA      0x90
#define MSDC_DMA_CTRL    0x98
#define MSDC_DMA_CFG     0x9c
#define MSDC_DBG_SEL     0xa0
#define MSDC_DBG_OUT     0xa4
#define MSDC_PATCH_BIT   0xb0
#define MSDC_PATCH_BIT1  0xb4
#define MSDC_PATCH_BIT2  0xb8
#define MSDC_PAD_TUNE    0xec
#define MSDC_PAD_TUNE0   0xf0
#define MSDC_PAD_TUNE1   0xf4
#define MSDC_DAT_RDDLY0  0xf8
#define MSDC_DAT_RDDLY1  0xfc
#define MSDC_PAD_CTL0    0x180
#define PAD_DS_TUNE      0x188
#define PAD_CMD_TUNE     0x18c
#define EMMC50_PAD_DAT01_TUNE    0x190
#define EMMC50_PAD_DAT23_TUNE    0x194
#define EMMC50_PAD_DAT45_TUNE    0x198
#define EMMC50_PAD_DAT67_TUNE    0x19c
#define EMMC51_CFG0      0x204
#define EMMC50_CFG0      0x208
#define EMMC50_CFG1      0x20c
#define EMMC50_CFG3      0x220
#define EMMC50_CFG4      0x224
#define SDC_FIFO_CFG     0x228
#define CQHCI_SETTING	 0x7fc

/*--------------------------------------------------------------------------*/
/* Top Pad Register Offset                                                  */
/*--------------------------------------------------------------------------*/
#define EMMC_TOP_CONTROL	0x00
#define EMMC_TOP_CMD		0x04
#define EMMC50_PAD_CTL0		0x08
#define EMMC50_PAD_DS_TUNE	0x0c
#define EMMC50_PAD_DAT0_TUNE	0x10
#define EMMC50_PAD_DAT1_TUNE	0x14
#define EMMC50_PAD_DAT2_TUNE	0x18
#define EMMC50_PAD_DAT3_TUNE	0x1c
#define EMMC50_PAD_DAT4_TUNE	0x20
#define EMMC50_PAD_DAT5_TUNE	0x24
#define EMMC50_PAD_DAT6_TUNE	0x28
#define EMMC50_PAD_DAT7_TUNE	0x2c

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
#define MSDC_CFG_CKDIV          (0xff << 8)	/* RW */
#define MSDC_CFG_CKMOD          (0x3 << 16)	/* RW */
#define MSDC_CFG_HS400_CK_MODE  (0x1 << 18)	/* RW */
#define MSDC_CFG_HS400_CK_MODE_EXTRA (0x1 << 22)/* RW */
#define MSDC_CFG_CKDIV_EXTRA    (0xfff << 8)	/* RW */
#define MSDC_CFG_CKMOD_EXTRA    (0x3 << 20)	/* RW */
#define MSDC_CFG_DVFS_HW        (0x1  << 28)	/* RW */
#define MSDC_CFG_DVFS_EN        (0x1  << 30)	/* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1 << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1 << 1)	/* RW */
#define MSDC_IOCON_DSPL         (0x1 << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (0x1 << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (0x1 << 4)	/* RW */
#define MSDC_IOCON_DSPLSEL      (0x1 << 5)	/* RW */
#define MSDC_IOCON_W_DSPL       (0x1 << 8)	/* RW */
#define MSDC_IOCON_W_DSMPL_SEL  (0x1 << 9)	/* RW */
#define MSDC_IOCON_D0SPL        (0x1 << 16)	/* RW */
#define MSDC_IOCON_D1SPL        (0x1 << 17)	/* RW */
#define MSDC_IOCON_D2SPL        (0x1 << 18)	/* RW */
#define MSDC_IOCON_D3SPL        (0x1 << 19)	/* RW */
#define MSDC_IOCON_D4SPL        (0x1 << 20)	/* RW */
#define MSDC_IOCON_D5SPL        (0x1 << 21)	/* RW */
#define MSDC_IOCON_D6SPL        (0x1 << 22)	/* RW */
#define MSDC_IOCON_D7SPL        (0x1 << 23)	/* RW */
#define MSDC_IOCON_RISCSZ       (0x3 << 24)	/* RW */

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
#define MSDC_INT_CMDQ           (0x1 << 28)	/* W1C */

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
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R  */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R  */
#define MSDC_FIFOCS_CLR         (0x1 << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1 << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1 << 1)	/* RW */
#define SDC_CFG_WRDTOC          (0x1fff << 2)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3 << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1 << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1 << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1 << 21)	/* RW */
#define SDC_CFG_DTOC            (0xff << 24)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (0x1 << 0)	/* RW */
#define SDC_STS_CMDBUSY         (0x1 << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (0x1 << 31)	/* RW */

#define SDC_DAT1_IRQ_TRIGGER	(0x1 << 19)	/* RW */
/* SDC_ADV_CFG0 mask */
#define SDC_RX_ENHANCE_EN	(0x1 << 20)	/* RW */

/* DMA_SA_H4BIT mask */
#define DMA_ADDR_HIGH_4BIT      (0xf << 0)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (0x1 << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (0x1 << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (0x1 << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (0x1 << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (0x1 << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7 << 12)	/* RW */

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (0x1 << 0)	/* R  */
#define MSDC_DMA_CFG_DECSEN     (0x1 << 1)	/* RW */
#define MSDC_DMA_CFG_AHBHPROT2  (0x2 << 8)	/* RW */
#define MSDC_DMA_CFG_ACTIVEEN   (0x2 << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B16B   (0x1 << 16)	/* RW */

/* MSDC_PATCH_BIT mask */
#define MSDC_PATCH_BIT_ODDSUPP    (0x1 <<  1)	/* RW */
#define MSDC_INT_DAT_LATCH_CK_SEL (0x7 <<  7)	/* RW */
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1f << 10)	/* RW */
#define MSDC_PATCH_BIT_RD_DAT_SEL (0x1 << 3)	/* RW */
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
#define MSDC_PATCH_BIT1_WRDAT     (0x7 << 0)	/* R  */
#define MSDC_PATCH_BIT1_CMDTA     (0x7 << 3)	/* RW */
#define MSDC_PB1_GET_BUSY_MA      (0x1 << 6)	/* RW */
#define MSDC_PB1_BUSY_CHECK_SEL   (0x1 << 7)	/* RW */
#define MSDC_PATCH_BIT1_STOP_DLY  (0xf << 8)	/* RW */
#define MSDC_PB1_DDR_CMD_FIX_SEL  (0x1 << 14)	/* RW */

/* MSDC_PATCH_BIT2 mask */
#define MSDC_PATCH_BIT2_CFGRESP   (0x1 << 15)	/* RW */
#define MSDC_PATCH_BIT2_CFGCRCSTS (0x1 << 28)	/* RW */
#define MSDC_PB2_SUPPORT_64G      (0x1 << 1)	/* RW */
#define MSDC_PB2_RESPWAIT         (0x3 << 2)	/* RW */
#define MSDC_PB2_RESPSTSENSEL     (0x7 << 16)	/* RW */
#define MSDC_PB2_DDR50SEL         (0x1 << 19)	/* RW */
#define MSDC_PB2_CRCSTSENSEL      (0x7 << 29)	/* RW */
#define MSDC_PB2_POPENCNT         (0xf << 20)	/* RW */
#define MSDC_PB2_CFGCRCSTSEDGE    (0x1 << 25)	/* RW */

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY	  (0x1f <<  0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY	  (0x1f <<  8)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLYSEL (0x1 << 13)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY	  (0x1f << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLYSEL (0x1 << 21)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY	  (0x1f << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTDLY	  (0x1f << 27)	/* RW */
#define MSDC_PAD_TUNE_RXDLYSEL	  (0x1 << 15)	/* RW */
#define MSDC_PAD_TUNE_RD_SEL	  (0x1 << 13)	/* RW */
#define MSDC_PAD_TUNE_CMD_SEL	  (0x1 << 21)	/* RW */

/* MSDC_PAD_TUNE1 mask */
#define MSDC_PAD_TUNE1_DATRRDLY2    (0x1f << 8)	/* RW */
#define MSDC_PAD_TUNE1_DATRRDLY2SEL (0x1 << 13)	/* RW */
#define MSDC_PAD_TUNE1_CMDRDLY2    (0x1f << 16)	/* RW */
#define MSDC_PAD_TUNE1_CMDRRDLY2SEL (0x1 << 21)	/* RW */

/* MSDC_PAD_CTL0 mask*/
#define MSDC_PAD_CTL0_DCCSEL     (0x1 << 0)	/* RW */
#define MSDC_PAD_CTL0_HLSEL      (0x1 << 1)	/* RW */

/* MSDC_PAD_DS_TUNE mask */
#define PAD_DS_TUNE_DLYSEL        (0x1 << 0)	/* RW */
#define PAD_DS_TUNE_DLY2SEL       (0x1 << 1)	/* RW */
#define PAD_DS_TUNE_DLY1	  (0x1f << 2)	/* RW */
#define PAD_DS_TUNE_DLY2	  (0x1f << 7)	/* RW */
#define PAD_DS_TUNE_DLY3	  (0x1f << 12)	/* RW */

/* MSDC_PAD_CMD_TUNE mask */
#define MSDC_PAD_CMD_TUNE_DLY3SEL (0x1 << 0)	/* RW */
#define MSDC_PAD_CMD_TUNE_RXDLY3  (0x1f << 1)	/* RW */
#define MSDC_PAD_CMD_TUNE_TXDLY   (0x1f << 6)	/* RW */

/* MSDC_PAD_DAT01_TUNE mask */
#define MSDC_PAD_DAT0_TXDLY     (0x1f << 6)	/* RW */
#define MSDC_PAD_DAT1_TXDLY     (0x1f << 22)	/* RW */

/* MSDC_PAD_DAT23_TUNE mask */
#define MSDC_PAD_DAT2_TXDLY     (0x1f << 6)	/* RW */
#define MSDC_PAD_DAT3_TXDLY     (0x1f << 22)	/* RW */

/* MSDC_PAD_DAT45_TUNE mask */
#define MSDC_PAD_DAT4_TXDLY     (0x1f << 6)	/* RW */
#define MSDC_PAD_DAT5_TXDLY     (0x1f << 22)	/* RW */

/* MSDC_PAD_DAT67_TUNE mask */
#define MSDC_PAD_DAT6_TXDLY     (0x1f << 6)	/* RW */
#define MSDC_PAD_DAT7_TXDLY     (0x1f << 22)	/* RW */

/* TOP_EMMC50_PAD_DS_TUNE mask */
#define PAD_DS_DLY3             (0x1f << 0)	/* RW */
#define PAD_DS_DLY2             (0x1f << 5)	/* RW */
#define PAD_DS_DLY1             (0x1f << 10)	/* RW */
#define PAD_DS_DLY2_SEL         (0x1 << 15)	/* RW */
#define PAD_DS_DLY_SEL          (0x1 << 16)	/* RW */

/* TOP_EMMC50_PAD_DAT0_TUNE mask */
#define DAT0_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT0_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT0_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT1_TUNE mask */
#define DAT1_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT1_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT1_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT2_TUNE mask */
#define DAT2_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT2_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT2_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT3_TUNE mask */
#define DAT3_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT3_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT3_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT4_TUNE mask */
#define DAT4_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT4_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT4_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT5_TUNE mask */
#define DAT5_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT5_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT5_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT6_TUNE mask */
#define DAT6_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT6_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT6_TX_DLY         (0x1f << 10)	/* RW */

/* TOP_EMMC50_PAD_DAT7_TUNE mask */
#define DAT7_RD_DLY2            (0x1f << 0)	/* RW */
#define DAT7_RD_DLY1            (0x1f << 5)	/* RW */
#define PAD_DAT7_TX_DLY         (0x1f << 10)	/* RW */

#define PAD_CMD_TUNE_RX_DLY3	  (0x1f << 1)	/* RW */

/* EMMC51_CFG0 mask */
#define CMDQ_RDAT_CNT		  (0x3ff << 12)	/* RW */

/* EMMC50_CFG0 mask */
#define EMMC50_CFG_PADCMD_LATCHCK (0x1 << 0)	/* RW */
#define EMMC50_CFG_CRCSTS_EDGE    (0x1 << 3)	/* RW */
#define EMMC50_CFG_CFCSTS_SEL     (0x1 << 4)	/* RW */
#define EMMC50_CFG_CMD_RESP_SEL   (0x1 << 9)	/* RW */
#define EMMC50_CFG_CMD_EDGE_SEL   (0x1 << 10)	/* RW */
#define EMMC50_CFG_END_BIT_CHK_CNT  (0xf << 5)	/* RW */
#define EMMC50_CFG_READ_DAT_CNT     (0x7 << 21)	/* RW */
#define EMMC50_CFG_TXSKEW_SEL       (0x1 << 29)	/* RW */

/* EMMC50_CFG1 mask */
#define EMMC50_CFG1_DS_CFG        (0x1 << 28)  /* RW */
#define EMMC50_CFG1_CKSWITCH_CNT  (0x7 << 8)	/* RW */

#define EMMC50_CFG3_OUTS_WR       (0x1f << 0)	/* RW */

/* SDC_FIFO_CFG mask */
#define SDC_FIFO_CFG_WRVALIDSEL   (0x1 << 24)	/* RW */
#define SDC_FIFO_CFG_RDVALIDSEL   (0x1 << 25)	/* RW */

/* CQHCI_SETTING */
#define CQHCI_RD_CMD_WND_SEL	  (0x1 << 14) /* RW */
#define CQHCI_WR_CMD_WND_SEL	  (0x1 << 15) /* RW */

/* EMMC_TOP_CONTROL mask */
#define PAD_RXDLY_SEL           (0x1 << 0)	/* RW */
#define DELAY_EN                (0x1 << 1)	/* RW */
#define PAD_DAT_RD_RXDLY2       (0x1f << 2)	/* RW */
#define PAD_DAT_RD_RXDLY        (0x1f << 7)	/* RW */
#define PAD_DAT_RD_RXDLY2_SEL   (0x1 << 12)	/* RW */
#define PAD_DAT_RD_RXDLY_SEL    (0x1 << 13)	/* RW */
#define DATA_K_VALUE_SEL        (0x1 << 14)	/* RW */
#define SDC_RX_ENH_EN           (0x1 << 15)	/* TW */

/* EMMC_TOP_CMD mask */
#define PAD_CMD_RXDLY2          (0x1f << 0)	/* RW */
#define PAD_CMD_RXDLY           (0x1f << 5)	/* RW */
#define PAD_CMD_RD_RXDLY2_SEL   (0x1 << 10)	/* RW */
#define PAD_CMD_RD_RXDLY_SEL    (0x1 << 11)	/* RW */
#define PAD_CMD_TX_DLY          (0x1f << 12)	/* RW */

/* TOP_EMMC50_PAD_CTL0 mask */
#define HL_SEL                  (0x1 << 0)	/* RW */
#define DCC_SEL                 (0x1 << 1)	/* RW */
#define PAD_CLK_TXDLY           (0x1f << 10)	/* RW */

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define MSDC_PREPARE_FLAG (0x1 << 0)
#define MSDC_ASYNC_FLAG   (0x1 << 1)
#define MSDC_MMAP_FLAG    (0x1 << 2)

#define MTK_MMC_AUTOSUSPEND_DELAY	50
#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */

#define DEFAULT_DEBOUNCE	(8)	/* 8 cycles CD debounce */
#define PAD_DELAY_MAX		32	/* PAD delay cells */

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#define MSDC_OCR_AVAIL\
	(MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 \
	| MMC_VDD_31_32 | MMC_VDD_32_33)
#define FPGA_SRC_CLK		10000000
#endif

#define VOL_CHG_CNT_DEFAULT_VAL		0x1F4 /* =500 */
#define SDC_CMD_VOLSWTH			(0x1 << 30) /* RW */
#define SDC_VOL_CHG_CNT			(0xffff << 0) /* RW  */

/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 gpd_info;
#define GPDMA_DESC_HWO		(0x1 << 0)
#define GPDMA_DESC_BDP		(0x1 << 1)
#define GPDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define GPDMA_DESC_INT		(0x1 << 16)
#define GPDMA_DESC_NEXT_H4	(0xf << 24)
#define GPDMA_DESC_PTR_H4	(0xf << 28)
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
#define BDMA_DESC_NEXT_H4	(0xf << 24)
#define BDMA_DESC_PTR_H4	(0xf << 28)
	u32 next;
	u32 ptr;
	u32 bd_data_len;
#define BDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
#define BDMA_DESC_BUFLEN_EXT	(0xffffff) /* bit0 ~ bit23 */
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
	u32 pad_tune;
	u32 patch_bit0;
	u32 patch_bit1;
	u32 patch_bit2;
	u32 pad_ds_tune;
	u32 pad_cmd_tune;
	u32 emmc50_cfg0;
	u32 emmc50_cfg3;
	u32 sdc_fifo_cfg;
	u32 emmc_top_control;
	u32 emmc_top_cmd;
	u32 emmc50_pad_ds_tune;
	/* for autok */
	u32 cmd_edge;    /* command latch edge */
	u32 rdata_edge;  /* read data latch edge */
	u32 wdata_edge;  /* write data latch edge */
	u32 sdc_adv_cfg0;
	u32 pad_tune1;
	u32 ds_dly1;
	u32 ds_dly3;
	u32 emmc50_pad_cmd_tune;
	u32 emmc50_dat01;
	u32 emmc50_dat23;
	u32 emmc50_dat45;
	u32 emmc50_dat67;

	/* msdc top reg  */
	u32 top_emmc50_pad_ctl0;
	u32 top_emmc50_pad_dat_tune[8];
};

struct mtk_mmc_compatible {
	u8 clk_div_bits;
	bool recheck_sdio_irq;
	bool hs400_tune; /* only used for MT8173 */
	u32 pad_tune_reg;
	bool async_fifo;
	bool data_tune;
	bool busy_check;
	bool stop_clk_fix;
	bool enhance_rx;
	bool support_64g;
	bool use_internal_cd;
	bool need_gate_cg;
};

struct msdc_tune_para {
	u32 iocon;
	u32 pad_tune;
	u32 pad_cmd_tune;
	u32 emmc_top_control;
	u32 emmc_top_cmd;
};

struct msdc_delay_phase {
	u8 maxlen;
	u8 start;
	u8 final_phase;
};

struct reg_oc_msdc {
	struct notifier_block nb;
	struct work_struct work;
};

struct msdc_host {
	struct device *dev;
	const struct mtk_mmc_compatible *dev_comp;
	int cmd_rsp;

	spinlock_t lock;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	int error;

	void __iomem *base;		/* host base address */
	void __iomem *top_base;		/* host top register base address */

	struct msdc_dma dma;	/* dma channel */
	u64 dma_mask;

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_uhs;
	struct delayed_work req_timeout;
	int irq;		/* host interrupt */
	int eint_irq;	        /* device interrupt */
	int sdio_irq_cnt;       /* irq enable cnt */
	struct reset_control *reset;

	struct clk *src_clk;	/* msdc source clock */
	struct clk *h_clk;      /* msdc h_clk */
	struct clk *bus_clk;	/* bus clock which used to access register */
	struct clk *crypto_clk;    /* msdc crypto clock */
	struct clk *crypto_cg;     /* msdc crypto clock control gate */
	struct clk *src_clk_cg; /* msdc source clock control gate */
	struct clk *sys_clk_cg;	/* msdc subsys clock control gate */
	struct clk_bulk_data bulk_clks[MSDC_NR_CLOCKS];
	u32 mclk;		/* mmc subsystem clock frequency */
	u32 src_clk_freq;	/* source clock frequency */
	unsigned char timing;
	bool vqmmc_enabled;
	u32 latch_ck;
	u32 hs400_ds_delay;
	u32 hs200_cmd_int_delay; /* cmd internal delay for HS200/SDR104 */
	u32 hs400_cmd_int_delay; /* cmd internal delay for HS400 */
	bool hs400_cmd_resp_sel_rising;
				 /* cmd response sample selection for HS400 */
	bool hs400_mode;	/* current eMMC will run at hs400 mode */
	bool internal_cd;	/* Use internal card-detect logic */
	bool cqhci;		/* support eMMC HW CMDQ */
	struct msdc_save_para save_para; /* used when gate HCLK */
	struct msdc_tune_para def_tune_para; /* default tune setting */
	struct msdc_tune_para saved_tune_para; /* tune result of CMD21/CMD19 */
	struct cqhci_host *cq_host;
	struct reg_oc_msdc sd_oc;
	/* autok */
	int	id;		/* host id */
	bool tuning_in_progress;
	u32 need_tune;
	bool is_autok_done;
	int autok_error;
	u32 tune_latch_ck_cnt;
	u8 autok_res[AUTOK_VCORE_NUM+1][TUNING_PARA_SCAN_COUNT];
	u8 card_inserted;  /* the status of card inserted */
	bool block_bad_card;
	int retune_times;
	int power_cycle_cnt;
	u32 data_timeout_cont; /* data continuous timeout */
	/* set vcore floor */
	u32 req_vcore;
	u32 ocr_volt;
	struct regulator *dvfsrc_vcore_power;
	bool use_cmd_intr;
	struct pm_qos_request pm_qos_req;
	bool qos_enable;
	struct icc_path *bw_path;
	unsigned int peak_bw;
};

/*--------------------------------------------------------------------------*/
/* SDCard error handler                                                     */
/*--------------------------------------------------------------------------*/
/* if continuous data timeout reach the limit */
/* driver will force remove card */
#define MSDC_MAX_DATA_TIMEOUT_CONTINUOUS (100)

/* if continuous power cycle fail reach the limit */
/* driver will force remove card */
#define MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS (3)

/* count of bad sd detecter (or bad sd condition kinds),
 * we can add it here if has other condition
 */
#define BAD_SD_DETECTER_COUNT 1

/* we take it as bad sd when the bad sd condition occurs
 * out of tolerance
 */
u32 bad_sd_tolerance[BAD_SD_DETECTER_COUNT] = {10};

/* bad sd condition occur times
 */
u32 bad_sd_detecter[BAD_SD_DETECTER_COUNT] = {0};

/* bad sd condition occur times will reset to zero by self
 * when reach the forget time (when set to 0, means not
 * reset to 0 by self), unit:s
 */
u32 bad_sd_forget[BAD_SD_DETECTER_COUNT] = {3};

/* the latest occur time of the bad sd condition,
 * unit: clock
 */
unsigned long bad_sd_timer[BAD_SD_DETECTER_COUNT] = {0};

#endif  /* _MTK_MMC_H_ */
