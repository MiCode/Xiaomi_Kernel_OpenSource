/*
 * linux/drivers/mmc/host/msm_sdcc_dml.c - Qualcomm MSM SDCC DML Driver
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <asm/sizes.h>
#include <mach/msm_iomap.h>

#include "msm_sdcc_dml.h"

/*
 * DML registers definations
 */

/* DML config register defination */
#define DML_CONFIG 0x0000
#define PRODUCER_CRCI_DIS   0x00
#define PRODUCER_CRCI_X_SEL 0x01
#define PRODUCER_CRCI_Y_SEL 0x02
#define PRODUCER_CRCI_MSK   0x3
#define CONSUMER_CRCI_DIS   (0x00 << 2)
#define CONSUMER_CRCI_X_SEL (0x01 << 2)
#define CONSUMER_CRCI_Y_SEL (0x02 << 2)
#define CONSUMER_CRCI_MSK   (0x3 << 2)
#define PRODUCER_TRANS_END_EN (1 << 4)
#define BYPASS (1 << 16)
#define DIRECT_MODE (1 << 17)
#define INFINITE_CONS_TRANS (1 << 18)

/* DML status register defination */
#define DML_STATUS 0x0004
#define PRODUCER_IDLE (1 << 0)
#define CONSUMER_IDLE (1 << 16)

/*
 * DML SW RESET register defination
 * NOTE: write to this register resets the DML core.
 * All internal state information will be lost and all
 * register values will be reset as well
 */
#define DML_SW_RESET 0x0008

/*
 * DML PRODUCER START register defination
 * NOTE: A write to this register triggers the DML
 * Producer state machine. No SW register values will be
 * altered.
 */
#define DML_PRODUCER_START 0x000C

/*
 * DML CONSUMER START register defination
 * NOTE: A write to this register triggers the DML
 * Consumer state machine. No SW register values will be
 * altered.
 */
#define DML_CONSUMER_START 0x0010

/*
 * DML producer pipe logical size register defination
 * NOTE: This register holds the size of the producer pipe
 * (in units of bytes) _to_ which the peripheral can
 * keep writing data to when its the PRODUCER.
 */
#define DML_PRODUCER_PIPE_LOGICAL_SIZE 0x0014

/*
 * DML producer pipe logical size register defination
 * NOTE: This register holds the size of the consumer pipe
 * (in units of bytes) _from_ which the peripheral
 * can keep _reading_ data from when its the CONSUMER.
 */
#define DML_CONSUMER_PIPE_LOGICAL_SIZE 0x00018

/*
 * DML PIPE ID register
 * This register holds pipe IDs that services
 * the producer and consumer side of the peripheral
 */
#define DML_PIPE_ID 0x0001C
#define PRODUCER_PIPE_ID_SHFT 0
#define PRODUCER_PIPE_ID_MSK 0x1f
#define CONSUMER_PIPE_ID_SHFT 16
#define CONSUMER_PIPE_ID_MSK (0x1f << 16)

/*
 * DML Producer trackers register defination.
 * This register is for debug purposes only. They reflect
 * the value of the producer block and transaction counters
 * when read. The values may be dynamically changing when
 * a transaction is in progress.
 */
#define DML_PRODUCER_TRACKERS 0x00020
#define PROD_BLOCK_CNT_SHFT 0
#define PROD_BLOCK_CNT_MSK  0xffff
#define PROD_TRANS_CNT_SHFT 16
#define PROD_TRANS_CNT_MSK  (0xffff << 16)

/*
 * DML Producer BAM block size register defination.
 * This regsiter holds the block size, in units of bytes,
 * associated with the Producer BAM. The DML asserts the
 * block_end side band signal to the BAM whenever the producer
 * side of the peripheral has generated the said amount of data.
 * This register value should be an integral multiple of the
 * Producer CRCI Block Size.
 */
#define DML_PRODUCER_BAM_BLOCK_SIZE 0x00024

/*
 * DML Producer BAM Transaction size defination.
 * This regsiter holds the transaction size, in units of bytes,
 * associated with the Producer BAM. The DML asserts the transaction_end
 * side band signal to the BAM whenever the producer side of the peripheral
 * has generated the said amount of data.
 */
#define DML_PRODUCER_BAM_TRANS_SIZE 0x00028

/*
 * DML Direct mode base address defination
 * This register is used whenever the DIRECT_MODE bit
 * in config register is set.
 */
#define DML_DIRECT_MODE_BASE_ADDR 0x002C
#define PRODUCER_BASE_ADDR_BSHFT 0
#define PRODUCER_BASE_ADDR_BMSK  0xffff
#define CONSUMER_BASE_ADDR_BSHFT 16
#define CONSUMER_BASE_ADDR_BMSK  (0xffff << 16)

/*
 * DMA Debug and status register defination.
 * These are the read-only registers useful debugging.
 */
#define DML_DEBUG 0x0030
#define DML_BAM_SIDE_STATUS_1 0x0034
#define DML_BAM_SIDE_STATUS_2 0x0038

/* other definations */
#define PRODUCER_PIPE_LOGICAL_SIZE 4096
#define CONSUMER_PIPE_LOGICAL_SIZE 4096

#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
/**
 * Initialize DML HW connected with SDCC core
 *
 */
