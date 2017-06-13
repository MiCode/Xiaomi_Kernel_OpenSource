/* Copyright (c) 2012, 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/coresight.h>
#include <linux/coresight-cti.h>
#include <linux/amba/bus.h>
#include <asm/cacheflush.h>
#include <linux/msm-sps.h>
#include <linux/usb_bam.h>
#include <linux/usb/usb_qdss.h>
#include <soc/qcom/memory_dump.h>

#include "coresight-priv.h"

#define TMC_RSZ			0x004
#define TMC_STS			0x00c
#define TMC_RRD			0x010
#define TMC_RRP			0x014
#define TMC_RWP			0x018
#define TMC_TRG			0x01c
#define TMC_CTL			0x020
#define TMC_RWD			0x024
#define TMC_MODE		0x028
#define TMC_LBUFLEVEL		0x02c
#define TMC_CBUFLEVEL		0x030
#define TMC_BUFWM		0x034
#define TMC_RRPHI		0x038
#define TMC_RWPHI		0x03c
#define TMC_AXICTL		0x110
#define TMC_DBALO		0x118
#define TMC_DBAHI		0x11c
#define TMC_FFSR		0x300
#define TMC_FFCR		0x304
#define TMC_PSCR		0x308
#define TMC_ITMISCOP0		0xee0
#define TMC_ITTRFLIN		0xee8
#define TMC_ITATBDATA0		0xeec
#define TMC_ITATBCTR2		0xef0
#define TMC_ITATBCTR1		0xef4
#define TMC_ITATBCTR0		0xef8

/* register description */
/* TMC_CTL - 0x020 */
#define TMC_CTL_CAPT_EN		BIT(0)
/* TMC_STS - 0x00C */
#define TMC_STS_TRIGGERED	BIT(1)
/* TMC_AXICTL - 0x110 */
#define TMC_AXICTL_PROT_CTL_B0	BIT(0)
#define TMC_AXICTL_PROT_CTL_B1	BIT(1)
#define TMC_AXICTL_SCT_GAT_MODE	BIT(7)
#define TMC_AXICTL_WR_BURST_LEN 0xF00
/* TMC_FFCR - 0x304 */
#define TMC_FFCR_EN_FMT		BIT(0)
#define TMC_FFCR_EN_TI		BIT(1)
#define TMC_FFCR_FON_FLIN	BIT(4)
#define TMC_FFCR_FON_TRIG_EVT	BIT(5)
#define TMC_FFCR_FLUSHMAN	BIT(6)
#define TMC_FFCR_TRIGON_TRIGIN	BIT(8)
#define TMC_FFCR_STOP_ON_FLUSH	BIT(12)

#define TMC_STS_TRIGGERED_BIT	2
#define TMC_FFCR_FLUSHMAN_BIT	6

#define TMC_ETR_SG_ENT_TO_BLK(phys_pte)	(((phys_addr_t)phys_pte >> 4)	\
					 << PAGE_SHIFT)
#define TMC_ETR_SG_ENT(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x2)
#define TMC_ETR_SG_NXT_TBL(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x3)
#define TMC_ETR_SG_LST_ENT(phys_pte)	(((phys_pte >> PAGE_SHIFT) << 4) | 0x1)

#define TMC_ETR_BAM_PIPE_INDEX	0
#define TMC_ETR_BAM_NR_PIPES	2

#define TMC_ETFETB_DUMP_MAGIC_V2	(0x42445953)
#define TMC_REG_DUMP_MAGIC_V2		(0x42445953)
#define TMC_REG_DUMP_VER		(1)

enum tmc_config_type {
	TMC_CONFIG_TYPE_ETB,
	TMC_CONFIG_TYPE_ETR,
	TMC_CONFIG_TYPE_ETF,
};

enum tmc_mode {
	TMC_MODE_CIRCULAR_BUFFER,
	TMC_MODE_SOFTWARE_FIFO,
	TMC_MODE_HARDWARE_FIFO,
};

enum tmc_mem_intf_width {
	TMC_MEM_INTF_WIDTH_32BITS	= 0x2,
	TMC_MEM_INTF_WIDTH_64BITS	= 0x3,
	TMC_MEM_INTF_WIDTH_128BITS	= 0x4,
	TMC_MEM_INTF_WIDTH_256BITS	= 0x5,
};

enum tmc_etr_mem_type {
	TMC_ETR_MEM_TYPE_CONTIG,
	TMC_ETR_MEM_TYPE_SG,
};

static const char * const str_tmc_etr_mem_type[] = {
	[TMC_ETR_MEM_TYPE_CONTIG]	= "contig",
	[TMC_ETR_MEM_TYPE_SG]		= "sg",
};

enum tmc_etr_out_mode {
	TMC_ETR_OUT_MODE_NONE,
	TMC_ETR_OUT_MODE_MEM,
	TMC_ETR_OUT_MODE_USB,
};

static const char * const str_tmc_etr_out_mode[] = {
	[TMC_ETR_OUT_MODE_NONE]		= "none",
	[TMC_ETR_OUT_MODE_MEM]		= "mem",
	[TMC_ETR_OUT_MODE_USB]		= "usb",
};

struct tmc_etr_bam_data {
	struct sps_bam_props	props;
	unsigned long		handle;
	struct sps_pipe		*pipe;
	struct sps_connect	connect;
	uint32_t		src_pipe_idx;
	unsigned long		dest;
	uint32_t		dest_pipe_idx;
	struct sps_mem_buffer	desc_fifo;
	struct sps_mem_buffer	data_fifo;
	bool			enable;
};

/**
 * struct tmc_drvdata - specifics associated to an TMC component
 * @base:	memory mapped base address for this component.
 * @dev:	the device entity associated to this component.
 * @csdev:	component vitals needed by the framework.
 * @miscdev:	specifics to handle "/dev/xyz.tmc" entry.
 * @spinlock:	only one at a time pls.
 * @read_count:	manages preparation of buffer for reading.
 * @buf:	area of memory where trace data get sent.
 * @paddr:	DMA start location in RAM.
 * @vaddr:	virtual representation of @paddr.
 * @size:	@buf size.
 * @enable:	this TMC is being used.
 * @config_type: TMC variant, must be of type @tmc_config_type.
 * @trigger_cntr: amount of words to store after a trigger.
 * @reg_data:	MSM memory dump data to store TMC registers.
 * @buf_data:	MSM memory dump data to store ETF/ETB buffer.
 */
struct tmc_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	spinlock_t		spinlock;
	int			read_count;
	bool			reading;
	bool			aborting;
	char			*buf;
	dma_addr_t		paddr;
	void __iomem		*vaddr;
	u32			size;
	struct mutex		mem_lock;
	u32			mem_size;
	bool			enable;
	bool			sticky_enable;
	enum tmc_config_type	config_type;
	u32			trigger_cntr;
	enum tmc_etr_mem_type	mem_type;
	enum tmc_etr_mem_type	memtype;
	u32			delta_bottom;
	int			sg_blk_num;
	enum tmc_etr_out_mode	out_mode;
	struct usb_qdss_ch	*usbch;
	struct tmc_etr_bam_data	*bamdata;
	bool			enable_to_bam;
	struct msm_dump_data	reg_data;
	struct msm_dump_data	buf_data;
	struct coresight_cti	*cti_flush;
	struct coresight_cti	*cti_reset;
	char			*reg_buf;
	bool			force_reg_dump;
	bool			dump_reg;
};

static void __tmc_reg_dump(struct tmc_drvdata *drvdata);

static void tmc_wait_for_ready(struct tmc_drvdata *drvdata)
{
	/* Ensure formatter, unformatter and hardware fifo are empty */
	if (coresight_timeout(drvdata->base,
			      TMC_STS, TMC_STS_TRIGGERED_BIT, 1)) {
		dev_err(drvdata->dev,
			"timeout observed when probing at offset %#x\n",
			TMC_STS);
	}
}

static void tmc_flush_and_stop(struct tmc_drvdata *drvdata)
{
	u32 ffcr;

	ffcr = readl_relaxed(drvdata->base + TMC_FFCR);
	ffcr |= TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	ffcr |= TMC_FFCR_FLUSHMAN;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	/* Ensure flush completes */
	if (coresight_timeout(drvdata->base,
			      TMC_FFCR, TMC_FFCR_FLUSHMAN_BIT, 0)) {
		dev_err(drvdata->dev,
			"timeout observed when probing at offset %#x\n",
			TMC_FFCR);
	}

	tmc_wait_for_ready(drvdata);
}

static void tmc_enable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(TMC_CTL_CAPT_EN, drvdata->base + TMC_CTL);
}

static void tmc_disable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(0x0, drvdata->base + TMC_CTL);
}

static void tmc_etb_enable_hw(struct tmc_drvdata *drvdata)
{
	/* Zero out the memory to help with debug */
	memset(drvdata->buf, 0, drvdata->size);

	CS_UNLOCK(drvdata->base);

	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);
	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI |
		       TMC_FFCR_FON_FLIN | TMC_FFCR_FON_TRIG_EVT |
		       TMC_FFCR_TRIGON_TRIGIN,
		       drvdata->base + TMC_FFCR);

	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etr_sg_tbl_free(uint32_t *vaddr, uint32_t size, uint32_t ents)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	phys_addr_t phys_pte;
	int total_ents = DIV_ROUND_UP(size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_st_tbl = vaddr;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;

			/* Do not go beyond number of entries allocated */
			if (i == ents) {
				free_page((unsigned long)virt_st_tbl);
				return;
			}

			phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
			virt_blk = phys_to_virt(phys_pte);

			if ((last_pte - i) > 1) {
				free_page((unsigned long)virt_blk);
				pte_n++;
			} else if (last_pte == total_ents) {
				free_page((unsigned long)virt_blk);
				free_page((unsigned long)virt_st_tbl);
			} else {
				free_page((unsigned long)virt_st_tbl);
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}
			i++;
		}
	}
}

static void tmc_etr_sg_tbl_flush(uint32_t *vaddr, uint32_t size)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	phys_addr_t phys_pte;
	int total_ents = DIV_ROUND_UP(size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_st_tbl = vaddr;
	dmac_flush_range((void *)virt_st_tbl, (void *)virt_st_tbl + PAGE_SIZE);

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;
			phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
			virt_blk = phys_to_virt(phys_pte);

			dmac_flush_range(virt_blk, virt_blk + PAGE_SIZE);

			if ((last_pte - i) > 1) {
				pte_n++;
			} else if (last_pte != total_ents) {
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}
			i++;
		}
	}
}

/*
 * Scatter gather table layout in memory:
 * 1. Table contains 32-bit entries
 * 2. Each entry in the table points to 4K block of memory
 * 3. Last entry in the table points to next table
 * 4. (*) Based on mem_size requested, if there is no need for next level of
 *    table, last entry in the table points directly to 4K block of memory.
 *
 *	   sg_tbl_num=0
 *	|---------------|<-- drvdata->vaddr
 *	|   blk_num=0   |
 *	|---------------|
 *	|   blk_num=1   |
 *	|---------------|
 *	|   blk_num=2   |
 *	|---------------|	   sg_tbl_num=1
 *	|(*)Nxt Tbl Addr|------>|---------------|
 *	|---------------|       |   blk_num=3   |
 *				|---------------|
 *				|   blk_num=4   |
 *				|---------------|
 *				|   blk_num=5   |
 *				|---------------|	   sg_tbl_num=2
 *				|(*)Nxt Tbl Addr|------>|---------------|
 *				|---------------|	|   blk_num=6   |
 *							|---------------|
 *							|   blk_num=7   |
 *							|---------------|
 *							|   blk_num=8   |
 *							|---------------|
 *							|               |End of
 *							|---------------|-----
 *									 Table
 * For simplicity above diagram assumes following:
 * a. mem_size = 36KB --> total_ents = 9
 * b. ents_per_blk = 4
 */

static int tmc_etr_sg_tbl_alloc(struct tmc_drvdata *drvdata)
{
	int ret;
	uint32_t i = 0, last_pte;
	uint32_t *virt_pgdir, *virt_st_tbl;
	void *virt_pte;
	int total_ents = DIV_ROUND_UP(drvdata->size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_pgdir = (uint32_t *)get_zeroed_page(GFP_KERNEL);
	if (!virt_pgdir)
		return -ENOMEM;

	virt_st_tbl = virt_pgdir;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = (void *)get_zeroed_page(GFP_KERNEL);
			if (!virt_pte) {
				ret = -ENOMEM;
				goto err;
			}

			if ((last_pte - i) > 1) {
				*virt_st_tbl =
				     TMC_ETR_SG_ENT(virt_to_phys(virt_pte));
				virt_st_tbl++;
			} else if (last_pte == total_ents) {
				*virt_st_tbl =
				     TMC_ETR_SG_LST_ENT(virt_to_phys(virt_pte));
			} else {
				*virt_st_tbl =
				     TMC_ETR_SG_NXT_TBL(virt_to_phys(virt_pte));
				virt_st_tbl = (uint32_t *)virt_pte;
				break;
			}
			i++;
		}
	}

	drvdata->vaddr = virt_pgdir;
	drvdata->paddr = virt_to_phys(virt_pgdir);

	/* Flush the dcache before proceeding */
	tmc_etr_sg_tbl_flush((uint32_t *)drvdata->vaddr, drvdata->size);

	dev_dbg(drvdata->dev, "%s: table starts at %#lx, total entries %d\n",
		__func__, (unsigned long)drvdata->paddr, total_ents);

	return 0;
err:
	tmc_etr_sg_tbl_free(virt_pgdir, drvdata->size, i);
	return ret;
}

static void tmc_etr_sg_mem_reset(uint32_t *vaddr, uint32_t size)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	phys_addr_t phys_pte;
	int total_ents = DIV_ROUND_UP(size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_st_tbl = vaddr;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;
			phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
			virt_blk = phys_to_virt(phys_pte);

			if ((last_pte - i) > 1) {
				memset(virt_blk, 0, PAGE_SIZE);
				pte_n++;
			} else if (last_pte == total_ents) {
				memset(virt_blk, 0, PAGE_SIZE);
			} else {
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}
			i++;
		}
	}

	/* Flush the dcache before proceeding */
	tmc_etr_sg_tbl_flush(vaddr, size);
}

static int tmc_etr_alloc_mem(struct tmc_drvdata *drvdata)
{
	int ret;

	if (!drvdata->vaddr) {
		if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG) {
			drvdata->vaddr = dma_zalloc_coherent(drvdata->dev,
							     drvdata->size,
							     &drvdata->paddr,
							     GFP_KERNEL);
			if (!drvdata->vaddr) {
				ret = -ENOMEM;
				goto err;
			}
		} else {
			ret = tmc_etr_sg_tbl_alloc(drvdata);
			if (ret)
				goto err;
		}
	}
	/*
	 * Need to reinitialize buf for each tmc enable session since it is
	 * getting modified during tmc etr dump.
	 */
	drvdata->buf = drvdata->vaddr;
	return 0;
err:
	dev_err(drvdata->dev, "etr ddr memory allocation failed\n");
	return ret;
}

static void tmc_etr_free_mem(struct tmc_drvdata *drvdata)
{
	if (drvdata->vaddr) {
		if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
			dma_free_coherent(drvdata->dev, drvdata->size,
					  drvdata->vaddr, drvdata->paddr);
		else
			tmc_etr_sg_tbl_free((uint32_t *)drvdata->vaddr,
				drvdata->size,
				DIV_ROUND_UP(drvdata->size, PAGE_SIZE));

		drvdata->vaddr = 0;
		drvdata->paddr = 0;
	}
}

static void tmc_etr_mem_reset(struct tmc_drvdata *drvdata)
{
	if (drvdata->vaddr) {
		if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
			memset(drvdata->vaddr, 0, drvdata->size);
		else
			tmc_etr_sg_mem_reset((uint32_t *)drvdata->vaddr,
					     drvdata->size);
	}
}

static void tmc_etr_enable_hw(struct tmc_drvdata *drvdata)
{
	u32 axictl;

	/* Zero out the memory to help with debug */
	tmc_etr_mem_reset(drvdata);

	CS_UNLOCK(drvdata->base);

	writel_relaxed(drvdata->size / 4, drvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl |= TMC_AXICTL_WR_BURST_LEN;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
		axictl &= ~TMC_AXICTL_SCT_GAT_MODE;
	else
		axictl |= TMC_AXICTL_SCT_GAT_MODE;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	axictl = (axictl &
		  ~(TMC_AXICTL_PROT_CTL_B0 | TMC_AXICTL_PROT_CTL_B1)) |
		  TMC_AXICTL_PROT_CTL_B1;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);

	writel_relaxed(drvdata->paddr, drvdata->base + TMC_DBALO);
	writel_relaxed(((u64)drvdata->paddr >> 32) & 0xFF,
		       drvdata->base + TMC_DBAHI);
	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI |
		       TMC_FFCR_FON_FLIN | TMC_FFCR_FON_TRIG_EVT |
		       TMC_FFCR_TRIGON_TRIGIN,
		       drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etf_enable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	writel_relaxed(TMC_MODE_HARDWARE_FIFO, drvdata->base + TMC_MODE);
	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI,
		       drvdata->base + TMC_FFCR);
	writel_relaxed(0x0, drvdata->base + TMC_BUFWM);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etr_fill_usb_bam_data(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	get_qdss_bam_connection_info(&bamdata->dest,
				    &bamdata->dest_pipe_idx,
				    &bamdata->src_pipe_idx,
				    &bamdata->desc_fifo,
				    &bamdata->data_fifo,
				    NULL);
}

static void __tmc_etr_enable_to_bam(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	uint32_t axictl;

	if (drvdata->enable_to_bam)
		return;

	/* Configure and enable required CSR registers */
	msm_qdss_csr_enable_bam_to_usb();

	/* Configure and enable ETR for usb bam output */

	CS_UNLOCK(drvdata->base);

	writel_relaxed(bamdata->data_fifo.size / 4, drvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl |= (0xF << 8);
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	axictl &= ~(0x1 << 7);
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	axictl = (axictl & ~0x3) | 0x2;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);

	writel_relaxed((uint32_t)bamdata->data_fifo.phys_base,
		       drvdata->base + TMC_DBALO);
	writel_relaxed((((uint64_t)bamdata->data_fifo.phys_base) >> 32) & 0xFF,
		       drvdata->base + TMC_DBAHI);
	/* Set FOnFlIn for periodic flush */
	writel_relaxed(0x133, drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);

	drvdata->enable_to_bam = true;
}

static int tmc_etr_bam_enable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	int ret;

	if (bamdata->enable)
		return 0;

	/* Reset bam to start with */
	ret = sps_device_reset(bamdata->handle);
	if (ret)
		goto err0;

	/* Now configure and enable bam */

	bamdata->pipe = sps_alloc_endpoint();
	if (!bamdata->pipe)
		return -ENOMEM;

	ret = sps_get_config(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->connect.mode = SPS_MODE_SRC;
	bamdata->connect.source = bamdata->handle;
	bamdata->connect.event_thresh = 0x4;
	bamdata->connect.src_pipe_index = TMC_ETR_BAM_PIPE_INDEX;
	bamdata->connect.options = SPS_O_AUTO_ENABLE;

	bamdata->connect.destination = bamdata->dest;
	bamdata->connect.dest_pipe_index = bamdata->dest_pipe_idx;
	bamdata->connect.desc = bamdata->desc_fifo;
	bamdata->connect.data = bamdata->data_fifo;

	ret = sps_connect(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->enable = true;
	return 0;
err1:
	sps_free_endpoint(bamdata->pipe);
err0:
	return ret;
}

static void tmc_wait_for_flush(struct tmc_drvdata *drvdata)
{
	int count;

	/* Ensure no flush is in progress */
	for (count = TIMEOUT_US;
	     BVAL(readl_relaxed(drvdata->base + TMC_FFSR), 0) != 0
	     && count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while waiting for TMC flush, TMC_FFSR: %#x\n",
	     readl_relaxed(drvdata->base + TMC_FFSR));
}

static void __tmc_etr_disable_to_bam(struct tmc_drvdata *drvdata)
{
	if (!drvdata->enable_to_bam)
		return;

	/* Ensure periodic flush is disabled in CSR block */
	msm_qdss_csr_disable_flush();

	CS_UNLOCK(drvdata->base);

	tmc_wait_for_flush(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata);

	/* Disable CSR configuration */
	msm_qdss_csr_disable_bam_to_usb();
	drvdata->enable_to_bam = false;
}

static void tmc_etr_bam_disable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	if (!bamdata->enable)
		return;

	sps_disconnect(bamdata->pipe);
	sps_free_endpoint(bamdata->pipe);
	bamdata->enable = false;
}

static void usb_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct tmc_drvdata *drvdata = priv;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&drvdata->mem_lock);
	if (event == USB_QDSS_CONNECT) {
		tmc_etr_fill_usb_bam_data(drvdata);
		ret = tmc_etr_bam_enable(drvdata);
		if (ret)
			dev_err(drvdata->dev, "ETR BAM enable failed\n");

		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_enable_to_bam(drvdata);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	} else if (event == USB_QDSS_DISCONNECT) {
		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_disable_to_bam(drvdata);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		tmc_etr_bam_disable(drvdata);
	}
	mutex_unlock(&drvdata->mem_lock);
}

static int tmc_enable(struct tmc_drvdata *drvdata, enum tmc_mode mode)
{
	int ret;
	unsigned long flags;

	pm_runtime_get_sync(drvdata->dev);

	mutex_lock(&drvdata->mem_lock);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR &&
	    drvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		/*
		 * ETR DDR memory is not allocated until user enables
		 * tmc at least once. If user specifies different ETR
		 * DDR size than the default size or switches between
		 * contiguous or scatter-gather memory type after
		 * enabling tmc; the new selection will be honored from
		 * next tmc enable session.
		 */
		if (drvdata->size != drvdata->mem_size ||
		    drvdata->memtype != drvdata->mem_type) {
			tmc_etr_free_mem(drvdata);
			drvdata->size = drvdata->mem_size;
			drvdata->memtype = drvdata->mem_type;
		}
		ret = tmc_etr_alloc_mem(drvdata);
		if (ret) {
			pm_runtime_put(drvdata->dev);
			mutex_unlock(&drvdata->mem_lock);
			return ret;
		}
		coresight_cti_map_trigout(drvdata->cti_flush, 3, 0);
		coresight_cti_map_trigin(drvdata->cti_reset, 2, 0);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR &&
		   drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		drvdata->usbch = usb_qdss_open("qdss", drvdata,
					       usb_notifier);
		if (IS_ERR_OR_NULL(drvdata->usbch)) {
			dev_err(drvdata->dev, "usb_qdss_open failed\n");
			ret = PTR_ERR(drvdata->usbch);
			pm_runtime_put(drvdata->dev);
			mutex_unlock(&drvdata->mem_lock);
			if (!ret)
				ret = -ENODEV;

			return ret;
		}
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETB ||
		   mode == TMC_MODE_CIRCULAR_BUFFER) {
		coresight_cti_map_trigout(drvdata->cti_flush, 1, 0);
		coresight_cti_map_trigin(drvdata->cti_reset, 2, 0);
	}
	mutex_unlock(&drvdata->mem_lock);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		if (drvdata->config_type == TMC_CONFIG_TYPE_ETR
		    && drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			usb_qdss_close(drvdata->usbch);
		pm_runtime_put(drvdata->dev);

		return -EBUSY;
	}

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		tmc_etb_enable_hw(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			tmc_etr_enable_hw(drvdata);
	} else {
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			tmc_etb_enable_hw(drvdata);
		else
			tmc_etf_enable_hw(drvdata);
	}
	drvdata->enable = true;
	if (drvdata->force_reg_dump) {
		drvdata->dump_reg = true;
		__tmc_reg_dump(drvdata);
		drvdata->dump_reg = false;
	}

	/*
	 * sticky_enable prevents users from reading tmc dev node before
	 * enabling tmc at least once.
	 */
	drvdata->sticky_enable = true;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC enabled\n");
	return 0;
}

static int tmc_enable_sink(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return tmc_enable(drvdata, TMC_MODE_CIRCULAR_BUFFER);
}

static int tmc_enable_link(struct coresight_device *csdev, int inport,
			   int outport)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return tmc_enable(drvdata, TMC_MODE_HARDWARE_FIFO);
}

static void tmc_etb_dump_hw(struct tmc_drvdata *drvdata)
{
	enum tmc_mem_intf_width memwidth;
	u8 memwords;
	char *bufp;
	u32 read_data;
	int i;

	memwidth = BMVAL(readl_relaxed(drvdata->base + CORESIGHT_DEVID), 8, 10);
	if (memwidth == TMC_MEM_INTF_WIDTH_32BITS)
		memwords = 1;
	else if (memwidth == TMC_MEM_INTF_WIDTH_64BITS)
		memwords = 2;
	else if (memwidth == TMC_MEM_INTF_WIDTH_128BITS)
		memwords = 4;
	else
		memwords = 8;

	bufp = drvdata->buf;
	while (1) {
		for (i = 0; i < memwords; i++) {
			read_data = readl_relaxed(drvdata->base + TMC_RRD);
			if (read_data == 0xFFFFFFFF)
				goto out;
			memcpy(bufp, &read_data, 4);
			bufp += 4;
		}
	}

out:
	if (drvdata->aborting)
		drvdata->buf_data.magic = TMC_ETFETB_DUMP_MAGIC_V2;
}

static void tmc_etb_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	tmc_etb_dump_hw(drvdata);
	__tmc_reg_dump(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etr_sg_rwp_pos(struct tmc_drvdata *drvdata, uint32_t rwp)
{
	uint32_t i = 0, pte_n = 0, last_pte;
	uint32_t *virt_st_tbl, *virt_pte;
	void *virt_blk;
	bool found = false;
	phys_addr_t phys_pte;
	int total_ents = DIV_ROUND_UP(drvdata->size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	virt_st_tbl = drvdata->vaddr;

	while (i < total_ents) {
		last_pte = ((i + ents_per_blk) > total_ents) ?
			   total_ents : (i + ents_per_blk);
		while (i < last_pte) {
			virt_pte = virt_st_tbl + pte_n;
			phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);

			/*
			 * When the trace buffer is full; RWP could be on any
			 * 4K block from scatter gather table. Compute below -
			 * 1. Block number where RWP is currently residing
			 * 2. RWP position in that 4K block
			 * 3. Delta offset from current RWP position to end of
			 *    block.
			 */
			if (phys_pte <= rwp && rwp < (phys_pte + PAGE_SIZE)) {
				virt_blk = phys_to_virt(phys_pte);
				drvdata->sg_blk_num = i;
				drvdata->buf = virt_blk + rwp - phys_pte;
				drvdata->delta_bottom =
					phys_pte + PAGE_SIZE - rwp;
				found = true;
				break;
			}

			if ((last_pte - i) > 1) {
				pte_n++;
			} else if (i < (total_ents - 1)) {
				virt_blk = phys_to_virt(phys_pte);
				virt_st_tbl = (uint32_t *)virt_blk;
				pte_n = 0;
				break;
			}

			i++;
		}
		if (found)
			break;
	}
}

static void tmc_etr_dump_hw(struct tmc_drvdata *drvdata)
{
	u32 rwp, val;

	rwp = readl_relaxed(drvdata->base + TMC_RWP);
	val = readl_relaxed(drvdata->base + TMC_STS);

	if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG) {
		/* How much memory do we still have */
		if (val & BIT(0))
			drvdata->buf = drvdata->vaddr + rwp - drvdata->paddr;
		else
			drvdata->buf = drvdata->vaddr;
	} else {
		/*
		 * Reset these variables before computing since we
		 * rely on their values during tmc read
		 */
		drvdata->sg_blk_num = 0;
		drvdata->delta_bottom = 0;

		if (val & BIT(0))
			tmc_etr_sg_rwp_pos(drvdata, rwp);
		else
			drvdata->buf = drvdata->vaddr;
	}
}

static void tmc_etr_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	tmc_etr_dump_hw(drvdata);
	__tmc_reg_dump(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etf_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_disable(struct tmc_drvdata *drvdata, enum tmc_mode mode)
{
	unsigned long flags;

	mutex_lock(&drvdata->mem_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		tmc_etb_disable_hw(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			__tmc_etr_disable_to_bam(drvdata);
		else
			tmc_etr_disable_hw(drvdata);
	} else {
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			tmc_etb_disable_hw(drvdata);
		else
			tmc_etf_disable_hw(drvdata);
	}
out:
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR
	    && drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		tmc_etr_bam_disable(drvdata);
		usb_qdss_close(drvdata->usbch);
	} else  if (drvdata->config_type == TMC_CONFIG_TYPE_ETR
		    && drvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		coresight_cti_unmap_trigin(drvdata->cti_reset, 2, 0);
		coresight_cti_unmap_trigout(drvdata->cti_flush, 3, 0);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETB
		   || mode == TMC_MODE_CIRCULAR_BUFFER) {
		coresight_cti_unmap_trigin(drvdata->cti_reset, 2, 0);
		coresight_cti_unmap_trigout(drvdata->cti_flush, 1, 0);
	}

	pm_runtime_put(drvdata->dev);
	mutex_unlock(&drvdata->mem_lock);
	dev_info(drvdata->dev, "TMC disabled\n");
}

static void tmc_disable_sink(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	tmc_disable(drvdata, TMC_MODE_CIRCULAR_BUFFER);
}

static void tmc_disable_link(struct coresight_device *csdev, int inport,
			     int outport)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	tmc_disable(drvdata, TMC_MODE_HARDWARE_FIFO);
}

static void tmc_abort(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	enum tmc_mode mode;

	drvdata->aborting = true;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading)
		goto out0;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		tmc_etb_disable_hw(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			tmc_etr_disable_hw(drvdata);
		else if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			__tmc_etr_disable_to_bam(drvdata);
	} else {
		mode = readl_relaxed(drvdata->base + TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			tmc_etb_disable_hw(drvdata);
		else
			goto out1;
	}
out0:
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC aborted\n");
	return;
out1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
}

static const struct coresight_ops_sink tmc_sink_ops = {
	.enable		= tmc_enable_sink,
	.disable	= tmc_disable_sink,
	.abort		= tmc_abort,
};

static const struct coresight_ops_link tmc_link_ops = {
	.enable		= tmc_enable_link,
	.disable	= tmc_disable_link,
};

static const struct coresight_ops tmc_etb_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
};

static const struct coresight_ops tmc_etr_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
};

static const struct coresight_ops tmc_etf_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
	.link_ops	= &tmc_link_ops,
};

static int tmc_read_prepare(struct tmc_drvdata *drvdata)
{
	int ret;
	unsigned long flags;
	enum tmc_mode mode;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->sticky_enable) {
		dev_err(drvdata->dev, "enable tmc once before reading\n");
		ret = -EPERM;
		goto err;
	}

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR &&
	    drvdata->vaddr == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	if (!drvdata->enable)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		tmc_etb_disable_hw(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		tmc_etr_disable_hw(drvdata);
	} else {
		mode = readl_relaxed(drvdata->base + TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER) {
			tmc_etb_disable_hw(drvdata);
		} else {
			ret = -ENODEV;
			goto err;
		}
	}
out:
	drvdata->reading = true;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC read start\n");
	return 0;
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	return ret;
}

static void tmc_read_unprepare(struct tmc_drvdata *drvdata)
{
	unsigned long flags;
	enum tmc_mode mode;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->enable)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		tmc_etb_enable_hw(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		tmc_etr_enable_hw(drvdata);
	} else {
		mode = readl_relaxed(drvdata->base + TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			tmc_etb_enable_hw(drvdata);
	}
out:
	drvdata->reading = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC read end\n");
}

static int tmc_open(struct inode *inode, struct file *file)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);
	int ret = 0;

	if (drvdata->read_count++)
		goto out;

	ret = tmc_read_prepare(drvdata);
	if (ret) {
		drvdata->read_count--;
		return ret;
	}
out:
	nonseekable_open(inode, file);

	dev_dbg(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
}

/*
 * TMC read logic when scatter gather feature is enabled:
 *
 *	   sg_tbl_num=0
 *	|---------------|<-- drvdata->vaddr
 *	|   blk_num=0	|
 *	| blk_num_rel=5	|
 *	|---------------|
 *	|   blk_num=1	|
 *	| blk_num_rel=6	|
 *	|---------------|
 *	|   blk_num=2	|
 *	| blk_num_rel=7	|
 *	|---------------|	   sg_tbl_num=1
 *	|  Next Table	|------>|---------------|
 *	|  Addr		|	|   blk_num=3	|
 *	|---------------|	| blk_num_rel=8	|
 *				|---------------|
 *		  4k Block Addr	|   blk_num=4	|
 *		 |--------------| blk_num_rel=0	|
 *		 |		|---------------|
 *		 |		|   blk_num=5	|
 *		 |		| blk_num_rel=1	|
 *		 |		|---------------|	   sg_tbl_num=2
 *	 |---------------|      |  Next Table	|------>|---------------|
 *	 |		 |	|  Addr		|	|   blk_num=6	|
 *	 |		 |	|---------------|	| blk_num_rel=2 |
 *	 |    read_off	 |				|---------------|
 *	 |		 |				|   blk_num=7	|
 *	 |		 | ppos				| blk_num_rel=3	|
 *	 |---------------|-----				|---------------|
 *	 |		 |				|   blk_num=8	|
 *	 |    delta_up	 |				| blk_num_rel=4	|
 *	 |		 | RWP/drvdata->buf		|---------------|
 *	 |---------------|-----------------		|		|
 *	 |		 |   |				|		|End of
 *	 |		 |   |				|---------------|-----
 *	 |		 | drvdata->delta_bottom			 Table
 *	 |		 |   |
 *	 |_______________|  _|_
 *	      4K Block
 *
 * For simplicity above diagram assumes following:
 * a. mem_size = 36KB --> total_ents = 9
 * b. ents_per_blk = 4
 * c. RWP is on 5th block (blk_num = 5); so we have to start reading from RWP
 *    position
 */

static void tmc_etr_sg_compute_read(struct tmc_drvdata *drvdata, loff_t *ppos,
				    char **bufpp, size_t *len)
{
	uint32_t i = 0, blk_num_rel = 0, read_len = 0;
	uint32_t blk_num, sg_tbl_num, blk_num_loc, read_off;
	uint32_t *virt_pte, *virt_st_tbl;
	void *virt_blk;
	phys_addr_t phys_pte = 0;
	int total_ents = DIV_ROUND_UP(drvdata->size, PAGE_SIZE);
	int ents_per_blk = PAGE_SIZE/sizeof(uint32_t);

	/*
	 * Find relative block number from ppos and reading offset
	 * within block and find actual block number based on relative
	 * block number
	 */
	if (drvdata->buf == drvdata->vaddr) {
		blk_num = *ppos / PAGE_SIZE;
		read_off = *ppos % PAGE_SIZE;
	} else {
		if (*ppos < drvdata->delta_bottom) {
			read_off = PAGE_SIZE - drvdata->delta_bottom;
		} else {
			blk_num_rel = (*ppos / PAGE_SIZE) + 1;
			read_off = (*ppos - drvdata->delta_bottom) % PAGE_SIZE;
		}

		blk_num = (drvdata->sg_blk_num + blk_num_rel) % total_ents;
	}

	virt_st_tbl = (uint32_t *)drvdata->vaddr;

	/* Compute table index and block entry index within that table */
	if (blk_num && (blk_num == (total_ents - 1)) &&
			!(blk_num % (ents_per_blk - 1))) {
		sg_tbl_num = blk_num / ents_per_blk;
		blk_num_loc = ents_per_blk - 1;
	} else {
		sg_tbl_num = blk_num / (ents_per_blk - 1);
		blk_num_loc = blk_num % (ents_per_blk - 1);
	}

	for (i = 0; i < sg_tbl_num; i++) {
		virt_pte = virt_st_tbl + (ents_per_blk - 1);
		phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
		virt_st_tbl = (uint32_t *)phys_to_virt(phys_pte);
	}

	virt_pte = virt_st_tbl + blk_num_loc;
	phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
	virt_blk = phys_to_virt(phys_pte);

	*bufpp = virt_blk + read_off;

	if (*len > (PAGE_SIZE - read_off))
		*len = PAGE_SIZE - read_off;

	/*
	 * When buffer is wrapped around and trying to read last relative
	 * block (i.e. delta_up), compute len differently
	 */
	if (blk_num_rel && (blk_num == drvdata->sg_blk_num)) {
		read_len = PAGE_SIZE - drvdata->delta_bottom - read_off;
		if (*len > read_len)
			*len = read_len;
	}

	dev_dbg_ratelimited(drvdata->dev,
	"%s: read at %p, phys %pa len %zu blk %d, rel blk %d RWP blk %d\n",
	 __func__, *bufpp, &phys_pte, *len, blk_num, blk_num_rel,
	drvdata->sg_blk_num);
}

static ssize_t tmc_read(struct file *file, char __user *data, size_t len,
			loff_t *ppos)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);
	char *bufp = drvdata->buf + *ppos;

	if (*ppos + len > drvdata->size)
		len = drvdata->size - *ppos;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG) {
			if (bufp == (char *)(drvdata->vaddr + drvdata->size))
				bufp = drvdata->vaddr;
			else if (bufp >
				 (char *)(drvdata->vaddr + drvdata->size))
				bufp -= drvdata->size;
			if ((bufp + len) >
			    (char *)(drvdata->vaddr + drvdata->size))
				len = (char *)(drvdata->vaddr + drvdata->size)
					- bufp;
		} else
			tmc_etr_sg_compute_read(drvdata, ppos, &bufp, &len);
	}

	if (copy_to_user(data, bufp, len)) {
		dev_dbg(drvdata->dev, "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%s: %zu bytes copied, %d bytes left\n",
		__func__, len, (int)(drvdata->size - *ppos));
	return len;
}

static int tmc_release(struct inode *inode, struct file *file)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	if (--drvdata->read_count) {
		if (drvdata->read_count < 0) {
			dev_err(drvdata->dev, "mismatched close\n");
			drvdata->read_count = 0;
		}
		goto out;
	}

	tmc_read_unprepare(drvdata);
out:
	dev_dbg(drvdata->dev, "%s: released\n", __func__);
	return 0;
}

static int tmc_etr_bam_init(struct amba_device *adev,
			    struct tmc_drvdata *drvdata)
{
	int ret;
	struct device *dev = &adev->dev;
	struct resource res;
	struct tmc_etr_bam_data *bamdata;

	bamdata = devm_kzalloc(dev, sizeof(*bamdata), GFP_KERNEL);
	if (!bamdata)
		return -ENOMEM;
	drvdata->bamdata = bamdata;

	ret = of_address_to_resource(adev->dev.of_node, 1, &res);
	if (ret)
		return -ENODEV;

	bamdata->props.phys_addr = res.start;
	bamdata->props.virt_addr = devm_ioremap(dev, res.start,
						resource_size(&res));
	if (!bamdata->props.virt_addr)
		return -ENOMEM;
	bamdata->props.virt_size = resource_size(&res);

	bamdata->props.event_threshold = 0x4; /* Pipe event threshold */
	bamdata->props.summing_threshold = 0x10; /* BAM event threshold */
	bamdata->props.irq = 0;
	bamdata->props.num_pipes = TMC_ETR_BAM_NR_PIPES;

	return sps_register_bam_device(&bamdata->props, &bamdata->handle);
}

static void tmc_etr_bam_exit(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	if (!bamdata->handle)
		return;
	sps_deregister_bam_device(bamdata->handle);
}

static const struct file_operations tmc_fops = {
	.owner		= THIS_MODULE,
	.open		= tmc_open,
	.read		= tmc_read,
	.release	= tmc_release,
	.llseek		= no_llseek,
};

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	u32 tmc_rsz, tmc_sts, tmc_rrp, tmc_rwp, tmc_trg;
	u32 tmc_ctl, tmc_ffsr, tmc_ffcr, tmc_mode, tmc_pscr;
	u32 devid;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pm_runtime_get_sync(drvdata->dev);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	CS_UNLOCK(drvdata->base);

	tmc_rsz = readl_relaxed(drvdata->base + TMC_RSZ);
	tmc_sts = readl_relaxed(drvdata->base + TMC_STS);
	tmc_rrp = readl_relaxed(drvdata->base + TMC_RRP);
	tmc_rwp = readl_relaxed(drvdata->base + TMC_RWP);
	tmc_trg = readl_relaxed(drvdata->base + TMC_TRG);
	tmc_ctl = readl_relaxed(drvdata->base + TMC_CTL);
	tmc_ffsr = readl_relaxed(drvdata->base + TMC_FFSR);
	tmc_ffcr = readl_relaxed(drvdata->base + TMC_FFCR);
	tmc_mode = readl_relaxed(drvdata->base + TMC_MODE);
	tmc_pscr = readl_relaxed(drvdata->base + TMC_PSCR);
	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);

	CS_LOCK(drvdata->base);
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	pm_runtime_put(drvdata->dev);

	return sprintf(buf,
		       "Depth:\t\t0x%x\n"
		       "Status:\t\t0x%x\n"
		       "RAM read ptr:\t0x%x\n"
		       "RAM wrt ptr:\t0x%x\n"
		       "Trigger cnt:\t0x%x\n"
		       "Control:\t0x%x\n"
		       "Flush status:\t0x%x\n"
		       "Flush ctrl:\t0x%x\n"
		       "Mode:\t\t0x%x\n"
		       "PSRC:\t\t0x%x\n"
		       "DEVID:\t\t0x%x\n",
			tmc_rsz, tmc_sts, tmc_rrp, tmc_rwp, tmc_trg,
			tmc_ctl, tmc_ffsr, tmc_ffcr, tmc_mode, tmc_pscr, devid);

	return -EINVAL;
}
static DEVICE_ATTR_RO(status);

static ssize_t trigger_cntr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return sprintf(buf, "%#lx\n", val);
}

static ssize_t trigger_cntr_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR_RW(trigger_cntr);

static ssize_t mem_size_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->mem_size;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t mem_size_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	mutex_lock(&drvdata->mem_lock);
	if (kstrtoul(buf, 16, &val)) {
		mutex_unlock(&drvdata->mem_lock);
		return -EINVAL;
	}

	drvdata->mem_size = val;
	mutex_unlock(&drvdata->mem_lock);
	return size;
}
static DEVICE_ATTR_RW(mem_size);

static ssize_t mem_type_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			str_tmc_etr_mem_type[drvdata->mem_type]);
}

static ssize_t mem_type_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mem_lock);
	if (!strcmp(str, str_tmc_etr_mem_type[TMC_ETR_MEM_TYPE_CONTIG])) {
		drvdata->mem_type = TMC_ETR_MEM_TYPE_CONTIG;
	} else if (!strcmp(str, str_tmc_etr_mem_type[TMC_ETR_MEM_TYPE_SG])) {
		drvdata->mem_type = TMC_ETR_MEM_TYPE_SG;
	} else {
		mutex_unlock(&drvdata->mem_lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->mem_lock);

	return size;
}
static DEVICE_ATTR_RW(mem_type);

static ssize_t out_mode_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			str_tmc_etr_out_mode[drvdata->out_mode]);
}

static ssize_t out_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	unsigned long flags;
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mem_lock);
	if (!strcmp(str, str_tmc_etr_out_mode[TMC_ETR_OUT_MODE_MEM])) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (!drvdata->enable) {
			drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
			goto out;
		}
		__tmc_etr_disable_to_bam(drvdata);
		tmc_etr_enable_hw(drvdata);
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		coresight_cti_map_trigout(drvdata->cti_flush, 3, 0);
		coresight_cti_map_trigin(drvdata->cti_reset, 2, 0);

		tmc_etr_bam_disable(drvdata);
		usb_qdss_close(drvdata->usbch);
	} else if (!strcmp(str, str_tmc_etr_out_mode[TMC_ETR_OUT_MODE_USB])) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (!drvdata->enable) {
			drvdata->out_mode = TMC_ETR_OUT_MODE_USB;
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
			goto out;
		}
		if (drvdata->reading) {
			ret = -EBUSY;
			goto err1;
		}
		tmc_etr_disable_hw(drvdata);
		drvdata->out_mode = TMC_ETR_OUT_MODE_USB;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		coresight_cti_unmap_trigin(drvdata->cti_reset, 2, 0);
		coresight_cti_unmap_trigout(drvdata->cti_flush, 3, 0);

		drvdata->usbch = usb_qdss_open("qdss", drvdata,
					       usb_notifier);
		if (IS_ERR(drvdata->usbch)) {
			dev_err(drvdata->dev, "usb_qdss_open failed\n");
			ret = PTR_ERR(drvdata->usbch);
			goto err0;
		}
	}
out:
	mutex_unlock(&drvdata->mem_lock);
	return size;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
err0:
	mutex_unlock(&drvdata->mem_lock);
	return ret;
}
static DEVICE_ATTR_RW(out_mode);

static ssize_t available_out_modes_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(str_tmc_etr_out_mode); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s ",
				str_tmc_etr_out_mode[i]);

	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}
static DEVICE_ATTR_RO(available_out_modes);

static struct attribute *coresight_etb_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_etb);

static struct attribute *coresight_etr_attrs[] = {
	&dev_attr_available_out_modes.attr,
	&dev_attr_mem_size.attr,
	&dev_attr_mem_type.attr,
	&dev_attr_out_mode.attr,
	&dev_attr_trigger_cntr.attr,
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_etr);

static struct attribute *coresight_etf_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_etf);

static int tmc_etf_set_buf_dump(struct tmc_drvdata *drvdata)
{
	int ret;
	struct msm_dump_entry dump_entry;
	static int count;

	drvdata->buf_data.addr = virt_to_phys(drvdata->buf);
	drvdata->buf_data.len = drvdata->size;
	scnprintf(drvdata->buf_data.name, sizeof(drvdata->buf_data.name),
		"KTMC_ETF%d", count);

	dump_entry.id = MSM_DUMP_DATA_TMC_ETF + count;
	dump_entry.addr = virt_to_phys(&drvdata->buf_data);

	ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
				     &dump_entry);
	if (ret)
		return ret;

	count++;

	return 0;
}

static void __tmc_reg_dump(struct tmc_drvdata *drvdata)
{
	uint32_t *reg_buf;

	if (!drvdata->reg_buf)
		return;
	else if (!drvdata->aborting && !drvdata->dump_reg)
		return;

	drvdata->reg_data.version = TMC_REG_DUMP_VER;

	reg_buf = (uint32_t *)drvdata->reg_buf;

	reg_buf[1] = readl_relaxed(drvdata->base + TMC_RSZ);
	reg_buf[3] = readl_relaxed(drvdata->base + TMC_STS);
	reg_buf[5] = readl_relaxed(drvdata->base + TMC_RRP);
	reg_buf[6] = readl_relaxed(drvdata->base + TMC_RWP);
	reg_buf[7] = readl_relaxed(drvdata->base + TMC_TRG);
	reg_buf[8] = readl_relaxed(drvdata->base + TMC_CTL);
	reg_buf[10] = readl_relaxed(drvdata->base + TMC_MODE);
	reg_buf[11] = readl_relaxed(drvdata->base + TMC_LBUFLEVEL);
	reg_buf[12] = readl_relaxed(drvdata->base + TMC_CBUFLEVEL);
	reg_buf[13] = readl_relaxed(drvdata->base + TMC_BUFWM);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		reg_buf[14] = readl_relaxed(drvdata->base + TMC_RRPHI);
		reg_buf[15] = readl_relaxed(drvdata->base + TMC_RWPHI);
		reg_buf[68] = readl_relaxed(drvdata->base + TMC_AXICTL);
		reg_buf[70] = readl_relaxed(drvdata->base + TMC_DBALO);
		reg_buf[71] = readl_relaxed(drvdata->base + TMC_DBAHI);
	}
	reg_buf[192] = readl_relaxed(drvdata->base + TMC_FFSR);
	reg_buf[193] = readl_relaxed(drvdata->base + TMC_FFCR);
	reg_buf[194] = readl_relaxed(drvdata->base + TMC_PSCR);
	reg_buf[1000] = readl_relaxed(drvdata->base + CORESIGHT_CLAIMSET);
	reg_buf[1001] = readl_relaxed(drvdata->base + CORESIGHT_CLAIMCLR);
	reg_buf[1005] = readl_relaxed(drvdata->base + CORESIGHT_LSR);
	reg_buf[1006] = readl_relaxed(drvdata->base + CORESIGHT_AUTHSTATUS);
	reg_buf[1010] = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	reg_buf[1011] = readl_relaxed(drvdata->base + CORESIGHT_DEVTYPE);
	reg_buf[1012] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR4);
	reg_buf[1013] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR5);
	reg_buf[1014] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR6);
	reg_buf[1015] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR7);
	reg_buf[1016] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR0);
	reg_buf[1017] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR1);
	reg_buf[1018] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR2);
	reg_buf[1019] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR3);
	reg_buf[1020] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR0);
	reg_buf[1021] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR1);
	reg_buf[1022] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR2);
	reg_buf[1023] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR3);

	drvdata->reg_data.magic = TMC_REG_DUMP_MAGIC_V2;
}

static int tmc_set_reg_dump(struct tmc_drvdata *drvdata)
{
	int ret;
	struct amba_device *adev;
	struct resource *res;
	struct device *dev = drvdata->dev;
	struct msm_dump_entry dump_entry;
	uint32_t size;
	static int count;

	adev = to_amba_device(dev);
	if (!adev)
		return -EINVAL;

	res = &adev->res;
	size = resource_size(res);

	drvdata->reg_buf = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!drvdata->reg_buf)
		return -ENOMEM;

	drvdata->reg_data.addr = virt_to_phys(drvdata->reg_buf);
	drvdata->reg_data.len = size;
	scnprintf(drvdata->reg_data.name, sizeof(drvdata->reg_data.name),
		"KTMC_REG%d", count);

	dump_entry.id = MSM_DUMP_DATA_TMC_REG + count;
	dump_entry.addr = virt_to_phys(&drvdata->reg_data);

	ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
				     &dump_entry);
	/*
	 * Don't free the buffer in case of error since it can
	 * still be used to dump registers as part of abort to
	 * aid post crash parsing.
	 */
	if (ret)
		return ret;

	count++;

	return 0;
}

static int tmc_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret = 0;
	u32 devid;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct tmc_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct coresight_desc *desc;
	struct device_node *np = adev->dev.of_node;
	struct coresight_cti_data *ctidata;

	if (!np)
		return -ENODEV;

	pdata = of_get_coresight_platform_data(dev, np);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	adev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &adev->dev;
	dev_set_drvdata(dev, drvdata);

	/* Validity for the resource is already checked by the AMBA core */
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	drvdata->base = base;

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->mem_lock);

	drvdata->force_reg_dump = of_property_read_bool(np,
							"qcom,force-reg-dump");

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	drvdata->config_type = BMVAL(devid, 6, 7);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
		if (np)
			ret = of_property_read_u32(np,
						   "arm,buffer-size",
						   &drvdata->size);
		if (ret)
			drvdata->size = SZ_1M;

		drvdata->mem_size = drvdata->size;

		if (of_property_read_bool(np, "arm,sg-enable"))
			drvdata->memtype  = TMC_ETR_MEM_TYPE_SG;
		else
			drvdata->memtype  = TMC_ETR_MEM_TYPE_CONTIG;
		drvdata->mem_type = drvdata->memtype;
	} else {
		drvdata->size = readl_relaxed(drvdata->base + TMC_RSZ) * 4;
	}

	ret = clk_set_rate(adev->pclk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	pm_runtime_put(&adev->dev);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		ret = tmc_etr_bam_init(adev, drvdata);
		if (ret)
			return ret;
	} else {
		drvdata->buf = devm_kzalloc(dev, drvdata->size, GFP_KERNEL);
		if (!drvdata->buf)
			return -ENOMEM;

		ret = tmc_etf_set_buf_dump(drvdata);
		if (ret)
			dev_err(dev, "TMC ETF-ETB dump setup failed. ret: %d\n",
				ret);
	}

	ret = tmc_set_reg_dump(drvdata);
	if (ret)
		dev_err(dev, "TMC REG dump setup failed. ret: %d\n", ret);

	pdata->default_sink = of_property_read_bool(np, "arm,default-sink");

	ctidata = of_get_coresight_cti_data(dev, adev->dev.of_node);
	if (IS_ERR(ctidata)) {
		dev_err(dev, "invalid cti data\n");
	} else if (ctidata && ctidata->nr_ctis == 2) {
		drvdata->cti_flush = coresight_cti_get(
				ctidata->names[0]);
		if (IS_ERR(drvdata->cti_flush))
			dev_err(dev, "failed to get flush cti\n");

		drvdata->cti_reset = coresight_cti_get(
				ctidata->names[1]);
		if (IS_ERR(drvdata->cti_reset))
			dev_err(dev, "failed to get reset cti\n");
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->pdata = pdata;
	desc->dev = dev;
	desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		desc->type = CORESIGHT_DEV_TYPE_SINK;
		desc->ops = &tmc_etb_cs_ops;
		desc->groups = coresight_etb_groups;
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		desc->type = CORESIGHT_DEV_TYPE_SINK;
		desc->ops = &tmc_etr_cs_ops;
		desc->groups = coresight_etr_groups;
	} else {
		desc->type = CORESIGHT_DEV_TYPE_LINKSINK;
		desc->subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_FIFO;
		desc->ops = &tmc_etf_cs_ops;
		desc->groups = coresight_etf_groups;
	}

	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	drvdata->miscdev.name = pdata->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err_misc_register;

	dev_info(dev, "TMC initialized\n");
	return 0;

err_misc_register:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static int tmc_remove(struct amba_device *adev)
{
	struct tmc_drvdata *drvdata = amba_get_drvdata(adev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR)
		tmc_etr_free_mem(drvdata);
	tmc_etr_bam_exit(drvdata);

	return 0;
}

static struct amba_id tmc_ids[] = {
	{
		.id     = 0x0003b961,
		.mask   = 0x0003ffff,
	},
	{ 0, 0},
};

static struct amba_driver tmc_driver = {
	.drv = {
		.name   = "coresight-tmc",
		.owner  = THIS_MODULE,
	},
	.probe		= tmc_probe,
	.remove		= tmc_remove,
	.id_table	= tmc_ids,
};

module_amba_driver(tmc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Trace Memory Controller driver");
