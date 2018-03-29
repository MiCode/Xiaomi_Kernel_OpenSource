/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>

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
#define MSDC_DBG_OUT     0xa4
#define MSDC_PATCH_BIT   0xb0
#define MSDC_PATCH_BIT1  0xb4
#define MSDC_PAD_TUNE    0xec
#define PAD_DS_TUNE      0x188
#define EMMC50_CFG0      0x208

#define MSDC_TOP_DAT_TUNE_CTRL0   0
#define MSDC_TOP_DAT_TUNE_CTRL1   0x4
#define MSDC_TOP_DAT_TUNE_CTRL2   0x8
#define MSDC_TOP_DAT_TUNE_CTRL3   0xc
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

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1 << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1 << 1)	/* RW */
#define MSDC_IOCON_DSPL         (0x1 << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (0x1 << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (0x1 << 4)	/* RW */
#define MSDC_IOCON_DSPLSEL      (0x1 << 5)	/* RW */
#define MSDC_IOCON_W_DSPL       (0x1 << 8)	/* RW */
#define MSDC_IOCON_W_DSPLSEL    (0x1 << 9)	/* RW */
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

#define EMMC50_CFG_PADCMD_LATCHCK (0x1 << 0)   /* RW */
#define EMMC50_CFG_CRCSTS_EDGE    (0x1 << 3)   /* RW */
#define EMMC50_CFG_CFCSTS_SEL     (0x1 << 4)   /* RW */

/*sdio use port 3*/
#define MSDC_TOP_SDIO_DATA_TUNE_D0      (0X1f << 0)
#define MSDC_TOP_SDIO_DATA_TUNE_D1      (0X1f << 5)
#define MSDC_TOP_SDIO_DATA_TUNE_D2      (0X1f << 10)
#define MSDC_TOP_SDIO_DATA_TUNE_D3      (0X1f << 15)
#define MSDC_TOP_SDIO_DATA_TUNE_CMD     (0X1f << 0)
#define MSDC_TOP_SDIO_DATA_TUNE_SEL     (0X1 << 30)

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
	u32 pad_tune;
	u32 patch_bit0;
	u32 patch_bit1;
	u32 pad_ds_tune;
	u32 emmc50_cfg0;
	u32 top_tune_ctrl1;
	u32 top_tune_ctrl2;
	u32 top_tune_ctrl3;
};

struct msdc_tune_para {
	u32 iocon;
	u32 pad_tune;
};

struct msdc_delay_phase {
	u8 maxlen;
	u8 start;
	u8 final_phase;
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
	void __iomem *top;	/* host toplvl address */

	struct msdc_dma dma;	/* dma channel */
	u64 dma_mask;

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */
	u32 tune_latch_ck_cnt;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_uhs;
	struct delayed_work req_timeout;
	int irq;		/* host interrupt */
	bool irq_thread_alive;

	struct clk *src_clk;	/* msdc source clock */
	struct clk *h_clk;      /* msdc h_clk */
	u32 mclk;		/* mmc subsystem clock frequency */
	u32 src_clk_freq;	/* source clock frequency */
	u32 sclk;		/* SD/MS bus clock frequency */
	bool clock_on;
	unsigned char timing;
	bool vqmmc_enabled;
	u32 hs400_ds_delay;
	bool hs400_mode;	/* current eMMC will run at hs400 mode */
	struct msdc_save_para save_para; /* used when gate HCLK */
	struct msdc_tune_para def_tune_para; /* default tune setting */
	struct msdc_tune_para saved_tune_para; /* tune result of CMD21/CMD19 */
};

static bool sdio_online_tune_fail;

void apply_sdio_setting(struct msdc_host *host, u32 hz);
extern void mmc_set_clock(struct mmc_host *host, unsigned int hz);
extern u32 vcorefs_sdio_get_vcore_nml(void);
extern int vcorefs_sdio_set_vcore_nml(u32 vcore_uv);
extern int vcorefs_sdio_lock_dvfs(bool in_ot);
extern int vcorefs_sdio_unlock_dvfs(bool in_ot);

static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val |= bs;
	writel(val, reg);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val &= ~bs;
	writel(val, reg);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv = readl(reg);

	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;

	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_RST);
	while (readl(host->base + MSDC_CFG) & MSDC_CFG_RST)
		cpu_relax();

	sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	while (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR)
		cpu_relax();

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);
static void msdc_recheck_sdio_irq(struct msdc_host *host);

static const u32 cmd_ints_mask = MSDC_INTEN_CMDRDY | MSDC_INTEN_RSPCRCERR |
			MSDC_INTEN_CMDTMO | MSDC_INTEN_ACMDRDY |
			MSDC_INTEN_ACMDCRCERR | MSDC_INTEN_ACMDTMO;
static const u32 data_ints_mask = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO |
			MSDC_INTEN_DATCRCERR | MSDC_INTEN_DMA_BDCSERR |
			MSDC_INTEN_DMA_GPDCSERR | MSDC_INTEN_DMA_PROTECT;

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xff - (u8) sum;
}

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	unsigned int j, dma_len;
	dma_addr_t dma_address;
	u32 dma_ctrl;
	struct scatterlist *sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	sg = data->sg;

	gpd = dma->gpd;
	bd = dma->bd;

	/* modify gpd */
	gpd->gpd_info |= GPDMA_DESC_HWO;
	gpd->gpd_info |= GPDMA_DESC_BDP;
	/* need to clear first. use these bits to calc checksum */
	gpd->gpd_info &= ~GPDMA_DESC_CHECKSUM;
	gpd->gpd_info |= msdc_dma_calcs((u8 *) gpd, 16) << 8;

	/* modify bd */
	for_each_sg(data->sg, sg, data->sg_count, j) {
		dma_address = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);

		/* init bd */
		bd[j].bd_info &= ~BDMA_DESC_BLKPAD;
		bd[j].bd_info &= ~BDMA_DESC_DWPAD;
		bd[j].ptr = (u32)dma_address;
		bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN;
		bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN);

		if (j == data->sg_count - 1) /* the last bd */
			bd[j].bd_info |= BDMA_DESC_EOL;
		else
			bd[j].bd_info &= ~BDMA_DESC_EOL;

		/* checksume need to clear first */
		bd[j].bd_info &= ~BDMA_DESC_CHECKSUM;
		bd[j].bd_info |= msdc_dma_calcs((u8 *)(&bd[j]), 16) << 8;
	}

	sdr_set_field(host->base + MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, 1);
	dma_ctrl = readl_relaxed(host->base + MSDC_DMA_CTRL);
	dma_ctrl &= ~(MSDC_DMA_CTRL_BRUSTSZ | MSDC_DMA_CTRL_MODE);
	dma_ctrl |= (MSDC_BURST_64B << 12 | 1 << 8);
	writel_relaxed(dma_ctrl, host->base + MSDC_DMA_CTRL);
	writel((u32)dma->gpd_addr, host->base + MSDC_DMA_SA);
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!(data->host_cookie & MSDC_PREPARE_FLAG)) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		data->host_cookie |= MSDC_PREPARE_FLAG;
		data->sg_count = dma_map_sg(host->dev, data->sg, data->sg_len,
					   read ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}
}

static void msdc_unprepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (data->host_cookie & MSDC_ASYNC_FLAG)
		return;

	if (data->host_cookie & MSDC_PREPARE_FLAG) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		dma_unmap_sg(host->dev, data->sg, data->sg_len,
			     read ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		data->host_cookie &= ~MSDC_PREPARE_FLAG;
	}
}

/* clock control primitives */
static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (0x1 << 20) - 1) >> 20;
		sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD, &mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, timeout);
}

static void msdc_gate_clock(struct msdc_host *host)
{
	clk_disable_unprepare(host->src_clk);
	clk_disable_unprepare(host->h_clk);
	host->clock_on = false;
}

static void msdc_ungate_clock(struct msdc_host *host)
{
	clk_prepare_enable(host->h_clk);
	clk_prepare_enable(host->src_clk);
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	host->clock_on = true;
}

static void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	unsigned long irq_flags;

	if (!hz) {
		dev_dbg(host->dev, "set mclk to 0\n");
		host->mclk = 0;
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
		return;
	}

	if (hz >= 100 * 1000 * 1000 && sdio_online_tune_fail)
		hz = 50 * 1000 * 1000;
	apply_sdio_setting(host, hz);

	spin_lock_irqsave(&host->irqlock, irq_flags);
	flags = readl(host->base + MSDC_INTEN);
	sdr_clr_bits(host->base + MSDC_INTEN, flags);
	spin_unlock_irqrestore(&host->irqlock, irq_flags);

	sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_HS400_CK_MODE);
	if (timing == MMC_TIMING_UHS_DDR50 ||
	    timing == MMC_TIMING_MMC_DDR52 ||
	    timing == MMC_TIMING_MMC_HS400) {
		if (timing == MMC_TIMING_MMC_HS400)
			mode = 0x3;
		else
			mode = 0x2; /* ddr mode and use divisor */

		if (hz >= (host->src_clk_freq >> 2)) {
			div = 0; /* mean div = 1/4 */
			sclk = host->src_clk_freq >> 2; /* sclk = clk / 4 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
			div = (div >> 1);
		}

		if (timing == MMC_TIMING_MMC_HS400 &&
		    hz >= (host->src_clk_freq >> 1)) {
			sdr_set_bits(host->base + MSDC_CFG,
				     MSDC_CFG_HS400_CK_MODE);
			sclk = host->src_clk_freq >> 1;
			div = 0; /* div is ignore when bit18 is set */
		}
	} else if (hz >= host->src_clk_freq) {
		mode = 0x1; /* no divisor */
		div = 0;
		sclk = host->src_clk_freq;
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (host->src_clk_freq >> 1)) {
			div = 0; /* mean div = 1/2 */
			sclk = host->src_clk_freq >> 1; /* sclk = clk / 2 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
		}
	}
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 8) | (div % 0xff));
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	host->sclk = sclk;
	host->mclk = hz;
	host->timing = timing;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);

	spin_lock_irqsave(&host->irqlock, irq_flags);
	sdr_set_bits(host->base + MSDC_INTEN, flags);
	spin_unlock_irqrestore(&host->irqlock, irq_flags);

	if (host->sclk <= 52000000) {
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS, 0x1);
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP, 0x1);
	} else {
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS, 0x2);
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP, 0x4);
	}

	dev_err(host->dev, "sclk: %d, timing: %d hz:%d\n", host->sclk, timing, hz);
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = 0x1;
		break;
	case MMC_RSP_R1B:
		resp = 0x7;
		break;
	case MMC_RSP_R2:
		resp = 0x2;
		break;
	case MMC_RSP_R3:
		resp = 0x3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = 0x0;
		break;
	}

	return resp;
}

static inline u32 msdc_cmd_prepare_raw_cmd(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	u32 opcode = cmd->opcode;
	u32 resp = msdc_cmd_find_resp(host, mrq, cmd);
	u32 rawcmd = (opcode & 0x3f) | ((resp & 0x7) << 7);

	host->cmd_rsp = resp;

	if ((opcode == SD_IO_RW_DIRECT &&
	    ((cmd->arg >> 9) & 0x1ffff) == SDIO_CCCR_ABORT) ||
	    opcode == MMC_STOP_TRANSMISSION)
		rawcmd |= (0x1 << 14);
	else if (opcode == SD_SWITCH_VOLTAGE)
		rawcmd |= (0x1 << 30);
	else if (opcode == SD_APP_SEND_SCR ||
		 opcode == SD_APP_SEND_NUM_WR_BLKS ||
		 (opcode == SD_SWITCH && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == SD_APP_SD_STATUS && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == MMC_SEND_EXT_CSD && mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		rawcmd |= (0x1 << 11);

	if (cmd->data) {
		struct mmc_data *data = cmd->data;

		if (mmc_op_multi(opcode)) {
			if (mmc_card_mmc(host->mmc->card) && mrq->sbc &&
			    !(mrq->sbc->arg & 0xFFFF0000))
				rawcmd |= 0x2 << 28; /* AutoCMD23 */
		}

		rawcmd |= ((data->blksz & 0xFFF) << 16);
		if (data->flags & MMC_DATA_WRITE)
			rawcmd |= (0x1 << 13);
		if (data->blocks > 1)
			rawcmd |= (0x2 << 11);
		else
			rawcmd |= (0x1 << 11);
		/* Always use dma mode */
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_PIO);

		if (host->timeout_ns != data->timeout_ns ||
		    host->timeout_clks != data->timeout_clks)
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);

		writel(data->blocks, host->base + SDC_BLK_NUM);
	}
	return rawcmd;
}

static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	unsigned long flags;
	bool read;

	WARN_ON(host->data);
	host->data = data;
	read = data->flags & MMC_DATA_READ;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	msdc_dma_setup(host, &host->dma, data);

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_set_bits(host->base + MSDC_INTEN, data_ints_mask);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);
	spin_unlock_irqrestore(&host->irqlock, flags);

	dev_dbg(host->dev, "DMA start\n");
	dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = cmd->resp;

	rsp[0] = readl(host->base + SDC_ACMD_RESP);

	if (events & MSDC_INT_ACMDRDY) {
		cmd->error = 0;
	} else {
		msdc_reset_hw(host);
		if (events & MSDC_INT_ACMDCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_STOP_EIO;
		} else if (events & MSDC_INT_ACMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
		}
		dev_err(host->dev,
			"%s: AUTO_CMD%d arg=%08X; rsp %08X; cmd_error=%d\n",
			__func__, cmd->opcode, cmd->arg, rsp[0], cmd->error);
	}
	return cmd->error;
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	if (host->error)
		dev_dbg(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
			__func__, cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;
	bool ret;

	ret = cancel_delayed_work(&host->req_timeout);
	if (!ret && in_interrupt()) {
		/* delay work already running */
		return;
	}

	spin_lock_irqsave(&host->lock, flags);
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_track_cmd_data(host, mrq->cmd, mrq->data);
	if (mrq->data)
		msdc_unprepare_data(host, mrq);
	mmc_request_done(host->mmc, mrq);
	msdc_recheck_sdio_irq(host);

	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	unsigned long flags;
	u32 *rsp = cmd->resp;

	if (mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
				   | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (!sbc_error && !(events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO)))
		return done;

	done = !host->cmd;
	spin_lock_irqsave(&host->lock, flags);
	host->cmd = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->irqlock, flags);

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			rsp[0] = readl(host->base + SDC_RESP3);
			rsp[1] = readl(host->base + SDC_RESP2);
			rsp[2] = readl(host->base + SDC_RESP1);
			rsp[3] = readl(host->base + SDC_RESP0);
		} else {
			rsp[0] = readl(host->base + SDC_RESP0);
		}
	}

	if (!sbc_error && !(events & MSDC_INT_CMDRDY)) {
		if (cmd->opcode != MMC_SEND_TUNING_BLOCK &&
		    cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200)
			/*
			 * should not clear fifo/interrupt as the tune data
			 * may have alreay come.
			 */
			msdc_reset_hw(host);
		if (events & MSDC_INT_RSPCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_CMD_EIO;
		} else if (events & MSDC_INT_CMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_CMD_TMO;
		}
	}
	if (cmd->error)
		dev_dbg(host->dev,
				"%s: cmd=%d arg=%08X; rsp %08X; cmd_error=%d\n",
				__func__, cmd->opcode, cmd->arg, rsp[0],
				cmd->error);

	msdc_cmd_next(host, mrq, cmd);
	return true;
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 status = readl(host->base + MSDC_PS);

	/* check if data0 is low */
	return !(status & BIT(16));
}

/* It is the core layer's responsibility to ensure card status
 * is correct before issue a request. but host design do below
 * checks recommended.
 */
static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	/* The max busy time we can endure is 20ms */
	unsigned long tmo = jiffies + msecs_to_jiffies(20);
	u32 count = 0;

	if (in_interrupt()) {
		while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) &&
		       (count < 1000)) {
			udelay(1);
			count++;
		}
	} else {
		while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) &&
		       time_before(jiffies, tmo))
			cpu_relax();
	}

	if (readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) {
		dev_err(host->dev, "CMD bus busy detected\n");
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (cmd->opcode != MMC_SEND_STATUS) {
		count = 0;
		/* Consider that CMD6 crc error before card was init done,
		 * mmc_retune() will return directly as host->card is null.
		 * and CMD6 will retry 3 times, must ensure card is in transfer
		 * state when retry.
		 */
		tmo = jiffies + msecs_to_jiffies(60 * 1000);
		while (1) {
			if (msdc_card_busy(host->mmc)) {
				if (in_interrupt()) {
					udelay(1);
					count++;
				} else {
					msleep_interruptible(10);
				}
			} else {
				break;
			}
			/* Timeout if the device never
			 * leaves the program state.
			 */
			if (count > 1000 || time_after(jiffies, tmo)) {
				pr_err("%s: Card stuck in programming state! %s\n",
				       mmc_hostname(host->mmc), __func__);
				host->error |= REQ_CMD_BUSY;
				msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
				return false;
			}
		}
	}
	return true;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long flags;
	u32 rawcmd;

	WARN_ON(host->cmd);
	host->cmd = cmd;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if ((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16 ||
	    readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_set_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->irqlock, flags);

	writel(cmd->arg, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);
}

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	if ((cmd->error &&
	    !(cmd->error == -EILSEQ &&
	      (cmd->opcode == MMC_SEND_TUNING_BLOCK ||
	       cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200))) ||
	    (mrq->sbc && mrq->sbc->error))
		msdc_request_done(host, mrq);
	else if (cmd == mrq->sbc)
		msdc_start_command(host, mrq, mrq->cmd);
	else if (!cmd->data)
		msdc_request_done(host, mrq);
	else
		msdc_start_data(host, mrq, cmd, cmd->data);
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->error = 0;
	WARN_ON(host->mrq);
	host->mrq = mrq;

	pm_runtime_get_sync(host->dev);

	if (mrq->data)
		msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & 0xFFFF0000)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
		bool is_first_req)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	msdc_prepare_data(host, mrq);
	data->host_cookie |= MSDC_ASYNC_FLAG;
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
		int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;

	data = mrq->data;
	if (!data)
		return;
	if (data->host_cookie) {
		data->host_cookie &= ~MSDC_ASYNC_FLAG;
		msdc_unprepare_data(host, mrq);
	}
}

static void msdc_data_xfer_next(struct msdc_host *host,
				struct mmc_request *mrq, struct mmc_data *data)
{
	if (mmc_op_multi(mrq->cmd->opcode) && mrq->stop && !mrq->stop->error &&
	    !mrq->sbc)
		msdc_start_command(host, mrq, mrq->stop);
	else
		msdc_request_done(host, mrq);
}

static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data)
{
	struct mmc_command *stop = data->stop;
	unsigned long flags;
	bool done;
	unsigned int check_data = events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT);

	done = !host->data;
	spin_lock_irqsave(&host->lock, flags);
	if (check_data)
		host->data = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	if (check_data || (stop && stop->error)) {
		dev_dbg(host->dev, "DMA status: 0x%8X\n",
				readl(host->base + MSDC_DMA_CFG));
		sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
		while (readl(host->base + MSDC_DMA_CFG) & MSDC_DMA_CFG_STS)
			cpu_relax();

		spin_lock_irqsave(&host->irqlock, flags);
		sdr_clr_bits(host->base + MSDC_INTEN, data_ints_mask);
		spin_unlock_irqrestore(&host->irqlock, flags);

		dev_dbg(host->dev, "DMA stop\n");

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error)) {
			data->bytes_xfered = data->blocks * data->blksz;
		} else {
			dev_dbg(host->dev, "interrupt events: %x\n", events);
			msdc_reset_hw(host);
			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO)
				data->error = -ETIMEDOUT;
			else if (events & MSDC_INT_DATCRCERR)
				data->error = -EILSEQ;

			dev_dbg(host->dev, "%s: cmd=%d; blocks=%d",
				__func__, mrq->cmd->opcode, data->blocks);
			dev_dbg(host->dev, "data_error=%d xfer_size=%d\n",
				(int)data->error, data->bytes_xfered);
		}

		msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = readl(host->base + SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	writel(val, host->base + SDC_CFG);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{

#if 0
	struct msdc_host *host = mmc_priv(mmc);
	int min_uv, max_uv;
	int ret = 0;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
			min_uv = 3300000;
			max_uv = 3300000;
		} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			min_uv = 1800000;
			max_uv = 1800000;
		} else {
			dev_err(host->dev, "Unsupported signal voltage!\n");
			return -EINVAL;
		}

		ret = regulator_set_voltage(mmc->supply.vqmmc, min_uv, max_uv);
		if (ret) {
			dev_dbg(host->dev, "Regulator set error %d (%d)\n",
				ret, ios->signal_voltage);
		} else {
			/* Apply different pinctrl settings for different signal voltage */
			if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
				pinctrl_select_state(host->pinctrl, host->pins_uhs);
			else
				pinctrl_select_state(host->pinctrl, host->pins_default);
		}
	}
	return ret;
#endif
	return 0;
}

static void msdc_request_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
			req_timeout.work);

	/* simulate HW timeout status */
	dev_err(host->dev, "%s: aborting cmd/data/mrq\n", __func__);
	if (host->mrq) {
		dev_err(host->dev, "%s: aborting mrq=%p cmd=%d\n", __func__,
				host->mrq, host->mrq->cmd->opcode);
		if (host->cmd) {
			dev_err(host->dev,
				"%s: aborting cmd=%d, arg=0x%x\n", __func__,
				host->cmd->opcode, host->cmd->arg);
			msdc_cmd_done(host, MSDC_INT_CMDTMO, host->mrq,
					host->cmd);
		} else if (host->data) {
			dev_err(host->dev,
				"%s: aborting data: cmd%d; %d blocks\n",
				    __func__, host->mrq->cmd->opcode,
				    host->data->blocks);
			msdc_data_xfer_done(host, MSDC_INT_DATTMO, host->mrq,
					host->data);
		}
	}
}

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	unsigned long flags;
	struct msdc_host *host = (struct msdc_host *) dev_id;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	u32 events, event_mask;

	spin_lock_irqsave(&host->irqlock, flags);
	events = readl(host->base + MSDC_INT);
	event_mask = readl(host->base + MSDC_INTEN);
	/* clear interrupts */
	writel(events & event_mask, host->base + MSDC_INT);

	mrq = host->mrq;
	cmd = host->cmd;
	data = host->data;
	spin_unlock_irqrestore(&host->irqlock, flags);

	if ((events & event_mask) & MSDC_INT_SDIOIRQ) {
		mmc_signal_sdio_irq(host->mmc);
		if (!mrq)
			return IRQ_HANDLED;
	}

	if (!(events & (event_mask & ~MSDC_INT_SDIOIRQ)))
		return IRQ_HANDLED;

	if (!mrq) {
		dev_err(host->dev,
				"%s: MRQ=NULL; events=%08X; event_mask=%08X\n",
				__func__, events, event_mask);
		WARN_ON(1);
		return IRQ_HANDLED;
	}

	if (cmd)
		msdc_cmd_done(host, events, mrq, cmd);
	else if (data)
		msdc_data_xfer_done(host, events, mrq, data);

	return IRQ_HANDLED;
}


static struct msdc_host *wifi_sdhci_host;

static void sdhci_status_notify_cb(int card_present, void *dev_id)
{
	struct msdc_host *sdhci = (struct msdc_host *)dev_id;
	printk("%s: card_present %d\n", mmc_hostname(sdhci->mmc),
		card_present);

	if (card_present == 1) {
		sdhci->mmc->rescan_disable = 0;
		mmc_detect_change(sdhci->mmc, 0);
	} else if (card_present == 0) {
		sdhci->mmc->detect_change = 0;
		sdhci->mmc->rescan_disable = 1;
	}
	return;
}

void smdc_mmc_card_detect(int card_present)
{
	printk("%s: enter \n", __func__);
	if (wifi_sdhci_host)
		sdhci_status_notify_cb(card_present, wifi_sdhci_host);
}

EXPORT_SYMBOL(smdc_mmc_card_detect);

static void msdc_init_hw(struct msdc_host *host)
{
	u32 val;
	unsigned long flags;

	/* Configure to MMC/SD mode, clock free running */
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_MODE | MSDC_CFG_CKPDN);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable card detection */
	sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	spin_lock_irqsave(&host->irqlock, flags);
	writel(0, host->base + MSDC_INTEN);
	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	spin_unlock_irqrestore(&host->irqlock, flags);

	writel(0, host->base + MSDC_IOCON);
	sdr_set_field(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	writel(0x403c0046, host->base + MSDC_PATCH_BIT);
	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	writel(0xffff0089, host->base + MSDC_PATCH_BIT1);
	sdr_set_bits(host->base + EMMC50_CFG0, EMMC50_CFG_CFCSTS_SEL);

	/* Configure to enable SDIO mode.
	 * it's must otherwise sdio cmd5 failed
	 */
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIO);

	if (host->mmc->caps & MMC_CAP_SDIO_IRQ)
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	else
	/* disable detect SDIO device interrupt function */
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

	host->def_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->def_tune_para.pad_tune = readl(host->base + MSDC_PAD_TUNE);
	dev_dbg(host->dev, "init hardware done!");
}

static void msdc_deinit_hw(struct msdc_host *host)
{
	u32 val;
	unsigned long flags;

	/* Disable and clear all interrupts */
	spin_lock_irqsave(&host->irqlock, flags);
	writel(0, host->base + MSDC_INTEN);

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	spin_unlock_irqrestore(&host->irqlock, flags);
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	int i;

	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * 2);

	gpd->gpd_info = GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	gpd->ptr = (u32)dma->bd_addr; /* physical address */
	/* gpd->next is must set for desc DMA
	 * That's why must alloc 2 gpd structure.
	 */
	gpd->next = (u32)dma->gpd_addr + sizeof(struct mt_gpdma_desc);
	memset(bd, 0, sizeof(struct mt_bdma_desc) * MAX_BD_NUM);
	for (i = 0; i < (MAX_BD_NUM - 1); i++)
		bd[i].next = (u32)dma->bd_addr + sizeof(*bd) * (i + 1);
}

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);
	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
#if 0
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			msdc_init_hw(host);
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(host->dev, "Failed to set vmmc power!\n");
				return;
			}
		}
		break;
	case MMC_POWER_ON:
		if (!IS_ERR(mmc->supply.vqmmc) && !host->vqmmc_enabled) {
			ret = regulator_enable(mmc->supply.vqmmc);
			if (ret)
				dev_err(host->dev, "Failed to set vqmmc power!\n");
			else
				host->vqmmc_enabled = true;
		}
		break;
	case MMC_POWER_OFF:
/* power always on */
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}
		break;
	default:
		break;
	}
#endif

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);
}

#define E_RESULT_PASS     (0)
#define E_RESULT_CMD_TMO  (1 << 0)
#define E_RESULT_RSP_CRC  (1 << 1)
#define E_RESULT_DAT_CRC  (1 << 2)
#define E_RESULT_DAT_TMO  (1 << 3)
#define E_RESULT_W_CRC    (1 << 4)
#define E_RESULT_ERR      (1 << 5)
#define E_RESULT_START    (1 << 6)
#define E_RESULT_PW_SMALL (1 << 7)
#define E_RESULT_KEEP_OLD (1 << 8)

#define AUTOK_CMD_TIMES			10
#define AUTOK_TUNING_INACCURACY		2
#define AUTOK_MARGIN_THOLD		3
#define AUTOK_READ			0
#define AUTOK_WRITE			1
#define AUTOK_DBG_OFF			0
#define AUTOK_DBG_ERROR			1
#define AUTOK_DBG_RES			2
#define AUTOK_DBG_WARN			3
#define AUTOK_DBG_TRACE			4
#define AUTOK_DBG_LOUD			5
unsigned int autok_debug_level = AUTOK_DBG_LOUD;

#define AUTOK_DBGPRINT(_level, _fmt ...) \
({\
	if (autok_debug_level >= _level) {\
		printk(_fmt);\
	} \
})

#define AUTOK_RAWPRINT(_fmt ...) ({\
	printk(_fmt);\
})

enum AUTOK_PARAM {
	/*
	 * command response sample selection (MSDC_SMPL_RISING,
	 * MSDC_SMPL_FALLING)
	 */
	CMD_EDGE,
	/* read data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)*/
	RDATA_EDGE,
	/* write data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)*/
	WDATA_EDGE,
	/*
	 * [Data Tune]CMD Pad RX Delay Line Control. This register is used
	 * to fine-tune CMD pad macro respose latch timing. Total 32
	 * stages[Data Tune]
	 */
	CMD_RD_D_DLY,
	/*
	 * [Data Tune]DAT0~3 Pad RX Delay Line Control (for MSDC RD), Total 32
	 * stages [Data Tune]
	 */
	DAT0_RD_D_DLY,
	DAT1_RD_D_DLY,
	DAT2_RD_D_DLY,
	DAT3_RD_D_DLY,
	/* [Clk Tune]Rx Delay Line Control. Total 32 stages [CLK Tune]*/
	DAT_RD_DLY,
	/*
	 * [INT Dly]Write Data Status && CMD Response Internal Delay Line Control.
	 * This register is used to fine-tune write status phase latched by
	 * MSDC internal clock. Total 32 stages
	 */
	DAT_WRD_DLY,
	CMD_RESP_RD_DLY,
	/* CKBUF in CKGEN Delay Selection. Total 32 stages*/
	CKGEN_MSDC_DLY_SEL,
	/*
	 * Internal MSDC clock phase selection. Total 8 stages, each stage can
	 * delay 1 clock period of msdc_src_ck
	 */
	INT_DAT_LATCH_CK,
	/*THE FOLLOWING PARAM WILL BE HARD-CODE FOR REAL CASE*/
	TUNING_PARAM_COUNT,
	/*
	 * CMD response turn around period. The turn around cycle =
	 * CMD_RSP_TA_CNTR + 2, Only for USH104 mode, this register should be
	 * set to 0 in non-UHS104 mode
	 */
	CMD_RSP_TA_CNTR,
	/* Write data and CRC status turn around period. The turn around cycle
	 * = WRDAT_CRCS_TA_CNTR + 2, Only for USH104 mode, this register
	 * should be set to 0 in non-UHS104 mode
	 */
	WRDAT_CRCS_TA_CNTR,
	/*
	 * Data line rising/falling latch fine tune selection in read
	 * transaction. 1'b0: All data line share one value indicated by
	 * MSDC_IOCON.R_D_SMPL. 1'b1: Each data line has its own selection
	 * value indicated by Data line (x): MSDC_IOCON.R_D(x)_SMPL
	 */
	READ_DATA_SMPL_SEL,
	/*
	 * Data line rising/falling latch  fine tune selection in write
	 * transaction. 1'b0: All data line share one value indicated by
	 * MSDC_IOCON.W_D_SMPL. 1'b1: Each data line has its own selection
	 * value indicated by Data line (x): MSDC_IOCON.W_D(x)_SMPL
	 */
	WRITE_DATA_SMPL_SEL,
	/*
	 * Data line delay line fine tune selection. 1'b0: All data line
	 * share one delay selection value indicated by PAD_TUNE.
	 * PAD_DAT_RD_RXDLY. 1'b1: Each data line has its own delay
	 * selection value indicated by Data line (x): DAT_RD_DLY(x).
	 *  DAT0_RD_DLY
	 */
	DATA_DLYLINE_SEL,
	/* [Data Tune]CMD & DATA Pin tune Data Selection[Data Tune Sel] */
	MSDC_DAT_TUNE_SEL,
	/*
	 * CLK Pad TX Delay Control. This register is used to add delay to
	 * CLK phase. Total 32 stages
	 */
	PAD_CLK_TXDLY,
	TOTAL_PARAM_COUNT
};

struct AUTOK_PARAM_RANGE {
	unsigned int start;
	unsigned int end;
};

struct AUTOK_PARAM_INFO {
	struct AUTOK_PARAM_RANGE range;
	char *param_name;
};

struct AUTOK_SCAN_RES {
	bool boud_info_valid;
	unsigned int bound_ckg;
	unsigned int bound_width;
	unsigned int Bound_Start;
	unsigned int Bound_End;
	unsigned int bound_edge;
};

struct AUTOK_REF_INFO {
	struct AUTOK_SCAN_RES bound_int;
	struct AUTOK_SCAN_RES bound_pad;
	struct AUTOK_SCAN_RES bound_rd;
	struct AUTOK_SCAN_RES bound_wr;
	unsigned int bound_cycle;
};

enum AUTOK_RAWD_SCAN_STA_E {
	RD_SCAN_NONE,
	RD_SCAN_PAD_BOUND_S,
	RD_SCAN_PAD_BOUND_E,
	RD_SCAN_PAD_MARGIN,
};
#define AUTOK_FINAL_CKGEN_SEL		0
#define SCALE_TA_CNTR			8
#define SCALE_INT_DAT_LATCH_CK		8

#define HIGH_VOLTAGE   1125000
#define LOW_VOLTAGE    1000000
#define CMD_TA_SDR104  4
#define DATA_TA_SDR104 2
#define CMD_TA_SDR50   1
#define DATA_TA_SDR50  1
static bool hv_autok_done;
static bool lv_autok_done;
static u8 sdio_hv_setting[TUNING_PARAM_COUNT];
static u8 sdio_lv_setting[TUNING_PARAM_COUNT];

const struct AUTOK_PARAM_INFO autok_param_info[] = {
	{{0, 1}, "CMD_EDGE"},
	{{0, 1}, "RDATA_EDGE"},
	{{0, 1}, "WDATA_EDGE"},
	{{0, 31}, "CMD_RD_D_DLY"},
	{{0, 31}, "DAT0_RD_D_DLY"},
	{{0, 31}, "DAT1_RD_D_DLY"},
	{{0, 31}, "DAT2_RD_D_DLY"},
	{{0, 31}, "DAT3_RD_D_DLY"},

	{{0, 0}, "DAT_RD_DLY"},	/*fix to 0*/
	{{0, 31}, "DAT_WRD_DLY"},	/*fix base on overlay*/
	{{0, 31}, "CMD_RESP_RD_DLY"},
	{{0, 31}, "CKGEN_MSDC_DLY_SEL"},
	{{0, 7}, "INT_DAT_LATCH_CK"},	/*fix to 0*/
	/*THE FOLLOWING PARAM WILL BE HARD-CODE FOR REAL CASE*/
	{{1, 7}, "CMD_RSP_TA_CNTR"},     /*fix to 4*/
	{{1, 7}, "WRDAT_CRCS_TA_CNTR"},  /*fix to 2*/

	{{0, 1}, "READ_DATA_SMPL_SEL"},  /*fix to 0*/
	{{0, 1}, "WRITE_DATA_SMPL_SEL"}, /*fix to 0*/
	{{0, 1}, "DATA_DLYLINE_SEL"},    /*fix to 0*/
	{{0, 1}, "MSDC_DAT_TUNE_SEL"},   /*fix to 1*/
	{{0, 31}, "PAD_CLK_TXDLY"},/*tx clk dly fix to 0 for HQA res*/
};

/**********************************************************
 * AutoK Basic Interface Implenment
 **********************************************************/
static int autok_send_raw_cmd19(struct msdc_host *host, unsigned int fifo_clr)
{
	unsigned int fifo_cnt;
	unsigned int rawcmd;
	unsigned int int_mask;
	unsigned int int_sts;
	unsigned int ret = 0;
	unsigned int i = 0;
	/*msdc hw reset*/
	if (fifo_clr)
		msdc_reset_hw(host);
	/*set timeout*/
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 0);

	/* start command */
	while (readl(host->base + SDC_STS) & SDC_STS_SDCBUSY)
		cpu_relax();

	rawcmd =  (64 << 16)|(0 << 13)|(1 << 11)|(1 << 7)|(19);
	if (fifo_clr)
		writel(1, host->base + SDC_BLK_NUM);
	else
		writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
	writel(0, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);

	/*Wait cmd done*/
	int_mask = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	while (!(readl(host->base + MSDC_INT) & int_mask))
		cpu_relax();

	int_sts = readl(host->base + MSDC_INT);
	writel((int_mask & int_sts), host->base + MSDC_INT);
	if (int_sts & MSDC_INT_CMDTMO) {
		ret |= E_RESULT_CMD_TMO;
		goto end;
	} else if (int_sts & MSDC_INT_RSPCRCERR) {
		ret |= E_RESULT_RSP_CRC;
		goto end;
	}
	/*Wait Data done*/
	while ((readl(host->base + SDC_STS) & SDC_STS_SDCBUSY)) {
		fifo_cnt = readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT;
		sdr_set_field(host->base + MSDC_DBG_SEL, 0xffff, 0x0b);
		sdr_get_field(host->base + MSDC_DBG_OUT, 0x7ff, &fifo_cnt);
		if (fifo_cnt >= 1024) {
			for (i = 0; i < 5; i++)
				readl(host->base + MSDC_RXDATA);
		}
	}
	int_sts = readl(host->base + MSDC_INT);
	int_mask = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	writel((int_mask & int_sts), host->base + MSDC_INT);
	if (int_sts & MSDC_INT_DATTMO) {
		ret |= E_RESULT_DAT_TMO;
		goto end;
	} else if (int_sts & MSDC_INT_DATCRCERR) {
		ret |= E_RESULT_DAT_CRC;
		goto end;
	}
end:
	if (fifo_clr)
		msdc_reset_hw(host);
	return ret;
}

static int autok_send_cmd19(struct msdc_host *host)
{
	return autok_send_raw_cmd19(host, 1);
}

static int autok_send_multi_cmd19(struct msdc_host *host)
{
	return autok_send_raw_cmd19(host, 0);
}

static int autok_simple_score(char *res_str, unsigned int result)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (0 == result) {
		/* maybe result	is 0*/
		strcpy(res_str, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
		return 32;
	}
	if (0xFFFFFFFF == result) {
		strcpy(res_str, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		return 0;
	}
	while (bit < 32) {	/* calc continue	zero number */
		if (result & (1	<< bit)) { /* failed */
			res_str[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;
	return old;
}

static int autok_check_scan_res(unsigned int rawdat,
				 struct AUTOK_SCAN_RES *scan_res)
{
	unsigned int bit;
	unsigned int filter = 2;
	enum AUTOK_RAWD_SCAN_STA_E RawScanSta = RD_SCAN_NONE;

	for (bit = 0; bit < 32; bit++) {
		if (rawdat & (1 << bit)) {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				scan_res->Bound_Start = 0;
				scan_res->bound_width++;
				break;
			case RD_SCAN_PAD_MARGIN:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				scan_res->Bound_Start = bit;
				scan_res->bound_width++;
				break;
			case RD_SCAN_PAD_BOUND_E:
				if ((filter) &&
				    ((bit - scan_res->Bound_End) <= AUTOK_TUNING_INACCURACY)) {
					pr_err(
						"[AUTOK]WARN: Try to filter the holes on raw data \r\n");
					RawScanSta = RD_SCAN_PAD_BOUND_S;
					scan_res->bound_width += (bit - scan_res->Bound_End);
					scan_res->Bound_End = 0;
					filter--;
				} else {
					/*
					 * Error Occours(the next bound Start),
					 * Only one Boundary allowed
					 */
					pr_err(
					"[AUTOK] Error : > 1 Boundary exist\r\n");
					return -1;
				}
				break;
			case RD_SCAN_PAD_BOUND_S:
				scan_res->bound_width++;
				break;
			default:
				break;
			}
		} else {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_MARGIN;
				break;
			case RD_SCAN_PAD_BOUND_S:
				RawScanSta = RD_SCAN_PAD_BOUND_E;
				scan_res->Bound_End = bit - 1;
				break;
			case RD_SCAN_PAD_MARGIN:
			case RD_SCAN_PAD_BOUND_E:
			default:
				break;
			}
		}
	}
	if ((scan_res->Bound_End == 0) && (scan_res->bound_width) != 0) {
		scan_res->Bound_End = scan_res->Bound_Start +
				scan_res->bound_width - 1;
	}
	return 0;
}

static int autok_calc_bit_cnt(unsigned int result)
{
	unsigned int cnt = 0;

	if (result == 0)
		return 0;
	else if (result == 0xFFFFFFFF)
		return 32;

	do {
		if (result & 0x1)
			cnt++;
		result = result	>> 1;
	} while (result);
	return cnt;
}

/*
 * function : autok_optimised_dly_sel
 *	used to sel optimised dly value for large pass window
 * params:
 *	bound -> boundary info for dly sel reference
 *	fix_edge -> dly sel value sel base on fix edge
 *	edge_sel -> dly sel form both edge and return edge res
 *	ckgen_base,cycle_cnt -> reference for opt dly sel
 *	dly_type-> 0: internal delay, 1: pad delay
*/
static int autok_optimised_dly_sel(struct AUTOK_SCAN_RES bound,
				   unsigned int *dly_sel,
				   unsigned int fix_edge,
				   unsigned int *edge_sel,
				   unsigned int ckgen_base,
				   unsigned int cycle_cnt,
				   unsigned int dly_type)
{
	int r_dly = 0;    /*for rising_edge dly sel*/
	int f_dly = 0;    /*for falling_edge dly sel*/
	int r_mglost = 0; /*for rising edge margin compress*/
	int f_mglost = 0; /*for falling edge margin compress*/
	int bd_mid_cur = 0; /*Boundary Mid Pos Current*/
	int bd_mid_prev = 0; /*Boundary Mid Pos Previous*/
	int bd_mid_next = 0; /*Boundary Mid Pos Previous*/
	int mg_mid_prev = 0; /*Margin Mid Pos prev*/
	int mg_mid_next = 31; /*Margin Mid Pos	next*/
	int dly_res = 0;
	unsigned int edge_scan = 0;
	unsigned int ckgen_sel = bound.bound_ckg;
	#define MG_THD 8

	if (cycle_cnt < 32) {
		AUTOK_RAWPRINT(
	"[AUTOK] Warning, Clock Cycle dely count < 32, not match with current Design\r\n");
		return -1;
	}
	if (edge_sel == NULL)
		edge_scan = fix_edge;
	do {
		/* mid:pre-process */
		bd_mid_cur = (bound.Bound_End + bound.Bound_Start + 1)/2;
		if (dly_type)
			bd_mid_cur -= ((int)ckgen_sel - ckgen_base) * 4;
		else
			bd_mid_cur += ((int)ckgen_sel - ckgen_base) * 4;
		bd_mid_cur = (edge_scan ^ (bound.bound_edge)) ?
			      (bd_mid_cur + cycle_cnt/2) :
			      bd_mid_cur;
		AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			       "[AUTOK] Boundary Mid Pos:%d, at edge: %d\r\n",
			       bd_mid_cur, edge_scan);
		while ((bd_mid_cur + (int)cycle_cnt) <= 0) {
			/*mid:[~, -1T]*/
			bd_mid_cur += (int)cycle_cnt;
		}
		/* mid:(-1T,0) -> no need pre-process */
		while (bd_mid_cur > 0) { /*mid:[0, +oo] ->s */
			bd_mid_cur -= (int)cycle_cnt;
		}
		if (bd_mid_cur > 0) {	/* currently bd_mid_cur must <= 0 */
			AUTOK_RAWPRINT("[AUTOK]Error BD_mid_Cur Calc Failed!\r\n");
			return -1;
		}
		/* currently bd_mid_cur must <= 0 */
		if ((bd_mid_cur + cycle_cnt) > 31) {
			bd_mid_next = bd_mid_cur + cycle_cnt;
			dly_res = (bd_mid_cur + bd_mid_next + 1) / 2;
			if ((dly_res < 0) && (edge_scan)) {
				/* falling edge */
				f_dly = 0;
				f_mglost = 0 - dly_res;
			} else if (dly_res < 0) { /* rising edge */
				r_dly = 0;
				r_mglost = 0 - dly_res;
			} else if ((dly_res > 31) && (edge_scan)) {
				/* falling edge */
				f_dly = 31;
				f_mglost = dly_res - 31;
			} else if (dly_res > 31) { /* rising edge */
				r_dly = 31;
				r_mglost = dly_res - 31;
			} else if (edge_scan) { /* falling edge */
				f_dly = dly_res;
			} else { /* rising edge */
				r_dly = dly_res;
			}
		} else { /* mid + 1T in [0,31] */
			bd_mid_prev = bd_mid_cur;
			bd_mid_cur += cycle_cnt;
			bd_mid_next = bd_mid_cur + cycle_cnt;
			mg_mid_prev = (bd_mid_prev + bd_mid_cur + 1) / 2;
			mg_mid_next = (bd_mid_next + bd_mid_cur + 1) / 2;
			if (((mg_mid_prev < 0) && (mg_mid_next > 31)) &&
			    ((mg_mid_next + MG_THD) < (31 - mg_mid_prev)) &&
			    (edge_scan)) {
				/* when right margin > left margin + 5,
				 * select right value */
				/* falling edge */
				f_dly = 31;
				f_mglost = mg_mid_next - 31;
			} else if (((mg_mid_prev < 0) && (mg_mid_next > 31)) &&
				  ((mg_mid_next + MG_THD) < (31 - mg_mid_prev))
				  ) {
				/* rising edge */
				r_dly = 31;
				r_mglost = mg_mid_next - 31;
			} else if (((mg_mid_prev < 0) && (mg_mid_next > 31)) &&
				    (edge_scan)) {
				/* falling edge */
				f_dly = 0;
				f_mglost = 0 - mg_mid_prev;
			} else if ((mg_mid_prev < 0) && (mg_mid_next > 31)) {
				/* rising edge */
				r_dly = 0;
				r_mglost = 0 - mg_mid_prev;
			} else if ((mg_mid_prev >= 0) && (edge_scan)) { /* falling edge*/
				f_dly = mg_mid_prev;
			} else if (mg_mid_prev >= 0) { /* rising edge */
				r_dly = mg_mid_prev;
			} else if (edge_scan) { /* falling edge */
				f_dly = mg_mid_next;
			} else { /* rising edge */
				r_dly = mg_mid_next;
			}
		}
		if (edge_sel == NULL)
			break;
		edge_scan ^= 0x1;
	} while (edge_scan);
	/*
	 * first check Int Dly & select smaller ,then Check Margin Lost,if
	 * margion lost so large then Sel the other edge
	 */
	if (edge_sel != NULL) {
		dly_res = (r_dly <= f_dly) ? (r_dly) : (f_dly);
		edge_scan = (r_dly <= f_dly) ? 0 : 1;
		AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Dly Sel Res for Rising edge: %d, Margin Lost:%d\r\n",
			r_dly, r_mglost);
		AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
		       "[AUTOK] Dly Sel Res for Falling edge: %d, Margin Lost:%d\r\n",
		       f_dly, f_mglost);
		if (!edge_scan) {  /* rising edge */
			if (r_mglost > (f_mglost + MG_THD))
				edge_scan = 1;
		} else { /* falling edge */
			if (f_mglost > (r_mglost + MG_THD))
				edge_scan = 0;
		}
		*edge_sel = edge_scan;
		dly_res = edge_scan ? f_dly : r_dly;
	} else {	/* fix sample edge for dly sel*/
		if (fix_edge == 0) {
			dly_res = r_dly;
			pr_err(
		"[AUTOK] Dly Sel Res for Rising edge: %d, Margin Lost:%d\r\n",
				r_dly, r_mglost);
		} else {
			dly_res = f_dly;
			pr_err(
		"[AUTOK] Dly Sel Res for Falling edge: %d, Margin Lost:%d\r\n",
				f_dly, f_mglost);
		}
	}
	*dly_sel = dly_res;
	return 0;
}

static int autok_cmd_rsp_dly_sel(struct AUTOK_REF_INFO *bdinf,
				 unsigned int *int_dly, unsigned int *cmd_edge,
				 unsigned int ckgen_base)
{
	return autok_optimised_dly_sel(bdinf->bound_int, int_dly, 0, cmd_edge,
				       ckgen_base, bdinf->bound_cycle, 0);
}

static int autok_cmd_pad_dly_sel(struct AUTOK_REF_INFO *bdinf,
				 unsigned int *pad_dly, unsigned int cmd_edge,
				 unsigned int ckgen_base)
{
	return autok_optimised_dly_sel(bdinf->bound_pad, pad_dly, cmd_edge, NULL,
				       ckgen_base, bdinf->bound_cycle, 1);
}

static int autok_data_pad_dly_sel(struct AUTOK_REF_INFO *bdinf,
				  unsigned int *pad_dly,
				  unsigned int ckgen_base)
{
	return autok_optimised_dly_sel(bdinf->bound_rd, pad_dly , 0, NULL,
			ckgen_base, bdinf->bound_cycle, 1);
}

/**
 * FUNCTION
 *  msdc_autok_adjust_param
 *
 * DESCRIPTION
 *  This	function for auto-K, adjust msdc parameter
 *
 * PARAMETERS
 *	host: msdc	host manipulator pointer
 *	param: enum of msdc parameter
 *	value: value of msdc parameter
 *	rw: AUTOK_READ/AUTOK_WRITE
 *
 * RETURN VALUES
 *	error code: 0 success,
 *		-1 parameter input error
 *		-2 read/write fail
 *		-3 else	error
 **/
int msdc_autok_adjust_param(struct msdc_host *host, enum AUTOK_PARAM param,
		u32 *value, int rw)
{
	u32 *reg;
	u32 field;

	switch (param) {
	case READ_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for READ_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_DSPLSEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for WRITE_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_W_DSPLSEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_DDLSEL);
		break;
	case MSDC_DAT_TUNE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL1;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_SEL);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for CMD_EDGE is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_RSPL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for RDATA_EDGE is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_DSPL);
		break;
	case WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug(
					"[%s] Input value(%d) for WDATA_EDGE is out of range, it should be [0~1]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_IOCON;
		field = (u32)(MSDC_IOCON_W_DSPL);
		break;
	case CMD_RD_D_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL3;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_CMD);
		break;
	case DAT0_RD_D_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for DAT0_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL2;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_D0);
		break;
	case DAT1_RD_D_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for DAT1_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL2;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_D1);
		break;
	case DAT2_RD_D_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for DAT2_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL2;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_D2);
		break;
	case DAT3_RD_D_DLY:
		reg = host->top + MSDC_TOP_DAT_TUNE_CTRL2;
		field = (u32)(MSDC_TOP_SDIO_DATA_TUNE_D3);
		break;
	case DAT_RD_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for DAT_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PAD_TUNE;
		field = (u32)(MSDC_PAD_TUNE_DATRRDLY);
		break;
	case DAT_WRD_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for DAT_WRD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PAD_TUNE;
		field = (u32)(MSDC_PAD_TUNE_DATWRDLY);
		break;
	case CMD_RESP_RD_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
					"[%s] Input value(%d) for CMD_RESP_RD_DLY is out of range, it should be [0~31]\n",
					__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PAD_TUNE;
		field = (u32)(MSDC_PAD_TUNE_CMDRRDLY);
		break;
	case INT_DAT_LATCH_CK:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug(
		"[%s] Input value(%d) for INT_DAT_LATCH_CK is out of range, it should be [0~7]\n",
				 __func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PATCH_BIT;
		field = (u32)(MSDC_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
		"[%s] Input value(%d) for CKGEN_MSDC_DLY_SEL is out of range, it should be [0~31]\n",
				 __func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PATCH_BIT;
		field = (u32)(MSDC_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug(
		"[%s] Input value(%d) for CMD_RSP_TA_CNTR is out of range, it should be [0~7]\n",
				 __func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PATCH_BIT1;
		field = (u32)(MSDC_PATCH_BIT1_CMD_RSP);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug(
		"[%s] Input value(%d) for WRDAT_CRCS_TA_CNTR is out of range, it should be [0~7]\n",
				 __func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PATCH_BIT1;
		field = (u32)(MSDC_PATCH_BIT1_WRDAT_CRCS);
		break;
	case PAD_CLK_TXDLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug(
		"[%s] Input value(%d) for PAD_CLK_TXDLY is out of range, it should be [0~31]\n",
				__func__, *value);
			return -1;
		}
		reg = host->base + MSDC_PAD_TUNE;
		field = (u32)(MSDC_PAD_TUNE_CLKTXDLY);
		break;
	default:
		pr_debug("[%s] Value of [enum AUTOK_PARAM param] is wrong\n",
			 __func__);
		return -1;
	}

	if (rw == AUTOK_READ) {
		sdr_get_field(reg, field, value);
	} else if (rw == AUTOK_WRITE)	{
		sdr_set_field(reg, field, *value);
		if (param == CKGEN_MSDC_DLY_SEL)
			mdelay(1);

	} else {
		pr_debug("[%s] Value of	[int rw] is wrong\n", __func__);
		return -1;
	}
	return 0;
}

static int autok_param_update(enum AUTOK_PARAM param_id, unsigned int result,
			      u8 *autok_tune_res)
{
	if (param_id < TUNING_PARAM_COUNT) {
		if ((result > autok_param_info[param_id].range.end) ||
		   (result < autok_param_info[param_id].range.start)) {
			AUTOK_RAWPRINT("[AUTOK]param:%d out range[%d,%d]\r\n",
			result, autok_param_info[param_id].range.start,
			autok_param_info[param_id].range.end);
			return -1;
		}
		autok_tune_res[param_id] = (u8)result;
		return 0;
	}
	AUTOK_RAWPRINT("[AUTOK]param not found\r\n");
	return -1;
}

static int autok_param_apply(struct msdc_host *host, u8 *autok_tune_res)
{
	unsigned int i;
	unsigned int value;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		value = (u8)autok_tune_res[i];
		msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
	}
	return 0;
}

static int autok_result_dump(struct msdc_host *host, u8 *autok_tune_res)
{
	unsigned int i;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		AUTOK_RAWPRINT("[AUTOK]param %s:%d\r\n",
			       autok_param_info[i].param_name,
			       autok_tune_res[i]);
	}
	return 0;
}

static void autok_tuning_parameter_init(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;

	ret = autok_param_apply(host, res);
}

static int autok_write_param(struct msdc_host *host,
			     enum AUTOK_PARAM param, u32 value)
{
	msdc_autok_adjust_param(host, param, &value, AUTOK_WRITE);
	return 0;
}

static int autok_path_sel(struct msdc_host *host, bool need_autok)
{
	void __iomem *base = host->base;

	if (need_autok) {
		autok_write_param(host, CMD_RSP_TA_CNTR, CMD_TA_SDR104);
		autok_write_param(host, WRDAT_CRCS_TA_CNTR, DATA_TA_SDR104);
		autok_write_param(host, INT_DAT_LATCH_CK, 0);
		/*data tune mode select*/
		autok_write_param(host, MSDC_DAT_TUNE_SEL, 1);
		writel(0x00000000, base + MSDC_PAD_TUNE);
	} else {
		writel(0x00000000, base + MSDC_IOCON);
		sdr_set_field(base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
		writel(0xFFFE00C9, base + MSDC_PATCH_BIT1);
		writel(0x00000000, base + MSDC_PAD_TUNE);

		/*data tune mode select*/
		writel(0, host->top + MSDC_TOP_DAT_TUNE_CTRL1);
		writel(0, host->top + MSDC_TOP_DAT_TUNE_CTRL2);
		writel(0, host->top + MSDC_TOP_DAT_TUNE_CTRL3);

		autok_write_param(host, CMD_RSP_TA_CNTR, CMD_TA_SDR50);
		autok_write_param(host, WRDAT_CRCS_TA_CNTR, DATA_TA_SDR50);
		autok_write_param(host, INT_DAT_LATCH_CK, 0);
	}

	/*clk tune all data Line share dly */
	autok_write_param(host, READ_DATA_SMPL_SEL, 0);
	autok_write_param(host, WRITE_DATA_SMPL_SEL, 0);
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/*tx clk dly fix to 0 for HQA res*/
	autok_write_param(host, PAD_CLK_TXDLY, 0);
	return 0;
}

/*
* autok_param_scan used to scan cmd or data path for different with range 0~31
* param : param used to scan
* res: return the scan result pass status for param 0~31
*/
static int autok_scan_latch_ck_param(struct msdc_host *host,
				enum AUTOK_PARAM param,
				u32 *res){
	unsigned int i, j;
	unsigned int RawData = 0;
	unsigned int ret;

	if (param != INT_DAT_LATCH_CK) {
		pr_err("[AUTOK]Error Parm out of range, please check\n\r");
		return -1;
	}
	for (i = 0; i < 8; i++) {
		autok_write_param(host, INT_DAT_LATCH_CK, i);
		for (j = 0; j < AUTOK_CMD_TIMES * 2; j++) {
			switch (j) {
			case 0:
			case 1:
			case 2:
				host->tune_latch_ck_cnt = 1;
				break;
			default:
				host->tune_latch_ck_cnt = j - 1;
				break;
			}
			ret = autok_send_multi_cmd19(host);
			if (ret & 0x3)  {
				AUTOK_RAWPRINT(
			"[AUTOK]Error CMD Failed, exit@(%d)!!!\r\n", i);
				return -1;
			} else if (ret != 0) {
				RawData |= (1 << i);
				break;
			}
		}
	}
	*res = RawData;
	return 0;
};

static int autok_scan_cmd_param(struct msdc_host *host,
				enum AUTOK_PARAM param,
				u32 *res){
	unsigned int i, j;
	unsigned int RawData = 0;
	unsigned int ret;

	for (i = 0; i < 32 ; i++) {
		autok_write_param(host, param, i);
		for (j = 0 ; j < AUTOK_CMD_TIMES * 2 ; j++) {
			ret = autok_send_cmd19(host);
			if ((ret&0x3) != 0) {
				RawData |= (1 << i);
				break;
			}
		}
	}
	*res = RawData;
	return 0;
};

static int autok_scan_read_param(struct msdc_host *host,
				 enum AUTOK_PARAM param,
				 u32 *res){
	unsigned int i, j;
	unsigned int RawData = 0;
	unsigned int ret;

	if ((param < DAT0_RD_D_DLY) || (param > DAT3_RD_D_DLY)) {
		pr_err("[AUTOK]Error Parm out of range, please check\n\r");
		return -1;
	}
	for (i = 0; i < 32; i++) {
		autok_write_param(host, DAT0_RD_D_DLY, i);
		autok_write_param(host, DAT1_RD_D_DLY, i);
		autok_write_param(host, DAT2_RD_D_DLY, i);
		autok_write_param(host, DAT3_RD_D_DLY, i);
		for (j = 0; j < AUTOK_CMD_TIMES; j++) {
			ret = autok_send_cmd19(host);
			if (ret & 0x3) {
				AUTOK_RAWPRINT(
			"[AUTOK]Error CMD Failed Need retune, exit!!!\r\n");
				return -1;
			} else if (ret != 0) {
				RawData |= (1 << i);
				break;
			}
		}
	}
	*res = RawData;
	return 0;
}

static void store_current_voltage_setting(u8 *setting)
{
	u32 vol = vcorefs_sdio_get_vcore_nml();

	if (vol == HIGH_VOLTAGE) {
		memcpy(sdio_hv_setting, setting, TUNING_PARAM_COUNT);
		hv_autok_done = true;
	} else if (vol == LOW_VOLTAGE) {
		memcpy(sdio_lv_setting, setting, TUNING_PARAM_COUNT);
		lv_autok_done = true;
	}
	pr_err("[AUTOK] cur_vol:%d, hv_done:%d, lv_done:%d\n",
	       vol, hv_autok_done, lv_autok_done);
}

static bool apply_tuned_vol_setting(struct msdc_host *host)
{
	u32 vol = vcorefs_sdio_get_vcore_nml();
	bool ret;

	if (vol == HIGH_VOLTAGE) {
		if (!hv_autok_done)
			return false;
		pr_err("[AUTOK] apply high vol:%d setting\n", HIGH_VOLTAGE);
		autok_result_dump(host, sdio_hv_setting);
		autok_tuning_parameter_init(host, sdio_hv_setting);
		ret = true;
	} else if (vol == LOW_VOLTAGE) {
		if (!lv_autok_done)
			return false;
		pr_err("[AUTOK] apply low vol:%d setting\n", LOW_VOLTAGE);
		autok_result_dump(host, sdio_lv_setting);
		autok_tuning_parameter_init(host, sdio_lv_setting);
		ret = true;
	} else {
		ret = false;
	}
	pr_err("[AUTOK] cur_vol:%d tuned:%d\n", vol, ret);
	return ret;
}

static void msdc_dump_register(struct msdc_host *host)
{
	void __iomem *base = host->base;
	pr_err("SDIO MSDC_CFG=0x%.8x\n", readl(base + MSDC_CFG));
	pr_err("SDIO MSDC_IOCON=0x%.8x\n", readl(base + MSDC_IOCON));
	pr_err("SDIO MSDC_PATCH_BIT=0x%.8x\n", readl(base + MSDC_PATCH_BIT));
	pr_err("SDIO MSDC_PATCH_BIT1=0x%.8x\n", readl(base + MSDC_PATCH_BIT1));
	pr_err("SDIO MSDC_PAD_TUNE=0x%.8x\n", readl(base + MSDC_PAD_TUNE));
	pr_err("SDIO TOP_DAT_TUNE_CTRL1=0x%.8x\n",
			readl(host->top + MSDC_TOP_DAT_TUNE_CTRL1));
	pr_err("SDIO TOP_DAT_TUNE_CTRL2=0x%.8x\n",
			readl(host->top + MSDC_TOP_DAT_TUNE_CTRL2));
	pr_err("SDIO TOP_DAT_TUNE_CTRL3=0x%.8x\n",
			readl(host->top + MSDC_TOP_DAT_TUNE_CTRL3));
	return;
}

static int execute_online_tuning(struct	msdc_host *host)
{
	unsigned int ret = 0;
	unsigned int uCkgenSel = 0;
	unsigned int uCmdPadDDly = 0;
	unsigned int uCmdIntDly = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatPadDDly = 0;
	unsigned int uWCrcIntDly = 0;
	unsigned int uLatchCK = 0;
	unsigned int uEdgeSwitchCnt = 0;
	unsigned int RawData = 0;
	unsigned int score = 0;
	unsigned int i = 0;
	/* use to got the first boundary for Int bound*/
	struct AUTOK_SCAN_RES RawScanRes;
	/* use to got the first boundary for Pad bound*/
	struct AUTOK_SCAN_RES RawScanPadRes;
	/*use to calc 1T cycle count*/
	struct AUTOK_SCAN_RES ScanRef;
	unsigned int raw_mid = 0;
	unsigned int ref_mid = 0;
	unsigned int lg_lvl = autok_debug_level;
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];
	char tune_result_str[33];
	struct AUTOK_REF_INFO g_autok_bound_info;

	autok_path_sel(host, true);
	if (apply_tuned_vol_setting(host))
		return ret;
	msdc_dump_register(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res)/sizeof(u8));
	p_autok_tune_res[DAT_WRD_DLY] = 4;
	memset((void *)&g_autok_bound_info, 0, sizeof(g_autok_bound_info)/sizeof(u8));

	/* 1 Step1.1 Find IntDly_Line Full Boundary Pos first for Referenct */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* AUTOK_RAWPRINT(
	 *	"[AUTOK]Step1.1Find Full Boundary Pos by CMD Resp Internal Delay\r\n");
	 */
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]CMD_EDGE \t CKGEN \t CMD_RESP_RD_DLY \r\n");
	uCmdEdge = 1;
	uCkgenSel = AUTOK_FINAL_CKGEN_SEL;
	uCmdPadDDly = 0;
	uEdgeSwitchCnt = 0;
	do {
		RawData = 0;
		/*Error can not found Full Boundary by swith cmd Edge twice*/
		if (uEdgeSwitchCnt > 1) {
			AUTOK_RAWPRINT(
		"[AUTOK]Error: Cannot found Full Boundary by swith cmd Edge twice");
			break;
		}
		memset(&RawScanRes, 0, sizeof(struct AUTOK_SCAN_RES));
		msdc_autok_adjust_param(host, CMD_RD_D_DLY, &uCmdPadDDly,
					AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel,
					AUTOK_WRITE);
		autok_scan_cmd_param(host, CMD_RESP_RD_DLY, &RawData);
		score = autok_simple_score(tune_result_str, RawData);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d\t %d\t %d\t %s\r\n",
			       uCmdEdge, uCkgenSel, score, tune_result_str);
		/*Pad Boundary occours*/
		if (autok_calc_bit_cnt(RawData) > 24) {
			uCmdPadDDly += 8; /*Shift Pad for Retry*/
			if (uCmdPadDDly > 31) {
				AUTOK_RAWPRINT(
				"[AUTOK] Fatal Error Invalid Scan by shift pad dly to max 31\r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Pad Boundary exist shift pad dly\r\n");
			continue;
		} else if (RawData == 0) { /* Boundary not fount */
			uCkgenSel += 2;    /*Shift CKG for Retry*/
			if (uCkgenSel > 31) {
				AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
				"[AUTOK] Fatal Error Invalid Scan by shift ckg to max 31\r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Boundary not found shift ckg\r\n");
			continue;
		} else {
			autok_check_scan_res(RawData, &RawScanRes);
			if ((RawScanRes.Bound_Start == 0) && (uCkgenSel)) {
				/*FB not found & retry*/
				/*shift CKG for FB check*/
				uCkgenSel -= 1;
				continue;
			} else if (RawScanRes.Bound_Start == 0) {
				/*can not find FB in this edge and ckg is 0*/
				uCmdEdge ^= 0x1;
				uCkgenSel = 0;
				uEdgeSwitchCnt++;
				continue;
			} else if ((RawScanRes.Bound_End == 31) &&
				  (uCkgenSel < 31)) {
				/*FB not found & retry*/
				uCkgenSel += 1;
				continue;
			} else if (RawScanRes.Bound_End == 31) {
				/*can not find FB in this edge and ckg is 31*/
				uCmdEdge ^= 0x1;
				uCkgenSel = 0;
				uEdgeSwitchCnt++;
				continue;
			} else {
				/* Full Bound found */
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK] ------Record Boundary info-------------\r\n");
				RawScanRes.boud_info_valid = true;
				RawScanRes.bound_ckg = uCkgenSel;
				RawScanRes.bound_edge = uCmdEdge;
				memcpy(&(g_autok_bound_info.bound_int),
				       &RawScanRes,
				       sizeof(struct AUTOK_SCAN_RES));
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK] FB Found CKG:%d, S:%d, E:%d, W:%d, Edge:%d, PadDdly:%d\r\n",
					       uCkgenSel, RawScanRes.Bound_Start,
					       RawScanRes.Bound_End,
					       RawScanRes.bound_width, uCmdEdge,
					       uCmdPadDDly);
				break;
			}
		}
	} while (1);
	if (g_autok_bound_info.bound_int.boud_info_valid != true) {
		AUTOK_RAWPRINT(
		"[AUTOK] Fatal Error couldn't find bound by scan ckgen and edge \r\n");
		return -1;
	}
	if (RawScanRes.bound_ckg > 16) {
		AUTOK_RAWPRINT(
		"[AUTOK] Warn Current CKG too large for reference\r\n");
	}
	/*
	 * 1 Step1.2 Caculate 1T Cycle Count -> base step1.1
	 * using CKGen && CMD_RSP_INT_DLY
	 */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* AUTOK_RAWPRINT(
	 *	"[AUTOK]Step1.2 Find 1T Cycle Count by switch CMD Edge\r\n");
	 */
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]CMD_EDGE \t CKGEN \t CMD_RESP_RD_DLY \r\n");
	uCmdEdge = RawScanRes.bound_edge ^ 0X1;
	uCkgenSel = RawScanRes.bound_ckg + (RawScanRes.Bound_End+3)/4;
	uCmdPadDDly = 0;
	do {
		RawData = 0;
		memset(&ScanRef, 0, sizeof(struct AUTOK_SCAN_RES));
		msdc_autok_adjust_param(host, CMD_RD_D_DLY, &uCmdPadDDly,
					AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel,
					AUTOK_WRITE);
		autok_scan_cmd_param(host, CMD_RESP_RD_DLY, &RawData);
		score = autok_simple_score(tune_result_str, RawData);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK]%d \t %d \t %d \t %s\r\n",
			       uCmdEdge, uCkgenSel, score, tune_result_str);
		if (autok_calc_bit_cnt(RawData) > 24) {
			/*Pad Boundary occours*/
			uCmdPadDDly += 8; /*Shift Pad for Retry*/
			if (uCmdPadDDly > 31) {
				AUTOK_RAWPRINT(
			"[AUTOK] Fatal Error Invalid Scan by shift pad dly to max 31 \r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Pad Boundary exist shift pad dly\r\n");
			continue;
		} else if (RawData == 0) { /*1T	> 64*/
			/* After switch edge still not find boundary shift
			 * small ckg may find nearest bound */
			uCkgenSel += 2;
			if (uCkgenSel > 31) {
				AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Fatal Error Invalid Scan by shift ckg to max 31\r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Boundary not found shift ckg\r\n");
			continue;
		} else {
			autok_check_scan_res(RawData, &ScanRef);
			if (ScanRef.Bound_Start == 0) {
				/* calc 1T by start*/
				g_autok_bound_info.bound_cycle =
				    ScanRef.Bound_End - RawScanRes.Bound_End
				    + (uCkgenSel - RawScanRes.bound_ckg) * 4;
				g_autok_bound_info.bound_cycle <<= 1;
			} else if (ScanRef.Bound_End == 31) {
				/*calc 1T	by end */
				g_autok_bound_info.bound_cycle =
				    ScanRef.Bound_Start - RawScanRes.Bound_Start
				    + (uCkgenSel-RawScanRes.bound_ckg) * 4;
				g_autok_bound_info.bound_cycle <<= 1;
			} else {
				/* calc 1T by mid*/
				raw_mid = RawScanRes.Bound_Start
					  + RawScanRes.bound_width / 2;
				ref_mid = ScanRef.Bound_Start
					  + ScanRef.bound_width / 2;
				g_autok_bound_info.bound_cycle = ref_mid - raw_mid
								 + (uCkgenSel-RawScanRes.bound_ckg) * 4;
				g_autok_bound_info.bound_cycle <<= 1;
			}
			break;
		}
	} while (1);
	/* AUTOK_DBGPRINT(AUTOK_DBG_RES,
	 *	"[AUTOK] ------Record 1T Cycle info-------------\r\n");
	 */
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK] 1T Cycle Count calc: %d\r\n",
		g_autok_bound_info.bound_cycle);
	/*
	 * 1 Step1.3 Find Pad_Dat_Dly_Line Full Boundary Pos for Referenct
	 */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* AUTOK_RAWPRINT(
	 * "[AUTOK]Step1.3Find Full Boundary Pos by CMD Pad Data Delay\r\n");
	 */
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]CMD_EDGE \t CKGEN \t CMD_RD_D_DLY \r\n");
	uCmdEdge = 0;
	uCkgenSel = AUTOK_FINAL_CKGEN_SEL;
	uCmdIntDly = 0;
	uEdgeSwitchCnt = 0;
	do {
		RawData = 0;
		if (uEdgeSwitchCnt > 1) {
			/* Error can not found Full Boundary by
			 * swith cmd Edge twice */
			AUTOK_RAWPRINT(
		"[AUTOK]Error: Can not found Full Boundary by swith cmd Edge twice");
			break;
		}
		memset(&RawScanPadRes, 0, sizeof(struct AUTOK_SCAN_RES));
		msdc_autok_adjust_param(host, CMD_RESP_RD_DLY, &uCmdIntDly,
					AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel,
					AUTOK_WRITE);
		autok_scan_cmd_param(host, CMD_RD_D_DLY, &RawData);
		score = autok_simple_score(tune_result_str, RawData);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK]%d \t %d \t %d \t %s\r\n",
			       uCmdEdge, uCkgenSel, score, tune_result_str);
		if (score < 8) { /*Pad Boundary occours*/
			uCmdIntDly += 8; /* Shift Pad for Retry*/
			if (uCmdIntDly > 31) {
				AUTOK_RAWPRINT(
			"[AUTOK] Fatal Error Invalid Scan by shift Int dly to max 31 \r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Int Boundary exist shift Int dly\r\n");
			continue;
		} else if (RawData == 0) { /*Boundary not fount*/
			uCkgenSel += 2; /*(32/4 - 1);*/  /*Shift CKG for Retry*/
			if (uCkgenSel > 31) {
				AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Fatal Error Invalid Scan by shift ckg to max 31\r\n");
				return -1;
			}
			AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
			"[AUTOK] Trace Boundary not found shift ckg\r\n");
			continue;
		} else {
			autok_check_scan_res(RawData, &RawScanPadRes);
			if ((RawScanPadRes.Bound_Start == 0) &&
			    (uCkgenSel < 31)) {
				/*FB not found & retry*/
				uCkgenSel += 1;
				continue;
			} else if (RawScanPadRes.Bound_Start == 0) {
				uCmdEdge ^= 0x1;
				uCkgenSel = 0;
				uEdgeSwitchCnt++;
				continue;
			} else if ((RawScanPadRes.Bound_End == 31) &&
				   (uCkgenSel > 0)) {
				/*FB not found & retry*/
				uCkgenSel -= 1;
				continue;
			} else if (RawScanPadRes.Bound_End == 31) {
				uCmdEdge ^= 0x1;
				uCkgenSel = 0;
				uEdgeSwitchCnt++;
				continue;
			} else { /* Full Bound found*/
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK] ------Record Pad Boundary info-------------\r\n");
				RawScanPadRes.boud_info_valid = true;
				RawScanPadRes.bound_ckg = uCkgenSel;
				RawScanPadRes.bound_edge = uCmdEdge;
				memcpy(&(g_autok_bound_info.bound_pad),
				       &RawScanPadRes,
				       sizeof(struct AUTOK_SCAN_RES));
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK] FB Found CKG:%d, Start:%d, End:%d, Width:%d, Edge:%d, IndDly:%d\r\n",
						uCkgenSel,
					       RawScanPadRes.Bound_Start,
					       RawScanPadRes.Bound_End,
					       RawScanPadRes.bound_width,
					       uCmdEdge, uCmdIntDly);
				break;
			}
		}
	} while (1);
	if (g_autok_bound_info.bound_pad.boud_info_valid != true) {
		AUTOK_RAWPRINT(
		"[AUTOK] Fatal Error couldn't find bound by scan ckgen and edge \r\n");
		return -1;
	}
	if (RawScanPadRes.bound_ckg > 16)
		AUTOK_RAWPRINT(
		"[AUTOK] Warn Current CKG too large for reference\r\n");
	/**
	 * Step 2: Calc optimised CMD Internal Delay base on BoundInt_info
	*/
	/* AUTOK_RAWPRINT(
	 * "[AUTOK]Step2. Calc Best Result for CMD Internal Delay and Pad Data Delay\r\n");
	 */
	if (autok_cmd_rsp_dly_sel(&g_autok_bound_info, &uCmdIntDly, &uCmdEdge,
				  AUTOK_FINAL_CKGEN_SEL) != 0)
		return -1;

	autok_param_update(CMD_RESP_RD_DLY, uCmdIntDly, p_autok_tune_res);
	autok_param_update(CMD_EDGE, uCmdEdge, p_autok_tune_res);
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
	"[AUTOK] CMD Edge Sel: %d, CMD Int Delay Sel: %d\r\n",
			uCmdEdge, uCmdIntDly);
	if (autok_cmd_pad_dly_sel(&g_autok_bound_info, &uCmdPadDDly, uCmdEdge,
				  AUTOK_FINAL_CKGEN_SEL) != 0)
		return -1;

	autok_param_update(CMD_RD_D_DLY, uCmdPadDDly, p_autok_tune_res);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD Pad Delay Sel: %d\r\n",
		       uCmdPadDDly);

	/* Step3.1 Scan & found Data Pad Bound info */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* AUTOK_RAWPRINT(
	 * "[AUTOK]Step3. Find Full Boundary Pos by Data Pad Data Delay\r\n");
	 */
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
	"[AUTOK]CKGEN \t [CMD_EDGE \t CMD_RESP_RD_DLY\t CMD_RD_D_DLY] \t DAT_RD_D_DLY\r\n");
	uCmdEdge = 0;
	uCmdIntDly = 0;
	uCmdPadDDly = 0;
	uCkgenSel = AUTOK_FINAL_CKGEN_SEL;
	do {
		memset(&RawScanPadRes, 0, sizeof(struct AUTOK_SCAN_RES));
		if (uCkgenSel > 31)
			uCkgenSel = 31;
		autok_debug_level = AUTOK_DBG_RES;
		if (autok_cmd_rsp_dly_sel(&g_autok_bound_info, &uCmdIntDly,
					  &uCmdEdge, uCkgenSel) != 0) {
			AUTOK_RAWPRINT(
			"[AUTOK]Error Cmd_Int_Dly Calc Failed\r\n");
			return -1;
		}
		if (autok_cmd_pad_dly_sel(&g_autok_bound_info, &uCmdPadDDly,
					  uCmdEdge, uCkgenSel) != 0) {
			AUTOK_RAWPRINT(
			"[AUTOK]Error Cmd_Pad_Dly Calc Failed\r\n");
			return -1;
		}
		autok_debug_level = lg_lvl;
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel,
					AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RESP_RD_DLY, &uCmdIntDly,
					AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RD_D_DLY, &uCmdPadDDly,
					AUTOK_WRITE);
		RawData = 0;
		ret = autok_scan_read_param(host, DAT0_RD_D_DLY, &RawData);
		if (ret != 0)
			pr_err("[AUTOK]Error Scan Data Path Failed\n\r");
		score = autok_simple_score(tune_result_str, RawData);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]%d  \t  [%d  \t  %d  \t  %d]  \t %d  \t  %s\r\n",
				uCkgenSel, uCmdEdge, uCmdIntDly, uCmdPadDDly,
				score, tune_result_str);

		if (score < 3) {  /* Shummo hole issue occoured */
			if (uCkgenSel < 31) {
				AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
				"[AUTOK] Trace Current CKG May occurred shummo hole, need shift ckg\r\n");
				uCkgenSel += 1; /* Shift Pad for Retry */
				continue;
			} else {
				AUTOK_RAWPRINT(
				"[AUTOK] Error Can not find Full Boundary by Shift CKG from 0~31\r\n");
				return -1;
			}
		} else if (RawData == 0) {
			/* Boundary not fount */
			if (uCkgenSel < 31) {
				/*Shift CKG for Retry*/
				uCkgenSel += 2; /* (32/4 -1); */
				AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
				"[AUTOK] Trace Boundary not found shift ckg\r\n");
				continue;
			} else {
				AUTOK_RAWPRINT(
			"[AUTOK] Error Can not find Full Boundary by Shift CKG from 0~31\r\n");
				return -1;
			}
		} else {
			autok_check_scan_res(RawData, &RawScanPadRes);
			if ((RawScanPadRes.Bound_Start == 0) &&
			    (uCkgenSel < 31)) {	/*FB not found & retry*/
				uCkgenSel += 1;
				continue;
			} else if (RawScanPadRes.Bound_Start == 0) {
				AUTOK_RAWPRINT(
			"[AUTOK] Error Can not find Full Boundary by Shift CKG from 0~31\r\n");
				return -1;
			} else if ((RawScanPadRes.Bound_End == 31) &&
				   (uCkgenSel > 0)) { /*FB not found & retry*/
				uCkgenSel -= 1;
				continue;
			} else if (RawScanPadRes.Bound_End == 31) {
				/* shift more ckg to find next
				 * full Boundary */
				uCkgenSel += 8;
				continue;
			} else if ((RawScanPadRes.Bound_End + RawScanPadRes.Bound_Start) / 2 < 16) {
				/*
				 * Full Bound found , but fail boundary position is not expected.
				 */
				uCkgenSel += 1;
				continue;
			} else { /* Full Bound found*/
				/* record Boundary info */
				autok_param_update(CKGEN_MSDC_DLY_SEL, uCkgenSel, p_autok_tune_res);
				autok_param_update(CMD_EDGE, uCmdEdge, p_autok_tune_res);
				autok_param_update(CMD_RESP_RD_DLY, uCmdIntDly, p_autok_tune_res);
				autok_param_update(CMD_RD_D_DLY, uCmdPadDDly, p_autok_tune_res);
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK] -------------Record Read Pad Boundary info-------------\r\n");
				RawScanPadRes.boud_info_valid = true;
				RawScanPadRes.bound_ckg = uCkgenSel;
				/*Read Path Sample Edge fix to rising edge*/
				RawScanPadRes.bound_edge = 0;
				memcpy(&(g_autok_bound_info.bound_rd),
				       &RawScanPadRes,
				       sizeof(struct AUTOK_SCAN_RES));
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK] FB Found CKG:%d, Start:%d, End:%d, Width:%d\r\n",
					       uCkgenSel, RawScanPadRes.Bound_Start,
					       RawScanPadRes.Bound_End,
					       RawScanPadRes.bound_width);
				break;
			}
		}
	} while (1);
	if (g_autok_bound_info.bound_rd.boud_info_valid != true) {
		AUTOK_RAWPRINT(
		"[AUTOK] Fatal Error couldn't find bound by scan ckgen and edge \r\n");
		return -1;
	}
	if (RawScanPadRes.bound_ckg > 16) {
		AUTOK_RAWPRINT(
		"[AUTOK] Warn Current CKG too large for reference\r\n");
		/* return -1; */
	}
	/* 1 Step3.2 Calc optimised data pad delay base on bound info
	 * & update Select result */
	if (autok_data_pad_dly_sel(&g_autok_bound_info, &uDatPadDDly, uCkgenSel) != 0) {
		AUTOK_RAWPRINT("[AUTOK]Error Data_Pad_Dly Calc Failed\r\n");
		return -1;
	}
	autok_param_update(DAT0_RD_D_DLY, uDatPadDDly, p_autok_tune_res);
	autok_param_update(DAT1_RD_D_DLY, uDatPadDDly, p_autok_tune_res);
	autok_param_update(DAT2_RD_D_DLY, uDatPadDDly, p_autok_tune_res);
	autok_param_update(DAT3_RD_D_DLY, uDatPadDDly, p_autok_tune_res);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Data Pad Delay Sel: %d\r\n",
		       uDatPadDDly);

	/*Step 4. optimised WCRC Internal Dely Same to CMD Resp Internal Delay*/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	uWCrcIntDly = uCmdIntDly;
	autok_param_update(DAT_WRD_DLY, uWCrcIntDly, p_autok_tune_res);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]WCRC Int Delay Sel: %d\r\n",
		       uWCrcIntDly);

	/*Step 5. Scan LatchCK Parameter for optimised*/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	pr_err("[AUTOK]Step 5.optimised LATCH_CK Dely Select\r\n");
	RawData = 0;
	if (autok_scan_latch_ck_param(host, INT_DAT_LATCH_CK, &RawData))
		pr_err("[AUTOK]Error Scan LatchCK Failed\n\r");
	score = autok_simple_score(tune_result_str, RawData);
	pr_err("[AUTOK]INT_LATCH_CK: %d \t %8.8s\r\n", score, tune_result_str);
	if ((RawData & 0xff) == 0xff) {
		pr_err("[AUTOK] Error No LatchCK Can be Test OK\r\n");
		return -1;
	}
	for (i = 0; i < 8; i++) {
		if (RawData & (1<<i)) {
			continue;
		} else {
			uLatchCK = i;
			break;
		}
	}
	autok_param_update(INT_DAT_LATCH_CK, uLatchCK, p_autok_tune_res);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]LatchCK Delay Sel: %d\r\n",
		       uLatchCK);
	pr_err(
	"[AUTOK]=================ONLINE TUNE RESULT================= \r\n");
	autok_result_dump(host, p_autok_tune_res);
	autok_tuning_parameter_init(host, p_autok_tune_res);
	store_current_voltage_setting(p_autok_tune_res);
	return 0;
}

void apply_sdio_setting(struct msdc_host *host, u32 hz)
{
	if (hz >= 200000000) {
		autok_path_sel(host, true);
		apply_tuned_vol_setting(host);
	} else {
		autok_path_sel(host, false);
	}
}

static int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	unsigned int int_en, clk_pwd, pio_mode;
	int result = 0;
	int retry_times = 5;
	u32 cur_vol;

	pm_runtime_get_sync(host->dev);
	int_en = readl(host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_PIO, &pio_mode);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwd);

	writel(0, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_PIO, 1);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);

	pr_err("[AUTOK] Execute Tuning: hz:%d, flag:%d\n",
	       mmc->ios.clock, sdio_online_tune_fail);
	if (mmc->ios.clock <= 100 * 1000 * 1000 || sdio_online_tune_fail)
		return 0;

	cur_vol = vcorefs_sdio_get_vcore_nml();
	pr_err("[AUTOK] status [vol:%d] [hv:%d] [lv:%d]\n",
	       cur_vol, hv_autok_done, lv_autok_done);

	if (!hv_autok_done) {
		do {
			vcorefs_sdio_lock_dvfs(false);
			vcorefs_sdio_set_vcore_nml(HIGH_VOLTAGE);
			if (vcorefs_sdio_get_vcore_nml() != HIGH_VOLTAGE) {
				vcorefs_sdio_unlock_dvfs(false);
				goto end;
			}

			result = execute_online_tuning(host);
			vcorefs_sdio_unlock_dvfs(false);
			pr_err("[AUTOK] tune vol:%d rst:%d\n",
				vcorefs_sdio_get_vcore_nml(), result);
			if (result)
				retry_times--;
			else
				break;
		} while (retry_times);
	}

	if (!lv_autok_done) {
		do {
			vcorefs_sdio_lock_dvfs(false);
			vcorefs_sdio_set_vcore_nml(LOW_VOLTAGE);
			if (vcorefs_sdio_get_vcore_nml() != LOW_VOLTAGE) {
				vcorefs_sdio_unlock_dvfs(false);
				goto end;
			}

			result = execute_online_tuning(host);
			vcorefs_sdio_unlock_dvfs(false);
			pr_err("[AUTOK] tune vol:%d rst:%d\n",
				vcorefs_sdio_get_vcore_nml(), result);
			if (result)
				retry_times--;
			else
				break;
		} while (retry_times);
	}

	if (result) {
		pr_err("[AUTOK] online tune fail! downgrade freq!\n");
		mmc->ios.clock = 50 * 1000 * 1000;
		mmc_set_clock(mmc, mmc->ios.clock);
		apply_sdio_setting(host, mmc->ios.clock);
		sdio_online_tune_fail = -1;
	}

 end:
	if (cur_vol != vcorefs_sdio_get_vcore_nml()) {
		vcorefs_sdio_lock_dvfs(false);
		vcorefs_sdio_set_vcore_nml(cur_vol);
		vcorefs_sdio_unlock_dvfs(false);
	}

	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_PIO, pio_mode);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwd);
	writel(int_en, host->base + MSDC_INTEN);

	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);
	return 0;
}