int msmsdcc_dml_init(struct msmsdcc_host *host)
{
	int rc = 0;
	u32 config = 0;
	void __iomem *dml_base;

	if (!host->dml_base) {
		host->dml_base = ioremap(host->dml_memres->start,
					resource_size(host->dml_memres));
		if (!host->dml_base) {
			pr_err("%s: DML ioremap() failed!!! phys_addr=0x%x,"
				" size=0x%x", mmc_hostname(host->mmc),
				host->dml_memres->start,
				(host->dml_memres->end -
				host->dml_memres->start));
			rc = -ENOMEM;
			goto out;
		}
		pr_info("%s: Qualcomm MSM SDCC-DML at 0x%016llx\n",
			mmc_hostname(host->mmc),
			(unsigned long long)host->dml_memres->start);
	}

	dml_base = host->dml_base;
	/* Reset the DML block */
	writel_relaxed(1, (dml_base + DML_SW_RESET));

	/* Disable the producer and consumer CRCI */
	config = (PRODUCER_CRCI_DIS | CONSUMER_CRCI_DIS);
	/*
	 * Disable the bypass mode. Bypass mode will only be used
	 * if data transfer is to happen in PIO mode and don't
	 * want the BAM interface to connect with SDCC-DML.
	 */
	config &= ~BYPASS;
	/*
	 * Disable direct mode as we don't DML to MASTER the AHB bus.
	 * BAM connected with DML should MASTER the AHB bus.
	 */
	config &= ~DIRECT_MODE;
	/*
	 * Disable infinite mode transfer as we won't be doing any
	 * infinite size data transfers. All data transfer will be
	 * of finite data size.
	 */
	config &= ~INFINITE_CONS_TRANS;
	writel_relaxed(config, (dml_base + DML_CONFIG));

	/*
	 * Initialize the logical BAM pipe size for producer
	 * and consumer.
	 */
	writel_relaxed(PRODUCER_PIPE_LOGICAL_SIZE,
		(dml_base + DML_PRODUCER_PIPE_LOGICAL_SIZE));
	writel_relaxed(CONSUMER_PIPE_LOGICAL_SIZE,
		(dml_base + DML_CONSUMER_PIPE_LOGICAL_SIZE));

	/* Initialize Producer/consumer pipe id */
	writel_relaxed(host->sps.src_pipe_index |
		(host->sps.dest_pipe_index << CONSUMER_PIPE_ID_SHFT),
		(dml_base + DML_PIPE_ID));
	mb();
out:
	return rc;
}

/**
 * Soft reset DML HW
 *
 */
void msmsdcc_dml_reset(struct msmsdcc_host *host)
{
	/* Reset the DML block */
	writel_relaxed(1, (host->dml_base + DML_SW_RESET));
	mb();
}

/**
 * Checks if DML HW is busy or not?
 *
 */
bool msmsdcc_is_dml_busy(struct msmsdcc_host *host)
{
	return !(readl_relaxed(host->dml_base + DML_STATUS) & PRODUCER_IDLE) ||
		!(readl_relaxed(host->dml_base + DML_STATUS) & CONSUMER_IDLE);
}

/**
 * Start data transfer.
 *
 */
void msmsdcc_dml_start_xfer(struct msmsdcc_host *host, struct mmc_data *data)
{
	u32 config;
	void __iomem *dml_base = host->dml_base;

	if (data->flags & MMC_DATA_READ) {
		/* Read operation: configure DML for producer operation */
		/* Set producer CRCI-x and disable consumer CRCI */
		config = readl_relaxed(dml_base + DML_CONFIG);
		config = (config & ~PRODUCER_CRCI_MSK) | PRODUCER_CRCI_X_SEL;
		config = (config & ~CONSUMER_CRCI_MSK) | CONSUMER_CRCI_DIS;
		writel_relaxed(config, (dml_base + DML_CONFIG));

		/* Set the Producer BAM block size */
		writel_relaxed(data->blksz, (dml_base +
					DML_PRODUCER_BAM_BLOCK_SIZE));

		/* Set Producer BAM Transaction size */
		writel_relaxed(host->curr.xfer_size,
			(dml_base + DML_PRODUCER_BAM_TRANS_SIZE));
		/* Set Producer Transaction End bit */
		writel_relaxed((readl_relaxed(dml_base + DML_CONFIG)
			| PRODUCER_TRANS_END_EN),
			(dml_base + DML_CONFIG));
		/* Trigger producer */
		writel_relaxed(1, (dml_base + DML_PRODUCER_START));
	} else {
		/* Write operation: configure DML for consumer operation */
		/* Set consumer CRCI-x and disable producer CRCI*/
		config = readl_relaxed(dml_base + DML_CONFIG);
		config = (config & ~CONSUMER_CRCI_MSK) | CONSUMER_CRCI_X_SEL;
		config = (config & ~PRODUCER_CRCI_MSK) | PRODUCER_CRCI_DIS;
		writel_relaxed(config, (dml_base + DML_CONFIG));
		/* Clear Producer Transaction End bit */
		writel_relaxed((readl_relaxed(dml_base + DML_CONFIG)
			& ~PRODUCER_TRANS_END_EN),
			(dml_base + DML_CONFIG));
		/* Trigger consumer */
		writel_relaxed(1, (dml_base + DML_CONSUMER_START));
	}
	mb();
}

/**
 * Deinitialize DML HW connected with SDCC core
 *
 */
void msmsdcc_dml_exit(struct msmsdcc_host *host)
{
	/* Put DML block in reset state before exiting */
	msmsdcc_dml_reset(host);
	iounmap(host->dml_base);
}
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */
