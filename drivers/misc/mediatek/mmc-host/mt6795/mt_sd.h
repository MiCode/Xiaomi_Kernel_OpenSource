#ifndef MT_SD_H
#define  MT_SD_H

#include <linux/bitops.h>
#include <linux/mmc/host.h>
#include <mach/sync_write.h>

#define MTK_MSDC_USE_CMD23
#if defined(CONFIG_MTK_EMMC_CACHE) && defined(MTK_MSDC_USE_CMD23)
#define MTK_MSDC_USE_CACHE
#define MTK_MSDC_USE_EDC_EMMC_CACHE (1)
#else
#define MTK_MSDC_USE_EDC_EMMC_CACHE (0)
#endif

#ifdef MTK_MSDC_USE_CMD23
#define MSDC_USE_AUTO_CMD23   (1)
#endif

#ifdef MTK_MSDC_USE_CACHE
#ifndef MMC_ENABLED_EMPTY_QUEUE_FLUSH
/* #define MTK_MSDC_FLUSH_BY_CLK_GATE */
#endif
#endif

#define HOST_MAX_NUM        (4)
#define MAX_REQ_SZ                       (512 * 1024)

#define CMD_TUNE_UHS_MAX_TIME            (2*32*8*8)
#define CMD_TUNE_HS_MAX_TIME             (2*32)

#define READ_TUNE_UHS_CLKMOD1_MAX_TIME   (2*32*32*8)
#define READ_TUNE_UHS_MAX_TIME           (2*32*32)
#define READ_TUNE_HS_MAX_TIME            (2*32)

#define WRITE_TUNE_HS_MAX_TIME           (2*32)
#define WRITE_TUNE_UHS_MAX_TIME          (2*32*8)

#ifdef CONFIG_EMMC_50_FEATURE
#define MAX_HS400_TUNE_COUNT (576) //(32*18)
#endif 

#define MAX_GPD_NUM         (1 + 1)	/* one null gpd */
#define MAX_BD_NUM          (1024)
#define MAX_BD_PER_GPD      (MAX_BD_NUM)
#define CLK_SRC_MAX_NUM        (1)

#define EINT_MSDC1_INS_POLARITY         (0)
#define SDIO_ERROR_BYPASS

/* #define MSDC_CLKSRC_REG     (0xf100000C) */

#ifdef CONFIG_SDIOAUTOK_SUPPORT
#define MTK_SDIO30_ONLINE_TUNING_SUPPORT
/* #define OT_LATENCY_TEST */
#endif				/* CONFIG_SDIOAUTOK_SUPPORT */
/* #define ONLINE_TUNING_DVTTEST */

#ifdef CONFIG_MTK_FPGA
#define FPGA_PLATFORM
#else
#define MTK_MSDC_BRINGUP_DEBUG
#endif
/* #define MTK_MSDC_DUMP_FIFO */

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)
#if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST)
#define MSDC_AUTOCMD53          (0x0004)
#endif				/* #if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST) */
/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#define REG_ADDR(x)                 ((volatile u32*)(base + OFFSET_##x))

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            (128)
#define MSDC_FIFO_THD           (64)	/* (128) */
#define MSDC_NUM                (4)

#define MSDC_MS                 (0)	/* No memory stick mode, 0 use to gate clock */
#define MSDC_SDMMC              (1)

#define MSDC_MODE_UNKNOWN       (0)
#define MSDC_MODE_PIO           (1)
#define MSDC_MODE_DMA_BASIC     (2)
#define MSDC_MODE_DMA_DESC      (3)
#define MSDC_MODE_DMA_ENHANCED  (4)
#define MSDC_MODE_MMC_STREAM    (5)

#define MSDC_BUS_1BITS          (0)
#define MSDC_BUS_4BITS          (1)
#define MSDC_BUS_8BITS          (2)

#define MSDC_BRUST_8B           (3)
#define MSDC_BRUST_16B          (4)
#define MSDC_BRUST_32B          (5)
#define MSDC_BRUST_64B          (6)

#define MSDC_PIN_PULL_NONE      (0)
#define MSDC_PIN_PULL_DOWN      (1)
#define MSDC_PIN_PULL_UP        (2)
#define MSDC_PIN_KEEP           (3)

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

#define MSDC_EMMC_BOOTMODE0     (0)	/* Pull low CMD mode */
#define MSDC_EMMC_BOOTMODE1     (1)	/* Reset CMD mode */

enum {
	RESP_NONE = 0,
	RESP_R1,
	RESP_R2,
	RESP_R3,
	RESP_R4,
	RESP_R5,
	RESP_R6,
	RESP_R7,
	RESP_R1B
};

/*--------------------------------------------------------------------------*/
/* Register Offset                                                          */
/*--------------------------------------------------------------------------*/
#define OFFSET_MSDC_CFG                  (0x0)
#define OFFSET_MSDC_IOCON                (0x04)
#define OFFSET_MSDC_PS                   (0x08)
#define OFFSET_MSDC_INT                  (0x0c)
#define OFFSET_MSDC_INTEN                (0x10)
#define OFFSET_MSDC_FIFOCS               (0x14)
#define OFFSET_MSDC_TXDATA               (0x18)
#define OFFSET_MSDC_RXDATA               (0x1c)
#define OFFSET_SDC_CFG                   (0x30)
#define OFFSET_SDC_CMD                   (0x34)
#define OFFSET_SDC_ARG                   (0x38)
#define OFFSET_SDC_STS                   (0x3c)
#define OFFSET_SDC_RESP0                 (0x40)
#define OFFSET_SDC_RESP1                 (0x44)
#define OFFSET_SDC_RESP2                 (0x48)
#define OFFSET_SDC_RESP3                 (0x4c)
#define OFFSET_SDC_BLK_NUM               (0x50)
#define OFFSET_SDC_CSTS                  (0x58)
#define OFFSET_SDC_CSTS_EN               (0x5c)
#define OFFSET_SDC_DCRC_STS              (0x60)
#define OFFSET_EMMC_CFG0                 (0x70)
#define OFFSET_EMMC_CFG1                 (0x74)
#define OFFSET_EMMC_STS                  (0x78)
#define OFFSET_EMMC_IOCON                (0x7c)
#define OFFSET_SDC_ACMD_RESP             (0x80)
#define OFFSET_SDC_ACMD19_TRG            (0x84)
#define OFFSET_SDC_ACMD19_STS            (0x88)
#define OFFSET_MSDC_DMA_SA               (0x90)
#define OFFSET_MSDC_DMA_CA               (0x94)
#define OFFSET_MSDC_DMA_CTRL             (0x98)
#define OFFSET_MSDC_DMA_CFG              (0x9c)
#define OFFSET_MSDC_DBG_SEL              (0xa0)
#define OFFSET_MSDC_DBG_OUT              (0xa4)
#define OFFSET_MSDC_DMA_LEN              (0xa8)
#define OFFSET_MSDC_PATCH_BIT0           (0xb0)
#define OFFSET_MSDC_PATCH_BIT1           (0xb4)
#define OFFSET_DAT0_TUNE_CRC             (0xc0)
#define OFFSET_DAT1_TUNE_CRC             (0xc4)
#define OFFSET_DAT2_TUNE_CRC             (0xc8)
#define OFFSET_DAT3_TUNE_CRC             (0xcc)
#define OFFSET_CMD_TUNE_CRC              (0xd0)
#define OFFSET_SDIO_TUNE_WIND            (0xd4)
#define OFFSET_MSDC_PAD_TUNE             (0xec)
#define OFFSET_MSDC_DAT_RDDLY0           (0xf0)
#define OFFSET_MSDC_DAT_RDDLY1           (0xf4)
#define OFFSET_MSDC_HW_DBG               (0xf8)
#define OFFSET_MSDC_VERSION              (0x100)
#define OFFSET_MSDC_ECO_VER              (0x104)
#define OFFSET_EMMC50_PAD_CTL0           (0x180)
#define OFFSET_EMMC50_PAD_DS_CTL0        (0x184)
#define OFFSET_EMMC50_PAD_DS_TUNE        (0x188)
#define OFFSET_EMMC50_PAD_CMD_TUNE       (0x18c)
#define OFFSET_EMMC50_PAD_DAT01_TUNE     (0x190)
#define OFFSET_EMMC50_PAD_DAT23_TUNE     (0x194)
#define OFFSET_EMMC50_PAD_DAT45_TUNE     (0x198)
#define OFFSET_EMMC50_PAD_DAT67_TUNE     (0x19c)
#define OFFSET_EMMC50_CFG0               (0x208)
#define OFFSET_EMMC50_CFG1               (0x20c)
#define OFFSET_EMMC50_CFG2               (0x21c)
#define OFFSET_EMMC50_CFG3               (0x220)
#define OFFSET_EMMC50_CFG4               (0x224)


/*--------------------------------------------------------------------------*/
/* Register Address                                                         */
/*--------------------------------------------------------------------------*/

/* common register */
#define MSDC_CFG                    REG_ADDR(MSDC_CFG)
#define MSDC_IOCON                  REG_ADDR(MSDC_IOCON)
#define MSDC_PS                     REG_ADDR(MSDC_PS)
#define MSDC_INT                    REG_ADDR(MSDC_INT)
#define MSDC_INTEN                  REG_ADDR(MSDC_INTEN)
#define MSDC_FIFOCS                 REG_ADDR(MSDC_FIFOCS)
#define MSDC_TXDATA                 REG_ADDR(MSDC_TXDATA)
#define MSDC_RXDATA                 REG_ADDR(MSDC_RXDATA)

/* sdmmc register */
#define SDC_CFG                     REG_ADDR(SDC_CFG)
#define SDC_CMD                     REG_ADDR(SDC_CMD)
#define SDC_ARG                     REG_ADDR(SDC_ARG)
#define SDC_STS                     REG_ADDR(SDC_STS)
#define SDC_RESP0                   REG_ADDR(SDC_RESP0)
#define SDC_RESP1                   REG_ADDR(SDC_RESP1)
#define SDC_RESP2                   REG_ADDR(SDC_RESP2)
#define SDC_RESP3                   REG_ADDR(SDC_RESP3)
#define SDC_BLK_NUM                 REG_ADDR(SDC_BLK_NUM)
#define SDC_CSTS                    REG_ADDR(SDC_CSTS)
#define SDC_CSTS_EN                 REG_ADDR(SDC_CSTS_EN)
#define SDC_DCRC_STS                REG_ADDR(SDC_DCRC_STS)

/* emmc register*/
#define EMMC_CFG0                   REG_ADDR(EMMC_CFG0)
#define EMMC_CFG1                   REG_ADDR(EMMC_CFG1)
#define EMMC_STS                    REG_ADDR(EMMC_STS)
#define EMMC_IOCON                  REG_ADDR(EMMC_IOCON)

/* auto command register */
#define SDC_ACMD_RESP               REG_ADDR(SDC_ACMD_RESP)
#define SDC_ACMD19_TRG              REG_ADDR(SDC_ACMD19_TRG)
#define SDC_ACMD19_STS              REG_ADDR(SDC_ACMD19_STS)

/* dma register */
#define MSDC_DMA_SA                 REG_ADDR(MSDC_DMA_SA)
#define MSDC_DMA_CA                 REG_ADDR(MSDC_DMA_CA)
#define MSDC_DMA_CTRL               REG_ADDR(MSDC_DMA_CTRL)
#define MSDC_DMA_CFG                REG_ADDR(MSDC_DMA_CFG)
#define MSDC_DMA_LEN                REG_ADDR(MSDC_DMA_LEN)

/* data read delay */
#define MSDC_DAT_RDDLY0             REG_ADDR(MSDC_DAT_RDDLY0)
#define MSDC_DAT_RDDLY1             REG_ADDR(MSDC_DAT_RDDLY1)

/* debug register */
#define MSDC_DBG_SEL                REG_ADDR(MSDC_DBG_SEL)
#define MSDC_DBG_OUT                REG_ADDR(MSDC_DBG_OUT)

/* misc register */
#define MSDC_PATCH_BIT0             REG_ADDR(MSDC_PATCH_BIT0)
#define MSDC_PATCH_BIT1             REG_ADDR(MSDC_PATCH_BIT1)
#define DAT0_TUNE_CRC               REG_ADDR(DAT0_TUNE_CRC)
#define DAT1_TUNE_CRC               REG_ADDR(DAT1_TUNE_CRC)
#define DAT2_TUNE_CRC               REG_ADDR(DAT2_TUNE_CRC)
#define DAT3_TUNE_CRC               REG_ADDR(DAT3_TUNE_CRC)
#define CMD_TUNE_CRC                REG_ADDR(CMD_TUNE_CRC)
#define SDIO_TUNE_WIND              REG_ADDR(SDIO_TUNE_WIND)
#define MSDC_PAD_TUNE               REG_ADDR(MSDC_PAD_TUNE)
#define MSDC_HW_DBG                 REG_ADDR(MSDC_HW_DBG)
#define MSDC_VERSION                REG_ADDR(MSDC_VERSION)
#define MSDC_ECO_VER                REG_ADDR(MSDC_ECO_VER)

/* eMMC 5.0 register */
#define EMMC50_PAD_DS_TUNE          REG_ADDR(EMMC50_PAD_DS_TUNE)
#define EMMC50_PAD_CMD_TUNE         REG_ADDR(EMMC50_PAD_CMD_TUNE)
#define EMMC50_PAD_DAT01_TUNE       REG_ADDR(EMMC50_PAD_DAT01_TUNE)
#define EMMC50_PAD_DAT23_TUNE       REG_ADDR(EMMC50_PAD_DAT23_TUNE)
#define EMMC50_PAD_DAT45_TUNE       REG_ADDR(EMMC50_PAD_DAT45_TUNE)
#define EMMC50_PAD_DAT67_TUNE       REG_ADDR(EMMC50_PAD_DAT67_TUNE)
#define EMMC50_CFG0                 REG_ADDR(EMMC50_CFG0)
#define EMMC50_CFG1                 REG_ADDR(EMMC50_CFG1)
#define EMMC50_CFG2                 REG_ADDR(EMMC50_CFG2)
#define EMMC50_CFG3                 REG_ADDR(EMMC50_CFG3)
#define EMMC50_CFG4                 REG_ADDR(EMMC50_CFG4)


/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

/* MSDC_CFG mask */
#define MSDC_CFG_MODE           (0x1  << 0)	/* RW */
#define MSDC_CFG_CKPDN          (0x1  << 1)	/* RW */
#define MSDC_CFG_RST            (0x1  << 2)	/* A0 */
#define MSDC_CFG_PIO            (0x1  << 3)	/* RW */
#define MSDC_CFG_CKDRVEN        (0x1  << 4)	/* RW */
#define MSDC_CFG_BV18SDT        (0x1  << 5)	/* RW */
#define MSDC_CFG_BV18PSS        (0x1  << 6)	/* R  */
#define MSDC_CFG_CKSTB          (0x1  << 7)	/* R  */
#define MSDC_CFG_CKDIV          (0xff << 8)	/* RW */
#define MSDC_CFG_CKMOD          (0x3  << 16)	/* W1C */
#define MSDC_CFG_CKMOD_HS400    (0x1  << 18)	/* RW */
#define MSDC_CFG_START_BIT      (0x3  << 19)	/* RW */
#define MSDC_CFG_SCLK_STOP_DDR  (0x1  << 21)	/* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1  << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1  << 1)	/* RW */
#define MSDC_IOCON_R_D_SMPL         (0x1  << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (0x1  << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (0x1  << 4)	/* RW */
#define MSDC_IOCON_R_D_SMPL_SEL      (0x1  << 5)	/* RW */
#define MSDC_IOCON_W_D_SMPL     (0x1  << 8)	/* RW */
#define MSDC_IOCON_W_D_SMPL_SEL (0x1  << 9)	/* RW */
#define MSDC_IOCON_W_D0SPL      (0x1  << 10)	/* RW */
#define MSDC_IOCON_W_D1SPL      (0x1  << 11)	/* RW */
#define MSDC_IOCON_W_D2SPL      (0x1  << 12)	/* RW */
#define MSDC_IOCON_W_D3SPL      (0x1  << 13)	/* RW */
#define MSDC_IOCON_R_D0SPL        (0x1  << 16)	/* RW */
#define MSDC_IOCON_R_D1SPL        (0x1  << 17)	/* RW */
#define MSDC_IOCON_R_D2SPL        (0x1  << 18)	/* RW */
#define MSDC_IOCON_R_D3SPL        (0x1  << 19)	/* RW */
#define MSDC_IOCON_R_D4SPL        (0x1  << 20)	/* RW */
#define MSDC_IOCON_R_D5SPL        (0x1  << 21)	/* RW */
#define MSDC_IOCON_R_D6SPL        (0x1  << 22)	/* RW */
#define MSDC_IOCON_R_D7SPL        (0x1  << 23)	/* RW */
#define MSDC_IOCON_RISCSZ       (0x3  << 24)	/* RW */

/* MSDC_PS mask */
#define MSDC_PS_CDEN            (0x1  << 0)	/* RW */
#define MSDC_PS_CDSTS           (0x1  << 1)	/* R  */
#define MSDC_PS_CDDEBOUNCE      (0xf  << 12)	/* RW */
#define MSDC_PS_DAT             (0xff << 16)	/* R  */
#define MSDC_PS_CMD             (0x1  << 24)	/* R  */
#define MSDC_PS_WP              (0x1UL<< 31)    /* R  */

/* MSDC_INT mask */
#define MSDC_INT_MMCIRQ         (0x1  << 0)	/* W1C */
#define MSDC_INT_CDSC           (0x1  << 1)	/* W1C */
#define MSDC_INT_ACMDRDY        (0x1  << 3)	/* W1C */
#define MSDC_INT_ACMDTMO        (0x1  << 4)	/* W1C */
#define MSDC_INT_ACMDCRCERR     (0x1  << 5)	/* W1C */
#define MSDC_INT_DMAQ_EMPTY     (0x1  << 6)	/* W1C */
#define MSDC_INT_SDIOIRQ        (0x1  << 7)	/* W1C */
#define MSDC_INT_CMDRDY         (0x1  << 8)	/* W1C */
#define MSDC_INT_CMDTMO         (0x1  << 9)	/* W1C */
#define MSDC_INT_RSPCRCERR      (0x1  << 10)	/* W1C */
#define MSDC_INT_CSTA           (0x1  << 11)	/* R */
#define MSDC_INT_XFER_COMPL     (0x1  << 12)	/* W1C */
#define MSDC_INT_DXFER_DONE     (0x1  << 13)	/* W1C */
#define MSDC_INT_DATTMO         (0x1  << 14)	/* W1C */
#define MSDC_INT_DATCRCERR      (0x1  << 15)	/* W1C */
#define MSDC_INT_ACMD19_DONE    (0x1  << 16)	/* W1C */
#define MSDC_INT_BDCSERR        (0x1  << 17)	/* W1C */
#define MSDC_INT_GPDCSERR       (0x1  << 18)	/* W1C */
#define MSDC_INT_DMAPRO         (0x1  << 19)	/* W1C */
#define MSDC_INT_AXI_RESP_ERR   (0x1  << 23)	/* W1C */
#if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST)
#define MSDC_INT_GEAR_OUT_BOUND (0x1  << 20)	/* W1C */
#define MSDC_INT_ACMD53_DONE    (0x1  << 21)	/* W1C */
#define MSDC_INT_ACMD53_FAIL    (0x1  << 22)	/* W1C */
#endif				/* #if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST) */

/* MSDC_INTEN mask */
#define MSDC_INTEN_MMCIRQ       (0x1  << 0)	/* RW */
#define MSDC_INTEN_CDSC         (0x1  << 1)	/* RW */
#define MSDC_INTEN_ACMDRDY      (0x1  << 3)	/* RW */
#define MSDC_INTEN_ACMDTMO      (0x1  << 4)	/* RW */
#define MSDC_INTEN_ACMDCRCERR   (0x1  << 5)	/* RW */
#define MSDC_INTEN_DMAQ_EMPTY   (0x1  << 6)	/* RW */
#define MSDC_INTEN_SDIOIRQ      (0x1  << 7)	/* RW */
#define MSDC_INTEN_CMDRDY       (0x1  << 8)	/* RW */
#define MSDC_INTEN_CMDTMO       (0x1  << 9)	/* RW */
#define MSDC_INTEN_RSPCRCERR    (0x1  << 10)	/* RW */
#define MSDC_INTEN_CSTA         (0x1  << 11)	/* RW */
#define MSDC_INTEN_XFER_COMPL   (0x1  << 12)	/* RW */
#define MSDC_INTEN_DXFER_DONE   (0x1  << 13)	/* RW */
#define MSDC_INTEN_DATTMO       (0x1  << 14)	/* RW */
#define MSDC_INTEN_DATCRCERR    (0x1  << 15)	/* RW */
#define MSDC_INTEN_ACMD19_DONE  (0x1  << 16)	/* RW */
#define MSDC_INTEN_BDCSERR      (0x1  << 17)	/* RW */
#define MSDC_INTEN_GPDCSERR     (0x1  << 18)	/* RW */
#define MSDC_INTEN_DMAPRO       (0x1  << 19)	/* RW */
#define MSDC_INTEN_GOBOUND      (0x1  << 20)	/* RW */
#define MSDC_INTEN_ACMD53_DONE  (0x1  << 21)	/* RW */
#define MSDC_INTEN_ACMD53_FAIL  (0x1  << 22)	/* RW */
#define MSDC_INTEN_AXI_RESP_ERR (0x1  << 23)	/* RW */


/* MSDC_FIFOCS mask */
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R */
#define MSDC_FIFOCS_CLR         (0x1UL<< 31)    /* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1  << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1  << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3  << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1  << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1  << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1  << 21)	/* RW */
#define SDC_CFG_DTOC            (0xffUL << 24)  /* RW */

/* SDC_CMD mask */
#define SDC_CMD_OPC             (0x3f << 0)	/* RW */
#define SDC_CMD_BRK             (0x1  << 6)	/* RW */
#define SDC_CMD_RSPTYP          (0x7  << 7)	/* RW */
#define SDC_CMD_DTYP            (0x3  << 11)	/* RW */
#define SDC_CMD_DTYP            (0x3  << 11)	/* RW */
#define SDC_CMD_RW              (0x1  << 13)	/* RW */
#define SDC_CMD_STOP            (0x1  << 14)	/* RW */
#define SDC_CMD_GOIRQ           (0x1  << 15)	/* RW */
#define SDC_CMD_BLKLEN          (0xfff << 16)	/* RW */
#define SDC_CMD_AUTOCMD         (0x3  << 28)	/* RW */
#define SDC_CMD_VOLSWTH         (0x1  << 30)	/* RW */
#define SDC_CMD_ACMD53          (0x1UL  << 31)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (0x1  << 0)	/* RW */
#define SDC_STS_CMDBUSY         (0x1  << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (0x1UL  << 31)	/* RW */

/* SDC_DCRC_STS mask */
#define SDC_DCRC_STS_NEG        (0xff << 8)	/* RO */
#define SDC_DCRC_STS_POS        (0xff << 0)	/* RO */

/* EMMC_CFG0 mask */
#define EMMC_CFG0_BOOTSTART     (0x1  << 0)	/* W */
#define EMMC_CFG0_BOOTSTOP      (0x1  << 1)	/* W */
#define EMMC_CFG0_BOOTMODE      (0x1  << 2)	/* RW */
#define EMMC_CFG0_BOOTACKDIS    (0x1  << 3)	/* RW */
#define EMMC_CFG0_BOOTWDLY      (0x7  << 12)	/* RW */
#define EMMC_CFG0_BOOTSUPP      (0x1  << 15)	/* RW */

/* EMMC_CFG1 mask */
#define EMMC_CFG1_BOOTDATTMC    (0xfffff << 0)	/* RW */
#define EMMC_CFG1_BOOTACKTMC    (0xfffUL << 20) /* RW */

/* EMMC_STS mask */
#define EMMC_STS_BOOTCRCERR     (0x1  << 0)	/* W1C */
#define EMMC_STS_BOOTACKERR     (0x1  << 1)	/* W1C */
#define EMMC_STS_BOOTDATTMO     (0x1  << 2)	/* W1C */
#define EMMC_STS_BOOTACKTMO     (0x1  << 3)	/* W1C */
#define EMMC_STS_BOOTUPSTATE    (0x1  << 4)	/* R */
#define EMMC_STS_BOOTACKRCV     (0x1  << 5)	/* W1C */
#define EMMC_STS_BOOTDATRCV     (0x1  << 6)	/* R */

/* EMMC_IOCON mask */
#define EMMC_IOCON_BOOTRST      (0x1  << 0)	/* RW */

/* SDC_ACMD19_TRG mask */
#define SDC_ACMD19_TRG_TUNESEL  (0xf  << 0)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (0x1  << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (0x1  << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (0x1  << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (0x1  << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (0x1  << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7  << 12)	/* RW */

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (0x1  << 0)	/* R */
#define MSDC_DMA_CFG_DECSEN     (0x1  << 1)	/* RW */
#define MSDC_DMA_CFG_AHBEN      (0x3  << 8)	/* RW */
#define MSDC_DMA_CFG_ACTEN      (0x3  << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B      (0x1  << 16)	/* RW */
#define MSDC_DMA_CFG_OUTB_STOP  (0x1  << 17)	/* RW */

/* MSDC_PATCH_BIT mask */
/* #define CKGEN_RX_SDClKO_SEL     (0x1  << 0) */    /*This bit removed on MT6589/MT8585*/
#define MSDC_PATCH_BIT_ODDSUPP    (0x1  <<  1)	/* RW */