int sdio_stop_transfer(void)
{
	if (!wifi_sdhci_host)
		return 0;

	pr_err("[AUTOK] stop transfer. cur_vol:%d\n",
	       vcorefs_sdio_get_vcore_nml());
	mmc_claim_host(wifi_sdhci_host->mmc);
	return 0;
}

int sdio_start_ot_transfer(void)
{
	bool vol_autok_done = false;

	if (!wifi_sdhci_host)
		return 0;

	vol_autok_done = (vcorefs_sdio_get_vcore_nml() == HIGH_VOLTAGE) ?
			 hv_autok_done : lv_autok_done;
	pr_err("[AUTOK] start transfer. cur_vol:%d, done:%d\n",
	       vcorefs_sdio_get_vcore_nml(), vol_autok_done);

	if (vol_autok_done &&
	    wifi_sdhci_host->mmc->ios.clock >= 200 * 1000 * 1000)
		apply_sdio_setting(wifi_sdhci_host,
				   wifi_sdhci_host->mmc->ios.clock);
	mmc_release_host(wifi_sdhci_host->mmc);
	return 0;
}

static void msdc_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	sdr_set_bits(host->base + EMMC_IOCON, 1);
	udelay(10); /* 10us is enough */
	sdr_clr_bits(host->base + EMMC_IOCON, 1);
}

/**
 * msdc_recheck_sdio_irq - recheck whether the SDIO IRQ is lost
 * @host: The host to check.
 *
 * Host controller may lost interrupt in some special case.
 * Add sdio IRQ recheck mechanism to make sure all interrupts
 * can be processed immediately
 *
*/
static void msdc_recheck_sdio_irq(struct msdc_host *host)
{
	u32 reg_int, reg_ps;

	if (host->clock_on && (host->mmc->caps & MMC_CAP_SDIO_IRQ)
		&& host->irq_thread_alive) {
		reg_int = readl(host->base + MSDC_INT);
		reg_ps  = readl(host->base + MSDC_PS);
		if (!((reg_int & MSDC_INT_SDIOIRQ) || (reg_ps & MSDC_PS_DATA1)))
			mmc_signal_sdio_irq(host->mmc);
	}
}

static void msdc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	host->irq_thread_alive = true;
	if (enable) {
		pm_runtime_get_sync(host->dev);
		msdc_recheck_sdio_irq(host);

		spin_lock_irqsave(&host->irqlock, flags);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		spin_unlock_irqrestore(&host->irqlock, flags);
	} else {
		spin_lock_irqsave(&host->irqlock, flags);
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		spin_unlock_irqrestore(&host->irqlock, flags);
	}
}

static struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_ro = mmc_gpio_get_ro,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
	.card_busy = msdc_card_busy,
	.execute_tuning = msdc_execute_tuning,
	.hw_reset = msdc_hw_reset,
	.enable_sdio_irq = msdc_enable_sdio_irq,
};

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *res;
	struct resource *res_top;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}
	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	ret = mmc_of_parse(mmc);
	if (ret)
		goto host_free;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto host_free;
	}

	res_top = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	host->top = devm_ioremap_resource(&pdev->dev, res_top);
	if (IS_ERR(host->top)) {
		ret = PTR_ERR(host->top);
		goto host_free;
	}

	ret = mmc_regulator_get_supply(mmc);
	if (ret == -EPROBE_DEFER)
		goto host_free;

	host->src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(host->src_clk)) {
		ret = PTR_ERR(host->src_clk);
		goto host_free;
	}

	host->h_clk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(host->h_clk)) {
		ret = PTR_ERR(host->h_clk);
		goto host_free;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		goto host_free;
	}

	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_err(&pdev->dev, "Cannot find pinctrl!\n");
		goto host_free;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl, "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_err(&pdev->dev, "Cannot find pinctrl default!\n");
		goto host_free;
	}

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl, "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_err(&pdev->dev, "Cannot find pinctrl uhs!\n");
		goto host_free;
	}
	pinctrl_select_state(host->pinctrl, host->pins_uhs);

	if (!of_property_read_u32(pdev->dev.of_node,
				"hs400-ds-delay", &host->hs400_ds_delay))
		dev_dbg(&pdev->dev, "hs400-ds-delay: %x\n",
			host->hs400_ds_delay);

	host->dev = &pdev->dev;
	host->mmc = mmc;
	host->src_clk_freq = clk_get_rate(host->src_clk);
	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = host->src_clk_freq / (4 * 255);
			mmc->ocr_avail = MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33;

	mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	mmc->caps |= MMC_CAP_RUNTIME_RESUME;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_BD_NUM;
	mmc->max_seg_size = BDMA_DESC_BUFLEN;
	mmc->max_blk_size = 2048;
	mmc->max_req_size = 512 * 1024;
	mmc->max_blk_count = mmc->max_req_size / 512;
	host->dma_mask = DMA_BIT_MASK(32);
	mmc_dev(mmc)->dma_mask = &host->dma_mask;

	host->timeout_clks = 3 * 1048576;
	host->irq_thread_alive = false;
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
				2 * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	if (!host->dma.gpd || !host->dma.bd) {
		ret = -ENOMEM;
		goto release_mem;
	}
	msdc_init_gpd_bd(host, &host->dma);
	INIT_DELAYED_WORK(&host->req_timeout, msdc_request_timeout);
	spin_lock_init(&host->lock);
	spin_lock_init(&host->irqlock);

	platform_set_drvdata(pdev, mmc);
	msdc_ungate_clock(host);
	msdc_init_hw(host);

	ret = devm_request_irq(&pdev->dev, host->irq, msdc_irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, pdev->name, host);
	if (ret)
		goto release;

	pm_runtime_set_active(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);
	pm_runtime_enable(host->dev);
	ret = mmc_add_host(mmc);
	printk("%s: add new sdio_host %s, index=%d, ret=%d \n", __func__, mmc_hostname(host->mmc), mmc->index, ret);
	wifi_sdhci_host = host;

	if (ret)
		goto end;

	return 0;
end:
	pm_runtime_disable(host->dev);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);
release_mem:
	if (host->dma.gpd)
		dma_free_coherent(&pdev->dev,
			2 * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	if (host->dma.bd)
		dma_free_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);
host_free:
	mmc_free_host(mmc);

	return ret;
}

static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);

	pm_runtime_disable(host->dev);
	pm_runtime_put_noidle(host->dev);
	dma_free_coherent(&pdev->dev,
			sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

#ifdef CONFIG_PM
static void msdc_save_reg(struct msdc_host *host)
{
	host->save_para.msdc_cfg = readl(host->base + MSDC_CFG);
	host->save_para.iocon = readl(host->base + MSDC_IOCON);
	host->save_para.sdc_cfg = readl(host->base + SDC_CFG);
	host->save_para.pad_tune = readl(host->base + MSDC_PAD_TUNE);
	host->save_para.patch_bit0 = readl(host->base + MSDC_PATCH_BIT);
	host->save_para.patch_bit1 = readl(host->base + MSDC_PATCH_BIT1);
	host->save_para.pad_ds_tune = readl(host->base + PAD_DS_TUNE);
	host->save_para.emmc50_cfg0 = readl(host->base + EMMC50_CFG0);
	host->save_para.top_tune_ctrl1 = readl(host->top + MSDC_TOP_DAT_TUNE_CTRL1);
  host->save_para.top_tune_ctrl2 = readl(host->top + MSDC_TOP_DAT_TUNE_CTRL2);
  host->save_para.top_tune_ctrl3 = readl(host->top + MSDC_TOP_DAT_TUNE_CTRL3);
}

static void msdc_restore_reg(struct msdc_host *host)
{
	writel(host->save_para.msdc_cfg, host->base + MSDC_CFG);
	writel(host->save_para.iocon, host->base + MSDC_IOCON);
	writel(host->save_para.sdc_cfg, host->base + SDC_CFG);
	writel(host->save_para.pad_tune, host->base + MSDC_PAD_TUNE);
	writel(host->save_para.patch_bit0, host->base + MSDC_PATCH_BIT);
	writel(host->save_para.patch_bit1, host->base + MSDC_PATCH_BIT1);
	writel(host->save_para.pad_ds_tune, host->base + PAD_DS_TUNE);
	writel(host->save_para.emmc50_cfg0, host->base + EMMC50_CFG0);
	writel(host->save_para.top_tune_ctrl1, host->top + MSDC_TOP_DAT_TUNE_CTRL1);
	writel(host->save_para.top_tune_ctrl2, host->top + MSDC_TOP_DAT_TUNE_CTRL2);
	writel(host->save_para.top_tune_ctrl3, host->top + MSDC_TOP_DAT_TUNE_CTRL3);
}

static int msdc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	msdc_save_reg(host);
	msdc_gate_clock(host);
	return 0;
}

static int msdc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	msdc_ungate_clock(host);
	msdc_restore_reg(host);
	return 0;
}
#endif

static const struct dev_pm_ops msdc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume, NULL)
};

static const struct of_device_id msdc_of_ids[] = {
	{   .compatible = "mediatek,mt8173-sdio", },
	{}
};

static struct platform_driver mt_sdio_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = "mtk-sdio",
		.of_match_table = msdc_of_ids,
		.pm = &msdc_dev_pm_ops,
	},
};

module_platform_driver(mt_sdio_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SDIO Driver");

