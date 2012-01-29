/*
 *  linux/drivers/mmc/host/msmsdcc.h - QCT MSM7K SDC Controller
 *
 *  Copyright (C) 2008 Google, All Rights Reserved.
 *  Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * - Based on mmci.h
 */

#ifndef _MSM_SDCC_H
#define _MSM_SDCC_H

#include <linux/types.h>

#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/pm_qos_params.h>
#include <mach/sps.h>

#include <asm/sizes.h>
#include <asm/mach/mmc.h>
#include <mach/dma.h>

#define MMCIPOWER		0x000
#define MCI_PWR_OFF		0x00
#define MCI_PWR_UP		0x02
#define MCI_PWR_ON		0x03
#define MCI_OD			(1 << 6)

#define MMCICLOCK		0x004
#define MCI_CLK_ENABLE		(1 << 8)
#define MCI_CLK_PWRSAVE		(1 << 9)
#define MCI_CLK_WIDEBUS_1	(0 << 10)
#define MCI_CLK_WIDEBUS_4	(2 << 10)
#define MCI_CLK_WIDEBUS_8	(3 << 10)
#define MCI_CLK_FLOWENA		(1 << 12)
#define MCI_CLK_INVERTOUT	(1 << 13)
#define MCI_CLK_SELECTIN	(1 << 15)
#define IO_PAD_PWR_SWITCH	(1 << 21)

#define MMCIARGUMENT		0x008
#define MMCICOMMAND		0x00c
#define MCI_CPSM_RESPONSE	(1 << 6)
#define MCI_CPSM_LONGRSP	(1 << 7)
#define MCI_CPSM_INTERRUPT	(1 << 8)
#define MCI_CPSM_PENDING	(1 << 9)
#define MCI_CPSM_ENABLE		(1 << 10)
#define MCI_CPSM_PROGENA	(1 << 11)
#define MCI_CSPM_DATCMD		(1 << 12)
#define MCI_CSPM_MCIABORT	(1 << 13)
#define MCI_CSPM_CCSENABLE	(1 << 14)
#define MCI_CSPM_CCSDISABLE	(1 << 15)
#define MCI_CSPM_AUTO_CMD19	(1 << 16)


#define MMCIRESPCMD		0x010
#define MMCIRESPONSE0		0x014
#define MMCIRESPONSE1		0x018
#define MMCIRESPONSE2		0x01c
#define MMCIRESPONSE3		0x020
#define MMCIDATATIMER		0x024
#define MMCIDATALENGTH		0x028

#define MMCIDATACTRL		0x02c
#define MCI_DPSM_ENABLE		(1 << 0)
#define MCI_DPSM_DIRECTION	(1 << 1)
#define MCI_DPSM_MODE		(1 << 2)
#define MCI_DPSM_DMAENABLE	(1 << 3)
#define MCI_AUTO_PROG_DONE	(1 << 19)
#define MCI_RX_DATA_PEND	(1 << 20)

#define MMCIDATACNT		0x030
#define MMCISTATUS		0x034
#define MCI_CMDCRCFAIL		(1 << 0)
#define MCI_DATACRCFAIL		(1 << 1)
#define MCI_CMDTIMEOUT		(1 << 2)
#define MCI_DATATIMEOUT		(1 << 3)
#define MCI_TXUNDERRUN		(1 << 4)
#define MCI_RXOVERRUN		(1 << 5)
#define MCI_CMDRESPEND		(1 << 6)
#define MCI_CMDSENT		(1 << 7)
#define MCI_DATAEND		(1 << 8)
#define MCI_DATABLOCKEND	(1 << 10)
#define MCI_CMDACTIVE		(1 << 11)
#define MCI_TXACTIVE		(1 << 12)
#define MCI_RXACTIVE		(1 << 13)
#define MCI_TXFIFOHALFEMPTY	(1 << 14)
#define MCI_RXFIFOHALFFULL	(1 << 15)
#define MCI_TXFIFOFULL		(1 << 16)
#define MCI_RXFIFOFULL		(1 << 17)
#define MCI_TXFIFOEMPTY		(1 << 18)
#define MCI_RXFIFOEMPTY		(1 << 19)
#define MCI_TXDATAAVLBL		(1 << 20)
#define MCI_RXDATAAVLBL		(1 << 21)
#define MCI_SDIOINTR		(1 << 22)
#define MCI_PROGDONE		(1 << 23)
#define MCI_ATACMDCOMPL		(1 << 24)
#define MCI_SDIOINTROPE		(1 << 25)
#define MCI_CCSTIMEOUT		(1 << 26)
#define MCI_AUTOCMD19TIMEOUT	(1 << 30)

#define MMCICLEAR		0x038
#define MCI_CMDCRCFAILCLR	(1 << 0)
#define MCI_DATACRCFAILCLR	(1 << 1)
#define MCI_CMDTIMEOUTCLR	(1 << 2)
#define MCI_DATATIMEOUTCLR	(1 << 3)
#define MCI_TXUNDERRUNCLR	(1 << 4)
#define MCI_RXOVERRUNCLR	(1 << 5)
#define MCI_CMDRESPENDCLR	(1 << 6)
#define MCI_CMDSENTCLR		(1 << 7)
#define MCI_DATAENDCLR		(1 << 8)
#define MCI_STARTBITERRCLR	(1 << 9)
#define MCI_DATABLOCKENDCLR	(1 << 10)

#define MCI_SDIOINTRCLR		(1 << 22)
#define MCI_PROGDONECLR		(1 << 23)
#define MCI_ATACMDCOMPLCLR	(1 << 24)
#define MCI_SDIOINTROPECLR	(1 << 25)
#define MCI_CCSTIMEOUTCLR 	(1 << 26)

#define MCI_CLEAR_STATIC_MASK	\
	(MCI_CMDCRCFAILCLR|MCI_DATACRCFAILCLR|MCI_CMDTIMEOUTCLR|\
	MCI_DATATIMEOUTCLR|MCI_TXUNDERRUNCLR|MCI_RXOVERRUNCLR|  \
	MCI_CMDRESPENDCLR|MCI_CMDSENTCLR|MCI_DATAENDCLR|	\
	MCI_STARTBITERRCLR|MCI_DATABLOCKENDCLR|MCI_SDIOINTRCLR|	\
	MCI_SDIOINTROPECLR|MCI_PROGDONECLR|MCI_ATACMDCOMPLCLR|	\
	MCI_CCSTIMEOUTCLR)

#define MMCIMASK0		0x03c
#define MCI_CMDCRCFAILMASK	(1 << 0)
#define MCI_DATACRCFAILMASK	(1 << 1)
#define MCI_CMDTIMEOUTMASK	(1 << 2)
#define MCI_DATATIMEOUTMASK	(1 << 3)
#define MCI_TXUNDERRUNMASK	(1 << 4)
#define MCI_RXOVERRUNMASK	(1 << 5)
#define MCI_CMDRESPENDMASK	(1 << 6)
#define MCI_CMDSENTMASK		(1 << 7)
#define MCI_DATAENDMASK		(1 << 8)
#define MCI_DATABLOCKENDMASK	(1 << 10)
#define MCI_CMDACTIVEMASK	(1 << 11)
#define MCI_TXACTIVEMASK	(1 << 12)
#define MCI_RXACTIVEMASK	(1 << 13)
#define MCI_TXFIFOHALFEMPTYMASK	(1 << 14)
#define MCI_RXFIFOHALFFULLMASK	(1 << 15)
#define MCI_TXFIFOFULLMASK	(1 << 16)
#define MCI_RXFIFOFULLMASK	(1 << 17)
#define MCI_TXFIFOEMPTYMASK	(1 << 18)
#define MCI_RXFIFOEMPTYMASK	(1 << 19)
#define MCI_TXDATAAVLBLMASK	(1 << 20)
#define MCI_RXDATAAVLBLMASK	(1 << 21)
#define MCI_SDIOINTMASK		(1 << 22)
#define MCI_PROGDONEMASK	(1 << 23)
#define MCI_ATACMDCOMPLMASK	(1 << 24)
#define MCI_SDIOINTOPERMASK	(1 << 25)
#define MCI_CCSTIMEOUTMASK	(1 << 26)
#define MCI_AUTOCMD19TIMEOUTMASK (1 << 30)

#define MMCIMASK1		0x040
#define MMCIFIFOCNT		0x044
#define MCI_VERSION		0x050
#define MCICCSTIMER		0x058
#define MCI_DLL_CONFIG		0x060
#define MCI_DLL_EN		(1 << 16)
#define MCI_CDR_EN		(1 << 17)
#define MCI_CK_OUT_EN		(1 << 18)
#define MCI_CDR_EXT_EN		(1 << 19)
#define MCI_DLL_PDN		(1 << 29)
#define MCI_DLL_RST		(1 << 30)

#define MCI_DLL_STATUS		0x068
#define MCI_DLL_LOCK		(1 << 7)

#define MCI_STATUS2		0x06C
#define MCI_MCLK_REG_WR_ACTIVE	(1 << 0)

#define MMCIFIFO		0x080 /* to 0x0bc */

#define MCI_TEST_INPUT		0x0D4

#define MCI_IRQENABLE	\
	(MCI_CMDCRCFAILMASK|MCI_DATACRCFAILMASK|MCI_CMDTIMEOUTMASK|	\
	MCI_DATATIMEOUTMASK|MCI_TXUNDERRUNMASK|MCI_RXOVERRUNMASK|	\
	MCI_CMDRESPENDMASK|MCI_CMDSENTMASK|MCI_DATAENDMASK|		\
	MCI_PROGDONEMASK|MCI_AUTOCMD19TIMEOUTMASK)

#define MCI_IRQ_PIO 	\
	(MCI_RXDATAAVLBLMASK | MCI_TXDATAAVLBLMASK | 	\
	MCI_RXFIFOEMPTYMASK | MCI_TXFIFOEMPTYMASK | MCI_RXFIFOFULLMASK |\
	MCI_TXFIFOFULLMASK | MCI_RXFIFOHALFFULLMASK |			\
	MCI_TXFIFOHALFEMPTYMASK | MCI_RXACTIVEMASK | MCI_TXACTIVEMASK)

/*
 * The size of the FIFO in bytes.
 */
#define MCI_FIFOSIZE	(16*4)

#define MCI_FIFOHALFSIZE (MCI_FIFOSIZE / 2)

#define NR_SG		128

#define MSM_MMC_IDLE_TIMEOUT	5000 /* msecs */

/*
 * Set the request timeout to 10secs to allow
 * bad cards/controller to respond.
 */
#define MSM_MMC_REQ_TIMEOUT	10000 /* msecs */
#define MSM_MMC_DISABLE_TIMEOUT        200 /* msecs */

/*
 * Controller HW limitations
 */
#define MCI_DATALENGTH_BITS	25
#define MMC_MAX_REQ_SIZE	((1 << MCI_DATALENGTH_BITS) - 1)
/* MCI_DATA_CTL BLOCKSIZE up to 4096 */
#define MMC_MAX_BLK_SIZE	4096
#define MMC_MIN_BLK_SIZE	512
#define MMC_MAX_BLK_CNT		(MMC_MAX_REQ_SIZE / MMC_MIN_BLK_SIZE)

/* 64KiB */
#define MAX_SG_SIZE		(64 * 1024)
#define MAX_NR_SG_DMA_PIO	(MMC_MAX_REQ_SIZE / MAX_SG_SIZE)

/*
 * BAM limitations
 */
/* upto 16 bits (64K - 1) */
#define SPS_MAX_DESC_FIFO_SIZE	65535
/* 16KiB */
#define SPS_MAX_DESC_SIZE	(16 * 1024)
/* Each descriptor is of length 8 bytes */
#define SPS_MAX_DESC_LENGTH	8
#define SPS_MAX_DESCS		(SPS_MAX_DESC_FIFO_SIZE / SPS_MAX_DESC_LENGTH)

/*
 * DMA limitations
 */
/* upto 16 bits (64K - 1) */
#define MMC_MAX_DMA_ROWS (64 * 1024 - 1)
#define MMC_MAX_DMA_BOX_LENGTH (MMC_MAX_DMA_ROWS * MCI_FIFOSIZE)
#define MMC_MAX_DMA_CMDS (MAX_NR_SG_DMA_PIO * (MMC_MAX_REQ_SIZE / \
		MMC_MAX_DMA_BOX_LENGTH))

struct clk;

struct msmsdcc_nc_dmadata {
	dmov_box	cmd[MMC_MAX_DMA_CMDS];
	uint32_t	cmdptr;
};

struct msmsdcc_dma_data {
	struct msmsdcc_nc_dmadata	*nc;
	dma_addr_t			nc_busaddr;
	dma_addr_t			cmd_busaddr;
	dma_addr_t			cmdptr_busaddr;

	struct msm_dmov_cmd		hdr;
	enum dma_data_direction		dir;

	struct scatterlist		*sg;
	int				num_ents;

	int				channel;
	int				crci;
	struct msmsdcc_host		*host;
	int				busy; /* Set if DM is busy */
	unsigned int 			result;
	struct msm_dmov_errdata		err;
};

struct msmsdcc_pio_data {
	struct scatterlist	*sg;
	unsigned int		sg_len;
	unsigned int		sg_off;
};

struct msmsdcc_curr_req {
	struct mmc_request	*mrq;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	unsigned int		xfer_size;	/* Total data size */
	unsigned int		xfer_remain;	/* Bytes remaining to send */
	unsigned int		data_xfered;	/* Bytes acked by BLKEND irq */
	int			got_dataend;
	int			wait_for_auto_prog_done;
	int			got_auto_prog_done;
	int			user_pages;
};

struct msmsdcc_sps_ep_conn_data {
	struct sps_pipe			*pipe_handle;
	struct sps_connect		config;
	struct sps_register_event	event;
};

struct msmsdcc_sps_data {
	struct msmsdcc_sps_ep_conn_data	prod;
	struct msmsdcc_sps_ep_conn_data	cons;
	struct sps_event_notify		notify;
	enum dma_data_direction		dir;
	struct scatterlist		*sg;
	int				num_ents;
	u32				bam_handle;
	unsigned int			src_pipe_index;
	unsigned int			dest_pipe_index;
	unsigned int			busy;
	unsigned int			xfer_req_cnt;
	bool				pipe_reset_pending;
	struct tasklet_struct		tlet;
};

struct msmsdcc_host {
	struct resource		*core_irqres;
	struct resource		*bam_irqres;
	struct resource		*core_memres;
	struct resource		*bam_memres;
	struct resource		*dml_memres;
	struct resource		*dmares;
	struct resource		*dma_crci_res;
	void __iomem		*base;
	void __iomem		*dml_base;
	void __iomem		*bam_base;

	int			pdev_id;

	struct msmsdcc_curr_req	curr;

	struct mmc_host		*mmc;
	struct clk		*clk;		/* main MMC bus clock */
	struct clk		*pclk;		/* SDCC peripheral bus clock */
	struct clk		*dfab_pclk;	/* Daytona Fabric SDCC clock */
	unsigned int		clks_on;	/* set if clocks are enabled */

	unsigned int		eject;		/* eject state */

	spinlock_t		lock;

	unsigned int		clk_rate;	/* Current clock rate */
	unsigned int		pclk_rate;
	unsigned int		ddr_doubled_clk_rate;

	u32			pwr;
	struct mmc_platform_data *plat;
	u32			sdcc_version;

	unsigned int		oldstat;

	struct msmsdcc_dma_data	dma;
	struct msmsdcc_sps_data sps;
	bool			is_dma_mode;
	bool			is_sps_mode;
	struct msmsdcc_pio_data	pio;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	int polling_enabled;
#endif

	struct tasklet_struct 	dma_tlet;

	unsigned int prog_enable;

	/* Command parameters */
	unsigned int		cmd_timeout;
	unsigned int		cmd_pio_irqmask;
	unsigned int		cmd_datactrl;
	struct mmc_command	*cmd_cmd;
	u32					cmd_c;

	unsigned int	mci_irqenable;
	unsigned int	dummy_52_needed;
	unsigned int	dummy_52_sent;

	unsigned int	sdio_irq_disabled;
	bool		is_resumed;
	struct wake_lock	sdio_wlock;
	struct wake_lock	sdio_suspend_wlock;
	unsigned int    sdcc_suspending;

	unsigned int sdcc_irq_disabled;
	struct timer_list req_tout_timer;
	unsigned long reg_write_delay;
	bool io_pad_pwr_switch;
	bool cmd19_tuning_in_progress;
	bool tuning_needed;
	bool sdio_gpio_lpm;
	bool irq_wake_enabled;
	struct pm_qos_request_list pm_qos_req_dma;
};

int msmsdcc_set_pwrsave(struct mmc_host *mmc, int pwrsave);
int msmsdcc_sdio_al_lpm(struct mmc_host *mmc, bool enable);

#ifdef CONFIG_MSM_SDIO_AL

static inline int msmsdcc_lpm_enable(struct mmc_host *mmc)
{
	return msmsdcc_sdio_al_lpm(mmc, true);
}

static inline int msmsdcc_lpm_disable(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	int ret;

	ret = msmsdcc_sdio_al_lpm(mmc, false);
	wake_unlock(&host->sdio_wlock);
	return ret;
}
#endif

#endif