#if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST)
#define MSDC_MASK_ACMD53_CRC_ERR_INTR   (0x1<<4)
#define MSDC_ACMD53_FAIL_ONE_SHOT       (0X1<<5)
#endif				/* #if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST) */

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY  (0x1F << 0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY  (0x1F << 8)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY   (0x1F << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY  (0x1F << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTXDLY  (0x1FUL << 27)	/* RW */

/* MSDC_DAT_RDDLY0/1 mask */
#define MSDC_DAT_RDDLY0_D3      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY0_D2      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY0_D1      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY0_D0      (0x1F << 24)	/* RW */

#define MSDC_DAT_RDDLY1_D7      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY1_D6      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY1_D5      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY1_D4      (0x1F << 24)	/* RW */

/* MSDC_PATCH_BIT0 mask */
#define MSDC_PB0_RESV1           (0x1 << 0)
#define MSDC_PB0_EN_8BITSUP      (0x1 << 1)
#define MSDC_PB0_DIS_RECMDWR     (0x1 << 2)
#define MSDC_PB0_RESV2           (0x1 << 3)
#define MSDC_PB0_ACMD53_CRCINTR  (0x1 << 4)
#define MSDC_PB0_ACMD53_ONESHOT  (0x1 << 5)
#define MSDC_PB0_RESV3           (0x1 << 6)
#define MSDC_PB0_INT_DAT_LATCH_CK_SEL (0x7 << 7)
#define MSDC_PB0_CKGEN_MSDC_DLY_SEL   (0x1F<<10)
#define MSDC_PB0_FIFORD_DIS      (0x1 << 15)
#define MSDC_PB0_SDIO_DBSSEL     (0x1 << 16)
#define MSDC_PB0_SDIO_INTCSEL    (0x1 << 17)
#define MSDC_PB0_SDIO_BSYDLY     (0xf << 18)
#define MSDC_PB0_SDC_WDOD        (0xf << 22)
#define MSDC_PB0_CMDIDRTSEL      (0x1 << 26)
#define MSDC_PB0_CMDFAILSEL      (0x1 << 27)
#define MSDC_PB0_SDIO_INTDLYSEL  (0x1 << 28)
#define MSDC_PB0_SPCPUSH         (0x1 << 29)
#define MSDC_PB0_DETWR_CRCTMO    (0x1 << 30)
#define MSDC_PB0_EN_DRVRSP       (0x1UL << 31)

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PB1_WRDAT_CRCS_TA_CNTR    (0x7 << 0)
#define MSDC_PB1_CMD_RSP_TA_CNTR      (0x7 << 3)
#define MSDC_PB1_BUSY_MARGIN     (0x1 << 6)
#define MSDC_PB1_CRC_MARGIN      (0x1 << 7)
#define MSDC_PB1_BIAS_TUNE_28NM  (0xf << 8)
#define MSDC_PB1_BIAS_EN18IO_28NM (0x1 << 12)
#define MSDC_PB1_BIAS_EXT_28NM   (0x1 << 13)
#define MSDC_PB1_RESV2           (0x3 << 14)
#define MSDC_PB1_RESV1           (0x3 << 16)
#define MSDC_PB1_DCMSEL2         (0x3 << 18)
#define MSDC_PB1_DCMSEL1         (0x1 << 20)
#define MSDC_PB1_DCMEN           (0x1 << 21)
#define MSDC_PB1_AXIWRAP_CKEN    (0x1 << 22)
#define MSDC_PB1_AHBCKEN         (0x1 << 23)
#define MSDC_PB1_CKSPCEN         (0x1 << 24)
#define MSDC_PB1_CKPSCEN         (0x1 << 25)
#define MSDC_PB1_CKVOLDETEN      (0x1 << 26)
#define MSDC_PB1_CKACMDEN        (0x1 << 27)
#define MSDC_PB1_CKSDEN          (0x1 << 28)
#define MSDC_PB1_CKWCTLEN        (0x1 << 29)
#define MSDC_PB1_CKRCTLEN        (0x1 << 30)
#define MSDC_PB1_CKSHBFFEN       (0x1UL << 31)

/* MSDC_HW_DBG_SEL mask */
#define MSDC_HW_DBG3_SEL         (0xff << 0)
#define MSDC_HW_DBG2_SEL         (0x3f << 8)
#define MSDC_HW_DBG1_SEL         (0x3f << 16)
#define MSDC_HW_DBG_WRAPTYPE_SEL (0x3 << 22)
#define MSDC_HW_DBG0_SEL         (0x3f << 24)
#define MSDC_HW_DBG_WRAP_SEL     (0x1  << 30)

/* EMMC50_PAD_DS_TUNE mask */
#define MSDC_EMMC50_PAD_DS_TUNE_DLYSEL  (0x1 << 0)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL (0x1 << 1)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY1    (0x1f << 2)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY2    (0x1f << 7)
#define MSDC_EMMC50_PAD_DS_TUNE_DLY3    (0x1F << 12)

/* EMMC50_PAD_CMD_TUNE mask */
#define MSDC_EMMC50_PAD_CMD_TUNE_DLY3SEL (0x1 << 0)
#define MSDC_EMMC50_PAD_CMD_TUNE_RXDLY3  (0x1f << 1)
#define MSDC_EMMC50_PAD_CMD_TUNE_TXDLY   (0x1f << 6)

/* EMMC50_PAD_DAT01_TUNE mask */
#define MSDC_EMMC50_PAD_DAT0_RXDLY3SEL   (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT0_RXDLY3      (0x1f << 1)
#define MSDC_EMMC50_PAD_DAT0_TXDLY       (0x1f << 6)
#define MSDC_EMMC50_PAD_DAT1_RXDLY3SEL   (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT1_RXDLY3      (0x1f << 17)
#define MSDC_EMMC50_PAD_DAT1_TXDLY       (0x1f << 22)

/* EMMC50_PAD_DAT23_TUNE mask */
#define MSDC_EMMC50_PAD_DAT2_RXDLY3SEL   (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT2_RXDLY3      (0x1f << 1)
#define MSDC_EMMC50_PAD_DAT2_TXDLY       (0x1f << 6)
#define MSDC_EMMC50_PAD_DAT3_RXDLY3SEL   (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT3_RXDLY3      (0x1f << 17)
#define MSDC_EMMC50_PAD_DAT3_TXDLY       (0x1f << 22)

/* EMMC50_PAD_DAT45_TUNE mask */
#define MSDC_EMMC50_PAD_DAT4_RXDLY3SEL   (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT4_RXDLY3      (0x1f << 1)
#define MSDC_EMMC50_PAD_DAT4_TXDLY       (0x1f << 6)
#define MSDC_EMMC50_PAD_DAT5_RXDLY3SEL   (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT5_RXDLY3      (0x1f << 17)
#define MSDC_EMMC50_PAD_DAT5_TXDLY       (0x1f << 22)

/* EMMC50_PAD_DAT67_TUNE mask */
#define MSDC_EMMC50_PAD_DAT6_RXDLY3SEL   (0x1 << 0)
#define MSDC_EMMC50_PAD_DAT6_RXDLY3      (0x1f << 1)
#define MSDC_EMMC50_PAD_DAT6_TXDLY       (0x1f << 6)
#define MSDC_EMMC50_PAD_DAT7_RXDLY3SEL   (0x1 << 16)
#define MSDC_EMMC50_PAD_DAT7_RXDLY3      (0x1f << 17)
#define MSDC_EMMC50_PAD_DAT7_TXDLY       (0x1f << 22)

/* EMMC50_CFG0 mask */
#define MSDC_EMMC50_CFG_PADCMD_LATCHCK   (0x1 << 0)
#define MSDC_EMMC50_CFG_CRCSTS_EDGE      (0x1 << 3)
#define MSDC_EMMC50_CFG_CFCSTS_SEL       (0x1 << 4)
#define MSDC_EMMC50_CFG_ENDBIT_CHK_BIT   (0xf << 5)
#define MSDC_EMMC50_CFG_CMDRSP_SEL       (0x1 << 9)
#define MSDC_EMMC50_CFG_CMDEDGE_SEL      (0x1 << 10)
#define MSDC_EMMC50_CFG_ENDBIT_CNT       (0x3ff<<11)
#define MSDC_EMMC50_CFG_RDAT_CNT         (0x7 << 21)
#define MSDC_EMMC50_CFG_SPARE_BAK0       (0x7f<<24)
#define MSDC_EMMC50_CFG_GDMA_RESET       (0x1UL << 31)

/* EMMC50_CFG1 mask */
#define MSDC_EMMC50_CFG1_WRPTR_MARGIN    (0xff << 0)
#define MSDC_EMMC50_CFG1_CKSWITCH_CNT    (0x7 << 8)
#define MSDC_EMMC50_CFG1_RDDAT_STOP      (0x1 << 11)
#define MSDC_EMMC50_CFG1_WAITCLK_CNT     (0xf << 12)
#define MSDC_EMMC50_CFG1_DBG_SEL         (0xff << 16)
#define MSDC_EMMC50_CFG1_SPARE1          (0xffUL << 24)

/* EMMC50_CFG2_mask */
#define MSDC_EMMC50_CFG2_AXI_GPD_UP            (0x1 << 0)
#define MSDC_EMMC50_CFG2_AXI_IOMMU_WR_EMI      (0x1 << 1)
#define MSDC_EMMC50_CFG2_AXI_SHARE_EN_WR_EMI   (0x1 << 2)
#define MSDC_EMMC50_CFG2_AXI_IOMMU_RD_EMI      (0x1 << 7)
#define MSDC_EMMC50_CFG2_AXI_SHARE_EN_RD_EMI   (0x1 << 8)
#define MSDC_EMMC50_CFG2_AXI_BOUND_128B        (0x1 << 13)
#define MSDC_EMMC50_CFG2_AXI_BOUND_256B        (0x1 << 14)
#define MSDC_EMMC50_CFG2_AXI_BOUND_512B        (0x1 << 15)
#define MSDC_EMMC50_CFG2_AXI_BOUND_1K          (0x1 << 16)
#define MSDC_EMMC50_CFG2_AXI_BOUND_2K          (0x1 << 17)
#define MSDC_EMMC50_CFG2_AXI_BOUND_4K          (0x1 << 18)
#define MSDC_EMMC50_CFG2_AXI_RD_OUTS_NUM       (0x1f << 19)
#define MSDC_EMMC50_CFG2_AXI_BURSTSZ           (0xf << 24)


/* EMMC50_CFG3_mask */
#define MSDC_EMMC50_CFG3_OUTS_WR               (0x1f << 0)
#define MSDC_EMMC50_CFG3_ULTRA_SET_WR          (0x3f << 5)
#define MSDC_EMMC50_CFG3_PREULTRA_SET_WR       (0x3f << 11)
#define MSDC_EMMC50_CFG3_ULTRA_SET_RD          (0x3f << 17)
#define MSDC_EMMC50_CFG3_PREULTRA_SET_RD       (0x3f << 23)

/* EMMC50_CFG4_mask */
#define MSDC_EMMC50_CFG4_IMPR_ULTRA_SET_WR     (0xff << 0)
#define MSDC_EMMC50_CFG4_IMPR_ULTRA_SET_RD     (0xff << 8)
#define MSDC_EMMC50_CFG4_ULTRA_EN              (0x3 << 16)

/* SDIO_TUNE_WIND mask*/
#define MSDC_SDIO_TUNE_WIND      (0x1f << 0)

/* MSDC_CFG[START_BIT] value */
#define START_AT_RISING    (0x0)
#define START_AT_FALLING   (0x1)
#define START_AT_RISING_AND_FALLING (0x2)
#define START_AT_RISING_OR_FALLING  (0x3)

#define MSDC_SMPL_RISING        (0)
#define MSDC_SMPL_FALLING       (1)
#define MSDC_SMPL_SEPERATE      (2)


#define TYPE_CMD_RESP_EDGE   (0)
#define TYPE_WRITE_CRC_EDGE  (1)
#define TYPE_READ_DATA_EDGE  (2)
#define TYPE_WRITE_DATA_EDGE (3)

#define CARD_READY_FOR_DATA             (1<<8)
#define CARD_CURRENT_STATE(x)           ((x&0x00001E00)>>9)

/*
 * MSDC pad control at top layer
 */
extern void __iomem *msdc_gpio_base;

#define MSDC_GPIO_ADDR(x)	((volatile u16 *)((ulong)msdc_gpio_base+(x)))

/*offset addr for gpio*/
#define MSDC0_GPIO_CLK_OFFSET                   (0xC00)
#define MSDC0_GPIO_CMD_OFFSET                   (0xC10)
#define MSDC0_GPIO_DAT_OFFSET                   (0xC20)
#define MSDC0_GPIO_PAD_OFFSET                   (0xC30)
#define MSDC0_GPIO_RST_OFFSET                   (0xD00)
#define MSDC0_GPIO_DS_OFFSET                    (0xD10)
#define MSDC0_GPIO_MODE0_OFFSET                 (0x7E0)
#define MSDC0_GPIO_MODE1_OFFSET                 (0x7F0)
#define MSDC0_GPIO_MODE2_OFFSET                 (0x800)
#define MSDC0_GPIO_MODE3_OFFSET                 (0x810)

#define MSDC1_GPIO_CLK_OFFSET                   (0xC40)
#define MSDC1_GPIO_CMD_OFFSET                   (0xC50)
#define MSDC1_GPIO_DAT_OFFSET                   (0xC60)
#define MSDC1_GPIO_PAD_OFFSET                   (0xC70)
#define MSDC1_GPIO_DAT1_OFFSET                  (0xD20)
#define MSDC1_GPIO_MODE0_OFFSET                 (0x820)
#define MSDC1_GPIO_MODE1_OFFSET                 (0x830)

#define MSDC2_GPIO_CLK_OFFSET                   (0xC80)
#define MSDC2_GPIO_CMD_OFFSET                   (0xC90)
#define MSDC2_GPIO_DAT_OFFSET                   (0xCA0)
#define MSDC2_GPIO_PAD_OFFSET                   (0xCB0)
#define MSDC2_GPIO_DAT1_OFFSET                  (0xD30)
#define MSDC2_GPIO_MODE0_OFFSET                 (0x740)
#define MSDC2_GPIO_MODE1_OFFSET                 (0x750)

#define MSDC3_GPIO_CLK_OFFSET                   (0xCC0)
#define MSDC3_GPIO_CMD_OFFSET                   (0xCD0)
#define MSDC3_GPIO_DAT_OFFSET                   (0xCE0)
#define MSDC3_GPIO_PAD_OFFSET                   (0xCF0)
#define MSDC3_GPIO_DAT1_OFFSET                  (0xD40)
#define MSDC3_GPIO_MODE0_OFFSET                 (0x640)
#define MSDC3_GPIO_MODE1_OFFSET                 (0x650)

/*base addr for gpio*/
#define MSDC0_GPIO_CLK_BASE                   (msdc_gpio_base + 0xC00)
#define MSDC0_GPIO_CMD_BASE                   (msdc_gpio_base + 0xC10)
#define MSDC0_GPIO_DAT_BASE                   (msdc_gpio_base + 0xC20)
#define MSDC0_GPIO_PAD_BASE                   (msdc_gpio_base + 0xC30)
#define MSDC0_GPIO_RST_BASE                   (msdc_gpio_base + 0xD00)
#define MSDC0_GPIO_DS_BASE                    (msdc_gpio_base + 0xD10)
#define MSDC0_GPIO_MODE0_BASE                 (msdc_gpio_base + 0x7E0)
#define MSDC0_GPIO_MODE1_BASE                 (msdc_gpio_base + 0x7F0)
#define MSDC0_GPIO_MODE2_BASE                 (msdc_gpio_base + 0x800)
#define MSDC0_GPIO_MODE3_BASE                 (msdc_gpio_base + 0x810)

#define MSDC1_GPIO_CLK_BASE                   (msdc_gpio_base + 0xC40)
#define MSDC1_GPIO_CMD_BASE                   (msdc_gpio_base + 0xC50)
#define MSDC1_GPIO_DAT_BASE                   (msdc_gpio_base + 0xC60)
#define MSDC1_GPIO_PAD_BASE                   (msdc_gpio_base + 0xC70)
#define MSDC1_GPIO_DAT1_BASE                  (msdc_gpio_base + 0xD20)
#define MSDC1_GPIO_MODE0_BASE                 (msdc_gpio_base + 0x820)
#define MSDC1_GPIO_MODE1_BASE                 (msdc_gpio_base + 0x830)

#define MSDC2_GPIO_CLK_BASE                   (msdc_gpio_base + 0xC80)
#define MSDC2_GPIO_CMD_BASE                   (msdc_gpio_base + 0xC90)
#define MSDC2_GPIO_DAT_BASE                   (msdc_gpio_base + 0xCA0)
#define MSDC2_GPIO_PAD_BASE                   (msdc_gpio_base + 0xCB0)
#define MSDC2_GPIO_DAT1_BASE                  (msdc_gpio_base + 0xD30)
#define MSDC2_GPIO_MODE0_BASE                 (msdc_gpio_base + 0x740)
#define MSDC2_GPIO_MODE1_BASE                 (msdc_gpio_base + 0x750)

#define MSDC3_GPIO_CLK_BASE                   (msdc_gpio_base + 0xCC0)
#define MSDC3_GPIO_CMD_BASE                   (msdc_gpio_base + 0xCD0)
#define MSDC3_GPIO_DAT_BASE                   (msdc_gpio_base + 0xCE0)
#define MSDC3_GPIO_PAD_BASE                   (msdc_gpio_base + 0xCF0)
#define MSDC3_GPIO_DAT1_BASE                  (msdc_gpio_base + 0xD40)
#define MSDC3_GPIO_MODE0_BASE                 (msdc_gpio_base + 0x640)
#define MSDC3_GPIO_MODE1_BASE                 (msdc_gpio_base + 0x650)


#define GPIO_PAD_TDSEL_MASK                   (0xF <<  0)
#define GPIO_PAD_RDSEL_MASK                   (0x3F <<  4)
#define GPIO_PAD_BIAS_MASK                    (0xF << 12)

#define GPIO_MSDC_R1R0_MASK                   (0x3 <<  0)
#define GPIO_MSDC_PUPD_MASK                   (0x1 <<  2)
#define GPIO_MSDC_DRV_MASK                    (0x7 <<  8)
#define GPIO_MSDC_SR_MASK                     (0x1 << 12)
#define GPIO_MSDC_SMT_MASK                    (0x1 << 13)
#define GPIO_MSDC_IES_MASK                    (0x1 << 14)

#define GPIO_MSDC_DAT0_R1R0_MASK              (0x3 <<  0)
#define GPIO_MSDC_DAT0_PUPD_MASK              (0x1 <<  2)
#define GPIO_MSDC_DAT1_R1R0_MASK              (0x3 <<  4)
#define GPIO_MSDC_DAT1_PUPD_MASK              (0x1 <<  6)
#define GPIO_MSDC_DAT2_R1R0_MASK              (0x3 <<  8)
#define GPIO_MSDC_DAT2_PUPD_MASK              (0x1 << 10)
#define GPIO_MSDC_DAT3_R1R0_MASK              (0x3 << 12)
#define GPIO_MSDC_DAT3_PUPD_MASK              (0x1 << 14)

/* add pull down/up mode define */
#define MSDC_GPIO_PULL_UP        (0)
#define MSDC_GPIO_PULL_DOWN      (1)

#define GPIO_CLK_CTRL       (0)
#define GPIO_CMD_CTRL       (1)
#define GPIO_DAT_CTRL       (2)
#define GPIO_RST_CTRL       (3)
#define GPIO_DS_CTRL        (4)
#define GPIO_PAD_CTRL       (5)
#define GPIO_MODE_CTRL      (6)

#define MSDC_PULL_0K        (0)
#define MSDC_PULL_10K       (1)
#define MSDC_PULL_50K       (2)
#define MSDC_PULL_8K        (3)

typedef enum MSDC_POWER {
	MSDC_VIO18_MC1 = 0,
	MSDC_VIO18_MC2,
	MSDC_VIO28_MC1,
	MSDC_VIO28_MC2,
	MSDC_VMC,
	MSDC_VGP6,
} MSDC_POWER_DOMAIN;


/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
typedef struct {
	u32 hwo:1;		/* could be changed by hw */
	u32 bdp:1;
	u32 rsv0:6;
	u32 chksum:8;
	u32 intr:1;
	u32 rsv1:15;
	u32  next;
	u32  ptr;
	u32  buflen:16;
	u32 extlen:8;
	u32 rsv2:8;
	u32 arg;
	u32 blknum;
	u32 cmd;
} gpd_t;

typedef struct {
	u32 eol:1;
	u32 rsv0:7;
	u32 chksum:8;
	u32 rsv1:1;
	u32 blkpad:1;
	u32 dwpad:1;
	u32 rsv2:13;
	u32  next;
	u32  ptr;
	u32 buflen:16;
	u32 rsv3:16;
} bd_t;

/*--------------------------------------------------------------------------*/
/* Register Debugging Structure                                             */
/*--------------------------------------------------------------------------*/

typedef struct {
	u32 msdc:1;
	u32 ckpwn:1;
	u32 rst:1;
	u32 pio:1;
	u32 ckdrven:1;
	u32 start18v:1;
	u32 pass18v:1;
	u32 ckstb:1;
	u32 ckdiv:8;
	u32 ckmod:2;
	u32 pad:14;
} msdc_cfg_reg;
typedef struct {
	u32 sdr104cksel:1;
	u32 rsmpl:1;
	u32 dsmpl:1;
	u32 ddlysel:1;
	u32 ddr50ckd:1;
	u32 dsplsel:1;
	u32 pad1:10;
	u32 d0spl:1;
	u32 d1spl:1;
	u32 d2spl:1;
	u32 d3spl:1;
	u32 d4spl:1;
	u32 d5spl:1;
	u32 d6spl:1;
	u32 d7spl:1;
	u32 riscsz:1;
	u32 pad2:7;
} msdc_iocon_reg;
typedef struct {
	u32 cden:1;
	u32 cdsts:1;
	u32 pad1:10;
	u32 cddebounce:4;
	u32 dat:8;
	u32 cmd:1;
	u32 pad2:6;
	u32 wp:1;
} msdc_ps_reg;
typedef struct {
	u32 mmcirq:1;
	u32 cdsc:1;
	u32 pad1:1;
	u32 atocmdrdy:1;
	u32 atocmdtmo:1;
	u32 atocmdcrc:1;
	u32 dmaqempty:1;
	u32 sdioirq:1;
	u32 cmdrdy:1;
	u32 cmdtmo:1;
	u32 rspcrc:1;
	u32 csta:1;
	u32 xfercomp:1;
	u32 dxferdone:1;
	u32 dattmo:1;
	u32 datcrc:1;
	u32 atocmd19done:1;
	u32 pad2:15;
} msdc_int_reg;
typedef struct {
	u32 mmcirq:1;
	u32 cdsc:1;
	u32 pad1:1;
	u32 atocmdrdy:1;
	u32 atocmdtmo:1;
	u32 atocmdcrc:1;
	u32 dmaqempty:1;
	u32 sdioirq:1;
	u32 cmdrdy:1;
	u32 cmdtmo:1;
	u32 rspcrc:1;
	u32 csta:1;
	u32 xfercomp:1;
	u32 dxferdone:1;
	u32 dattmo:1;
	u32 datcrc:1;
	u32 atocmd19done:1;
	u32 pad2:15;
} msdc_inten_reg;
typedef struct {
	u32 rxcnt:8;
	u32 pad1:8;
	u32 txcnt:8;
	u32 pad2:7;
	u32 clr:1;
} msdc_fifocs_reg;
typedef struct {
	u32 val;
} msdc_txdat_reg;
typedef struct {
	u32 val;
} msdc_rxdat_reg;
typedef struct {
	u32 sdiowkup:1;
	u32 inswkup:1;
	u32 pad1:14;
	u32 buswidth:2;
	u32 pad2:1;
	u32 sdio:1;
	u32 sdioide:1;
	u32 intblkgap:1;
	u32 pad4:2;
	u32 dtoc:8;
} sdc_cfg_reg;
typedef struct {
	u32 cmd:6;
	u32 brk:1;
	u32 rsptyp:3;
	u32 pad1:1;
	u32 dtype:2;
	u32 rw:1;
	u32 stop:1;
	u32 goirq:1;
	u32 blklen:12;
	u32 atocmd:2;
	u32 volswth:1;
	u32 pad2:1;
} sdc_cmd_reg;
typedef struct {
	u32 arg;
} sdc_arg_reg;
typedef struct {
	u32 sdcbusy:1;
	u32 cmdbusy:1;
	u32 pad:29;
	u32 swrcmpl:1;
} sdc_sts_reg;
typedef struct {
	u32 val;
} sdc_resp0_reg;
typedef struct {
	u32 val;
} sdc_resp1_reg;
typedef struct {
	u32 val;
} sdc_resp2_reg;
typedef struct {
	u32 val;
} sdc_resp3_reg;
typedef struct {
	u32 num;
} sdc_blknum_reg;
typedef struct {
	u32 sts;
} sdc_csts_reg;
typedef struct {
	u32 sts;
} sdc_cstsen_reg;
typedef struct {
	u32 datcrcsts:8;
	u32 ddrcrcsts:4;
	u32 pad:20;
} sdc_datcrcsts_reg;
typedef struct {
	u32 bootstart:1;
	u32 bootstop:1;
	u32 bootmode:1;
	u32 pad1:9;
	u32 bootwaidly:3;
	u32 bootsupp:1;
	u32 pad2:16;
} emmc_cfg0_reg;
typedef struct {
	u32 bootcrctmc:16;
	u32 pad:4;
	u32 bootacktmc:12;
} emmc_cfg1_reg;
typedef struct {
	u32 bootcrcerr:1;
	u32 bootackerr:1;
	u32 bootdattmo:1;
	u32 bootacktmo:1;
	u32 bootupstate:1;
	u32 bootackrcv:1;
	u32 bootdatrcv:1;
	u32 pad:25;
} emmc_sts_reg;
typedef struct {
	u32 bootrst:1;
	u32 pad:31;
} emmc_iocon_reg;
typedef struct {
	u32 val;
} msdc_acmd_resp_reg;
typedef struct {
	u32 tunesel:4;
	u32 pad:28;
} msdc_acmd19_trg_reg;
typedef struct {
	u32 val;
} msdc_acmd19_sts_reg;
typedef struct {
	u32 addr;
} msdc_dma_sa_reg;
typedef struct {
	u32 addr;
} msdc_dma_ca_reg;
typedef struct {
	u32 start:1;
	u32 stop:1;
	u32 resume:1;
	u32 pad1:5;
	u32 mode:1;
	u32 pad2:1;
	u32 lastbuf:1;
	u32 pad3:1;
	u32 brustsz:3;
	u32 pad4:1;
	u32 xfersz:16;
} msdc_dma_ctrl_reg;
typedef struct {
	u32 status:1;
	u32 decsen:1;
	u32 pad1:2;
	u32 bdcsen:1;
	u32 gpdcsen:1;
	u32 pad2:26;
} msdc_dma_cfg_reg;
typedef struct {
	u32 sel:16;
	u32 pad2:16;
} msdc_dbg_sel_reg;
typedef struct {
	u32 val;
} msdc_dbg_out_reg;
typedef struct {
	u32 clkdrvn:3;
	u32 rsv0:1;
	u32 clkdrvp:3;
	u32 rsv1:1;
	u32 clksr:1;
	u32 rsv2:7;
	u32 clkpd:1;
	u32 clkpu:1;
	u32 clksmt:1;
	u32 clkies:1;
	u32 clktdsel:4;
	u32 clkrdsel:8;
} msdc_pad_ctl0_reg;
typedef struct {
	u32 cmddrvn:3;
	u32 rsv0:1;
	u32 cmddrvp:3;
	u32 rsv1:1;
	u32 cmdsr:1;
	u32 rsv2:7;
	u32 cmdpd:1;
	u32 cmdpu:1;
	u32 cmdsmt:1;
	u32 cmdies:1;
	u32 cmdtdsel:4;
	u32 cmdrdsel:8;
} msdc_pad_ctl1_reg;
typedef struct {
	u32 datdrvn:3;
	u32 rsv0:1;
	u32 datdrvp:3;
	u32 rsv1:1;
	u32 datsr:1;
	u32 rsv2:7;
	u32 datpd:1;
	u32 datpu:1;
	u32 datsmt:1;
	u32 daties:1;
	u32 dattdsel:4;
	u32 datrdsel:8;
} msdc_pad_ctl2_reg;
typedef struct {
	u32 wrrxdly:3;
	u32 pad1:5;
	u32 rdrxdly:8;
	u32 pad2:16;
} msdc_pad_tune_reg;
typedef struct {
	u32 dat0:5;
	u32 rsv0:3;
	u32 dat1:5;
	u32 rsv1:3;
	u32 dat2:5;
	u32 rsv2:3;
	u32 dat3:5;
	u32 rsv3:3;
} msdc_dat_rddly0;
typedef struct {
	u32 dat4:5;
	u32 rsv4:3;
	u32 dat5:5;
	u32 rsv5:3;
	u32 dat6:5;
	u32 rsv6:3;
	u32 dat7:5;
	u32 rsv7:3;
} msdc_dat_rddly1;
typedef struct {
	u32 dbg0sel:8;
	u32 dbg1sel:6;
	u32 pad1:2;
	u32 dbg2sel:6;
	u32 pad2:2;
	u32 dbg3sel:6;
	u32 pad3:2;
} msdc_hw_dbg_reg;
typedef struct {
	u32 val;
} msdc_version_reg;
typedef struct {
	u32 val;
} msdc_eco_ver_reg;

struct msdc_regs {
	msdc_cfg_reg msdc_cfg;	/* base+0x00h */
	msdc_iocon_reg msdc_iocon;	/* base+0x04h */
	msdc_ps_reg msdc_ps;	/* base+0x08h */
	msdc_int_reg msdc_int;	/* base+0x0ch */
	msdc_inten_reg msdc_inten;	/* base+0x10h */
	msdc_fifocs_reg msdc_fifocs;	/* base+0x14h */
	msdc_txdat_reg msdc_txdat;	/* base+0x18h */
	msdc_rxdat_reg msdc_rxdat;	/* base+0x1ch */
	u32 rsv1[4];
	sdc_cfg_reg sdc_cfg;	/* base+0x30h */
	sdc_cmd_reg sdc_cmd;	/* base+0x34h */
	sdc_arg_reg sdc_arg;	/* base+0x38h */
	sdc_sts_reg sdc_sts;	/* base+0x3ch */
	sdc_resp0_reg sdc_resp0;	/* base+0x40h */
	sdc_resp1_reg sdc_resp1;	/* base+0x44h */
	sdc_resp2_reg sdc_resp2;	/* base+0x48h */
	sdc_resp3_reg sdc_resp3;	/* base+0x4ch */
	sdc_blknum_reg sdc_blknum;	/* base+0x50h */
	u32 rsv2[1];
	sdc_csts_reg sdc_csts;	/* base+0x58h */
	sdc_cstsen_reg sdc_cstsen;	/* base+0x5ch */
	sdc_datcrcsts_reg sdc_dcrcsta;	/* base+0x60h */
	u32 rsv3[3];
	emmc_cfg0_reg emmc_cfg0;	/* base+0x70h */
	emmc_cfg1_reg emmc_cfg1;	/* base+0x74h */
	emmc_sts_reg emmc_sts;	/* base+0x78h */
	emmc_iocon_reg emmc_iocon;	/* base+0x7ch */
	msdc_acmd_resp_reg acmd_resp;	/* base+0x80h */
	msdc_acmd19_trg_reg acmd19_trg;	/* base+0x84h */
	msdc_acmd19_sts_reg acmd19_sts;	/* base+0x88h */
	u32 rsv4[1];
	msdc_dma_sa_reg dma_sa;	/* base+0x90h */
	msdc_dma_ca_reg dma_ca;	/* base+0x94h */
	msdc_dma_ctrl_reg dma_ctrl;	/* base+0x98h */
	msdc_dma_cfg_reg dma_cfg;	/* base+0x9ch */
	msdc_dbg_sel_reg dbg_sel;	/* base+0xa0h */
	msdc_dbg_out_reg dbg_out;	/* base+0xa4h */
	u32 rsv5[2];
	u32 patch0;		/* base+0xb0h */
	u32 patch1;		/* base+0xb4h */
	u32 rsv6[10];
	msdc_pad_ctl0_reg pad_ctl0;	/* base+0xe0h */
	msdc_pad_ctl1_reg pad_ctl1;	/* base+0xe4h */
	msdc_pad_ctl2_reg pad_ctl2;	/* base+0xe8h */
	msdc_pad_tune_reg pad_tune;	/* base+0xech */
	msdc_dat_rddly0 dat_rddly0;	/* base+0xf0h */
	msdc_dat_rddly1 dat_rddly1;	/* base+0xf4h */
	msdc_hw_dbg_reg hw_dbg;	/* base+0xf8h */
	u32 rsv7[1];
	msdc_version_reg version;	/* base+0x100h */
	msdc_eco_ver_reg eco_ver;	/* base+0x104h */
};

struct scatterlist_ex {
	u32 cmd;
	u32 arg;
	u32 sglen;
	struct scatterlist *sg;
};

#define DMA_FLAG_NONE       (0x00000000)
#define DMA_FLAG_EN_CHKSUM  (0x00000001)
#define DMA_FLAG_PAD_BLOCK  (0x00000002)
#define DMA_FLAG_PAD_DWORD  (0x00000004)

struct msdc_dma {
	u32 flags;		/* flags */
	u32 xfersz;		/* xfer size in bytes */
	u32 sglen;		/* size of scatter list */
	u32 blklen;		/* block size */
	struct scatterlist *sg;	/* I/O scatter list */
	struct scatterlist_ex *esg;	/* extended I/O scatter list */
	u8 mode;		/* dma mode        */
	u8 burstsz;		/* burst size      */
	u8 intr;		/* dma done interrupt */
	u8 padding;		/* padding */
	u32 cmd;		/* enhanced mode command */
	u32 arg;		/* enhanced mode arg */
	u32 rsp;		/* enhanced mode command response */
	u32 autorsp;		/* auto command response */

	gpd_t *gpd;		/* pointer to gpd array */
	bd_t *bd;		/* pointer to bd array */
	dma_addr_t gpd_addr;	/* the physical address of gpd array */
	dma_addr_t bd_addr;	/* the physical address of bd array */
	u32 used_gpd;		/* the number of used gpd elements */
	u32 used_bd;		/* the number of used bd elements */
};

struct tune_counter {
	u32 time_cmd;
	u32 time_read;
	u32 time_write;
	u32 time_hs400;
};
struct msdc_saved_para {
	u32 pad_tune;
	u32 ddly0;
	u32 ddly1;
	u8 cmd_resp_ta_cntr;
	u8 wrdat_crc_ta_cntr;
	u8 suspend_flag;
	u32 msdc_cfg;
	u32 mode;
	u32 div;
	u32 sdc_cfg;
	u32 iocon;
	u32 state;
	u32 hz;
	u8 int_dat_latch_ck_sel;
	u8 ckgen_msdc_dly_sel;
	u8 inten_sdio_irq;
	u8     write_busy_margin; /* for write: 3T need wait before host check busy after crc status */
	u8     write_crc_margin; /* for write: host check timeout change to 16T */
#ifdef CONFIG_EMMC_50_FEATURE
	u8 ds_dly1;
	u8 ds_dly3;
	u32 emmc50_pad_cmd_tune;
#endif
	u32 vcore_uv;//ALPS01919187
};

#if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST)
struct ot_data {
	u32 eco_ver;
	u32 orig_blknum;
	u32 orig_patch_bit0;
	u32 orig_iocon;

#define DMA_ON 0
#define DMA_OFF 1
	u32 orig_dma;
	u32 orig_cmdrdly;
	u32 orig_ddlsel;
	u32 orig_paddatrddly;
	u32 orig_paddatwrdly;
	u32 orig_dat0rddly;
	u32 orig_dat1rddly;
	u32 orig_dat2rddly;
	u32 orig_dat3rddly;
	u32 orig_dtoc;

	u32 cmdrdly;
	u32 datrddly;
	u32 dat0rddly;
	u32 dat1rddly;
	u32 dat2rddly;
	u32 dat3rddly;

	u32 cmddlypass;
	u32 datrddlypass;
	u32 dat0rddlypass;
	u32 dat1rddlypass;
	u32 dat2rddlypass;
	u32 dat3rddlypass;

	u32 fCmdTestedGear;
	u32 fDatTestedGear;
	u32 fDat0TestedGear;
	u32 fDat1TestedGear;
	u32 fDat2TestedGear;
	u32 fDat3TestedGear;

	u32 rawcmd;
	u32 rawarg;
	u32 tune_wind_size;
	u32 fn;
	u32 addr;
	u32 retry;
};

struct ot_work_t {
	struct      delayed_work ot_delayed_work;
	struct      msdc_host *host;
	int         chg_volt;
	atomic_t    ot_disable;
	atomic_t	autok_done;
	struct      completion ot_complete;
};
#endif				/* #if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST) */

struct msdc_host {
	struct msdc_hw *hw;

	struct mmc_host *mmc;	/* mmc structure */
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_request *mrq;
	int cmd_rsp;
	int cmd_rsp_done;
	int cmd_r1b_done;

	int error;
	spinlock_t lock;	/* mutex */
	spinlock_t clk_gate_lock;
	spinlock_t remove_bad_card;	/*to solve removing bad card race condition with hot-plug enable */
	int clk_gate_count;
	struct semaphore sem;

	u32 blksz;		/* host block size */
	void __iomem                *base;           /* host base address */    
	int id;			/* host id */
	int pwr_ref;		/* core power reference count */

	u32 xfer_size;		/* total transferred size */

	struct msdc_dma dma;	/* dma channel */
	u32 dma_addr;		/* dma transfer address */
	u32 dma_left_size;	/* dma transfer left size */
	u32 dma_xfer_size;	/* dma transfer size in bytes */
	int dma_xfer;		/* dma transfer mode */

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */

	atomic_t abort;		/* abort transfer */

	int irq;		/* host interrupt */

	struct tasklet_struct card_tasklet;
#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	struct tasklet_struct flush_cache_tasklet
#endif
	    /* struct delayed_work           remove_card; */
#if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST)
#ifdef MTK_SDIO30_DETECT_THERMAL
	int pre_temper;		/* previous set temperature */
	struct timer_list ot_timer;
	bool ot_period_check_start;
#endif				/* MTK_SDIO30_DETECT_THERMAL */
	struct ot_work_t ot_work;
	atomic_t ot_done;
#endif				/* #if defined(MTK_SDIO30_ONLINE_TUNING_SUPPORT) || defined(ONLINE_TUNING_DVTTEST) */
	atomic_t sdio_stopping;

	struct completion cmd_done;
	struct completion xfer_done;
	struct pm_message pm_state;

    u32                         mclk;           /* mmc subsystem clock */
    u32                         hclk;           /* host clock speed */        
    u32                         sclk;           /* SD/MS clock speed */
    u8                          core_clkon;     /* Host core clock on ? */
    u8                          card_clkon;     /* Card clock on ? */
    u8                          core_power;     /* core power */    
    u8                          power_mode;     /* host power mode */
    u8                          card_inserted;  /* card inserted ? */
    u8                          suspend;        /* host suspended ? */    
    u8                          reserved;
    u8                          app_cmd;        /* for app command */     
    u32                         app_cmd_arg;    
    u64                         starttime;
    struct timer_list           timer;     
    struct tune_counter         t_counter;
    u32                         rwcmd_time_tune;
    int                         read_time_tune;
    int                         write_time_tune;
    u32                         write_timeout_uhs104;
    u32                         read_timeout_uhs104;
    u32                         write_timeout_emmc;
    u32                         read_timeout_emmc;
    u8                          autocmd;
    u32                         sw_timeout;
    u32                         power_cycle; /* power cycle done in tuning flow*/
	bool                        power_cycle_enable;/*Enable power cycle*/

	u32							continuous_fail_request_count;

    u32                         sd_30_busy;
    bool                        tune;
    MSDC_POWER_DOMAIN           power_domain;
    u32                         state;
    struct msdc_saved_para      saved_para;    
    int                         sd_cd_polarity;
    int                         sd_cd_insert_work; //to make sure insert mmc_rescan this work in start_host when boot up
                                                   //driver will get a EINT(Level sensitive) when boot up phone with card insert
    struct wake_lock            trans_lock;
    bool                        block_bad_card;                                               
#ifdef SDIO_ERROR_BYPASS      
    int                         sdio_error;     /* sdio error can't recovery */
#endif									   
    void    (*power_control)(struct msdc_host *host,u32 on);
    void    (*power_switch)(struct msdc_host *host,u32 on);
};

typedef enum {
	cmd_counter = 0,
	read_counter,
	write_counter,
	all_counter,
} TUNE_COUNTER;

enum msdc_state {
	MSDC_STATE_DEFAULT = 0,
	MSDC_STATE_DDR,
	MSDC_STATE_HS200,
	MSDC_STATE_HS400
};

typedef enum {
	TRAN_MOD_PIO,
	TRAN_MOD_DMA,
	TRAN_MOD_NUM
} transfer_mode;

typedef enum {
	OPER_TYPE_READ,
	OPER_TYPE_WRITE,
	OPER_TYPE_NUM
} operation_type;

struct dma_addr {
	u32 start_address;
	u32 size;
	u8 end;
	struct dma_addr *next;
};

#define MSDC_TOP_RESET_ERROR_TUNE
#if 1
struct msdc_reg_control {
        ulong addr;
	u32 mask;
	u32 value;
	u32 default_value;
	int (*restore_func) (int restore);
};
#endif
static inline unsigned int uffs(unsigned int x)
{
	unsigned int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#define sdr_read8(reg)           __raw_readb((const volatile void *)reg)
#define sdr_read16(reg)          __raw_readw((const volatile void *)reg)
#define sdr_read32(reg)          __raw_readl((const volatile void *)reg)
#define sdr_write8(reg, val)      mt_reg_sync_writeb(val, reg)
#define sdr_write16(reg, val)     mt_reg_sync_writew(val, reg)
#define sdr_write32(reg, val)     mt_reg_sync_writel(val, reg)
#define sdr_set_bits(reg, bs) \
    do {\
	volatile unsigned int tv = sdr_read32(reg);\
	tv |= (u32)(bs); \
        sdr_write32(reg, tv); \
    } while (0)
#define sdr_clr_bits(reg, bs) \
    do {\
	volatile unsigned int tv = sdr_read32(reg);\
	tv &= ~((u32)(bs)); \
        sdr_write32(reg, tv); \
    } while (0)


#define sdr_set_field(reg, field, val) \
    do {    \
	volatile unsigned int tv = sdr_read32(reg);    \
	tv &= ~(field); \
	tv |= ((val) << (uffs((unsigned int)field) - 1)); \
        sdr_write32(reg, tv); \
    } while (0)
#define sdr_get_field(reg, field, val) \
    do {    \
	volatile unsigned int tv = sdr_read32(reg);    \
	val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
    } while (0)
#define sdr_set_field_discrete(reg, field, val) \
    do {    \
	volatile unsigned int tv = sdr_read32(reg); \
	tv = (val == 1) ? (tv|(field)):(tv & ~(field));\
        sdr_write32(reg, tv); \
    } while (0)
#define sdr_get_field_discrete(reg, field, val) \
    do {    \
	volatile unsigned int tv = sdr_read32(reg); \
	val = tv & (field); \
        val = (val == field) ? 1 : 0;\
    } while (0)

extern void mmc_remove_card(struct mmc_card *card);
extern void mmc_detach_bus(struct mmc_host *host);
extern void mmc_power_off(struct mmc_host *host);
#endif /* end of  MT_SD_H */

