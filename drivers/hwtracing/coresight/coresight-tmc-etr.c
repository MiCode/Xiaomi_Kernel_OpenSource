/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright(C) 2016 Linaro Limited. All rights reserved.
 * Author: Mathieu Poirier <mathieu.poirier@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/coresight.h>
#include <linux/dma-mapping.h>
#include "coresight-priv.h"
#include "coresight-tmc.h"

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

void tmc_etr_sg_compute_read(struct tmc_drvdata *drvdata, loff_t *ppos,
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

void tmc_etr_sg_rwp_pos(struct tmc_drvdata *drvdata, phys_addr_t rwp)
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
EXPORT_SYMBOL(tmc_etr_sg_rwp_pos);

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

void tmc_etr_enable_hw(struct tmc_drvdata *drvdata)
{
	u32 axictl, sts;

	/* Zero out the memory to help with debug */
	tmc_etr_mem_reset(drvdata);

	CS_UNLOCK(drvdata->base);

	/* Wait for TMCSReady bit to be set */
	tmc_wait_for_tmcready(drvdata);

	writel_relaxed(drvdata->size / 4, drvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl &= ~TMC_AXICTL_CLEAR_MASK;
	if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
		axictl &= ~TMC_AXICTL_SCT_GAT_MODE;
	else
		axictl |= TMC_AXICTL_SCT_GAT_MODE;
	axictl |= (TMC_AXICTL_PROT_CTL_B1 | TMC_AXICTL_WR_BURST_16);
	axictl |= TMC_AXICTL_AXCACHE_OS;

	if (tmc_etr_has_cap(drvdata, TMC_ETR_AXI_ARCACHE)) {
		axictl &= ~TMC_AXICTL_ARCACHE_MASK;
		axictl |= TMC_AXICTL_ARCACHE_OS;
	}

	axictl = (axictl &
		  ~(TMC_AXICTL_CACHE_CTL_B0 | TMC_AXICTL_CACHE_CTL_B1 |
		  TMC_AXICTL_CACHE_CTL_B2 | TMC_AXICTL_CACHE_CTL_B3)) |
		  TMC_AXICTL_CACHE_CTL_B0;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	tmc_write_dba(drvdata, drvdata->paddr);
	/*
	 * If the TMC pointers must be programmed before the session,
	 * we have to set it properly (i.e, RRP/RWP to base address and
	 * STS to "not full").
	 */
	if (tmc_etr_has_cap(drvdata, TMC_ETR_SAVE_RESTORE)) {
		tmc_write_rrp(drvdata, drvdata->paddr);
		tmc_write_rwp(drvdata, drvdata->paddr);
		sts = readl_relaxed(drvdata->base + TMC_STS) & ~TMC_STS_FULL;
		writel_relaxed(sts, drvdata->base + TMC_STS);
	}

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

static void tmc_etr_dump_hw(struct tmc_drvdata *drvdata)
{
	const u32 *barrier;
	u32 val;
	u32 *temp;
	u64 rwp;

	rwp = tmc_read_rwp(drvdata);
	val = readl_relaxed(drvdata->base + TMC_STS);

	if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG) {
		/*
		 * Adjust the buffer to point to the beginning of the trace data
		 * and update the available trace data.
		 */
		if (val & TMC_STS_FULL) {
			drvdata->buf = drvdata->vaddr + rwp - drvdata->paddr;
			drvdata->len = drvdata->size;

			barrier = barrier_pkt;
			temp = (u32 *)drvdata->buf;

			while (*barrier) {
				*temp = *barrier;
				temp++;
				barrier++;
			}

		} else {
			drvdata->buf = drvdata->vaddr;
			drvdata->len = rwp - drvdata->paddr;
		}
	} else {
		/*
		 * Reset these variables before computing since we
		 * rely on their values during tmc read
		 */
		drvdata->sg_blk_num = 0;
		drvdata->delta_bottom = 0;
		drvdata->len = drvdata->size;

		if (val & TMC_STS_FULL)
			tmc_etr_sg_rwp_pos(drvdata, rwp);
		else
			drvdata->buf = drvdata->vaddr;
	}
}

void tmc_etr_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	/*
	 * When operating in sysFS mode the content of the buffer needs to be
	 * read before the TMC is disabled.
	 */
	if (drvdata->mode == CS_MODE_SYSFS)
		tmc_etr_dump_hw(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
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

void tmc_etr_free_mem(struct tmc_drvdata *drvdata)
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

static int tmc_etr_fill_usb_bam_data(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	dma_addr_t data_fifo_iova, desc_fifo_iova;

	get_qdss_bam_connection_info(&bamdata->dest,
				    &bamdata->dest_pipe_idx,
				    &bamdata->src_pipe_idx,
				    &bamdata->desc_fifo,
				    &bamdata->data_fifo,
				    NULL);

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		data_fifo_iova = dma_map_resource(drvdata->dev,
			bamdata->data_fifo.phys_base, bamdata->data_fifo.size,
			DMA_BIDIRECTIONAL, 0);
		if (!data_fifo_iova)
			return -ENOMEM;
		dev_dbg(drvdata->dev, "%s:data p_addr:%pa,iova:%pad,size:%x\n",
			__func__, &(bamdata->data_fifo.phys_base),
			&data_fifo_iova, bamdata->data_fifo.size);
		bamdata->data_fifo.iova = data_fifo_iova;
		desc_fifo_iova = dma_map_resource(drvdata->dev,
			bamdata->desc_fifo.phys_base, bamdata->desc_fifo.size,
			DMA_BIDIRECTIONAL, 0);
		if (!desc_fifo_iova)
			return -ENOMEM;
		dev_dbg(drvdata->dev, "%s:desc p_addr:%pa,iova:%pad,size:%x\n",
			__func__, &(bamdata->desc_fifo.phys_base),
			&desc_fifo_iova, bamdata->desc_fifo.size);
		bamdata->desc_fifo.iova = desc_fifo_iova;
	}
	return 0;
}

static void __tmc_etr_enable_to_bam(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	uint32_t axictl;

	if (drvdata->enable_to_bam)
		return;

	/* Configure and enable required CSR registers */
	msm_qdss_csr_enable_bam_to_usb(drvdata->csr);

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

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		writel_relaxed((uint32_t)bamdata->data_fifo.iova,
		       drvdata->base + TMC_DBALO);
		writel_relaxed((((uint64_t)bamdata->data_fifo.iova) >> 32)
			& 0xFF, drvdata->base + TMC_DBAHI);
	} else {
		writel_relaxed((uint32_t)bamdata->data_fifo.phys_base,
		       drvdata->base + TMC_DBALO);
		writel_relaxed((((uint64_t)bamdata->data_fifo.phys_base) >> 32)
			& 0xFF, drvdata->base + TMC_DBAHI);
	}
	/* Set FOnFlIn for periodic flush */
	writel_relaxed(0x133, drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);

	drvdata->enable_to_bam = true;
}

static int get_usb_bam_iova(struct device *dev, unsigned long usb_bam_handle,
				unsigned long *iova)
{
	int ret = 0;
	phys_addr_t p_addr;
	u32 bam_size;

	ret = sps_get_bam_addr(usb_bam_handle, &p_addr, &bam_size);
	if (ret) {
		dev_err(dev, "sps_get_bam_addr failed at handle:%lx, err:%d\n",
			usb_bam_handle, ret);
		return ret;
	}
	*iova = dma_map_resource(dev, p_addr, bam_size, DMA_BIDIRECTIONAL, 0);
	if (!(*iova))
		return -ENOMEM;
	return 0;
}

static int tmc_etr_bam_enable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	unsigned long iova;
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

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		ret = get_usb_bam_iova(drvdata->dev, bamdata->dest, &iova);
		if (ret)
			goto err1;
		bamdata->connect.dest_iova = iova;
	}
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

void __tmc_etr_disable_to_bam(struct tmc_drvdata *drvdata)
{
	if (!drvdata->enable_to_bam)
		return;

	/* Ensure periodic flush is disabled in CSR block */
	msm_qdss_csr_disable_flush(drvdata->csr);

	CS_UNLOCK(drvdata->base);

	tmc_wait_for_flush(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata);

	/* Disable CSR configuration */
	msm_qdss_csr_disable_bam_to_usb(drvdata->csr);
	drvdata->enable_to_bam = false;
}

void tmc_etr_bam_disable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	if (!bamdata->enable)
		return;

	sps_disconnect(bamdata->pipe);
	sps_free_endpoint(bamdata->pipe);
	bamdata->enable = false;
}

void usb_notifier(void *priv, unsigned int event, struct qdss_request *d_req,
		  struct usb_qdss_ch *ch)
{
	struct tmc_drvdata *drvdata = priv;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&drvdata->mem_lock);
	if (event == USB_QDSS_CONNECT) {
		ret = tmc_etr_fill_usb_bam_data(drvdata);
		if (ret)
			dev_err(drvdata->dev, "ETR get usb bam data failed\n");
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

int tmc_etr_bam_init(struct amba_device *adev,
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
	if (device_property_present(dev, "iommus")
		&& !device_property_present(dev, "qcom,smmu-s1-bypass")) {
		pr_info("%s: setting SPS_BAM_SMMU_EN flag with (%s)\n",
		__func__, dev_name(dev));
		bamdata->props.options |= SPS_BAM_SMMU_EN;
	}

	return sps_register_bam_device(&bamdata->props, &bamdata->handle);
}

static int tmc_enable_etr_sink_sysfs(struct coresight_device *csdev)
{
	int ret = 0;
	bool used = false;
	unsigned long flags;
	void __iomem *vaddr = NULL;
	dma_addr_t paddr;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);


	/*
	 * If we don't have a buffer release the lock and allocate memory.
	 * Otherwise keep the lock and move along.
	 */
	mutex_lock(&drvdata->mem_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->vaddr) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
			/*
			 * ETR DDR memory is not allocated until user enables
			 * tmc at least once. If user specifies different ETR
			 * DDR size than the default size or switches between
			 * contiguous or scatter-gather memory type after
			 * enabling tmc; the new selection will be honored from
			 * next tmc enable session.
			 */
			if (drvdata->size != drvdata->mem_size) {
				tmc_etr_free_mem(drvdata);
				drvdata->size = drvdata->mem_size;
				drvdata->memtype = drvdata->mem_type;
			}

			ret = tmc_etr_alloc_mem(drvdata);
			if (ret) {
				mutex_unlock(&drvdata->mem_lock);
				return ret;
			}
			coresight_cti_map_trigout(drvdata->cti_flush, 3, 0);
			coresight_cti_map_trigin(drvdata->cti_reset, 2, 0);
		} else {
			drvdata->usbch = usb_qdss_open("qdss", drvdata,
								usb_notifier);
			if (IS_ERR_OR_NULL(drvdata->usbch)) {
				dev_err(drvdata->dev, "usb_qdss_open failed\n");
				ret = PTR_ERR(drvdata->usbch);
				mutex_unlock(&drvdata->mem_lock);
				return ret;
			}
		}
		spin_lock_irqsave(&drvdata->spinlock, flags);
	}

	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	/*
	 * In sysFS mode we can have multiple writers per sink.  Since this
	 * sink is already enabled no memory is needed and the HW need not be
	 * touched.
	 */
	if (drvdata->mode == CS_MODE_SYSFS)
		goto out;

	/*
	 * If drvdata::buf == NULL, use the memory allocated above.
	 * Otherwise a buffer still exists from a previous session, so
	 * simply use that.
	 */
	if (drvdata->buf == NULL) {
		used = true;
		drvdata->vaddr = vaddr;
		drvdata->paddr = paddr;
		drvdata->buf = drvdata->vaddr;
	}

	drvdata->mode = CS_MODE_SYSFS;
	if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
		tmc_etr_enable_hw(drvdata);

	drvdata->enable = true;
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	/* Free memory outside the spinlock if need be */
	if (!used && vaddr)
		tmc_etr_free_mem(drvdata);

	if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
		tmc_etr_byte_cntr_start(drvdata->byte_cntr);
	mutex_unlock(&drvdata->mem_lock);

	if (!ret)
		dev_info(drvdata->dev, "TMC-ETR enabled\n");

	return ret;
}

static int tmc_enable_etr_sink_perf(struct coresight_device *csdev)
{
	int ret = 0;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * In Perf mode there can be only one writer per sink.  There
	 * is also no need to continue if the ETR is already operated
	 * from sysFS.
	 */
	if (drvdata->mode != CS_MODE_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	drvdata->mode = CS_MODE_PERF;
	tmc_etr_enable_hw(drvdata);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return ret;
}

static int tmc_enable_etr_sink(struct coresight_device *csdev, u32 mode)
{
	switch (mode) {
	case CS_MODE_SYSFS:
		return tmc_enable_etr_sink_sysfs(csdev);
	case CS_MODE_PERF:
		return tmc_enable_etr_sink_perf(csdev);
	}

	/* We shouldn't be here */
	return -EINVAL;
}

static void tmc_disable_etr_sink(struct coresight_device *csdev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mutex_lock(&drvdata->mem_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		mutex_unlock(&drvdata->mem_lock);
		return;
	}

	/* Disable the TMC only if it needs to */
	if (drvdata->mode != CS_MODE_DISABLED) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
			__tmc_etr_disable_to_bam(drvdata);
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
			tmc_etr_bam_disable(drvdata);
			usb_qdss_close(drvdata->usbch);
			drvdata->mode = CS_MODE_DISABLED;
			goto out;
		} else {
			tmc_etr_disable_hw(drvdata);
		}
		drvdata->mode = CS_MODE_DISABLED;
	}

	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		coresight_cti_unmap_trigin(drvdata->cti_reset, 2, 0);
		coresight_cti_unmap_trigout(drvdata->cti_flush, 3, 0);
		tmc_etr_byte_cntr_stop(drvdata->byte_cntr);
		tmc_etr_free_mem(drvdata);
	}
out:
	mutex_unlock(&drvdata->mem_lock);
	dev_info(drvdata->dev, "TMC-ETR disabled\n");
}

static void tmc_abort_etr_sink(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading)
		goto out0;

	if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
		tmc_etr_disable_hw(drvdata);
	else if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
		__tmc_etr_disable_to_bam(drvdata);
out0:
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC aborted\n");
}

static const struct coresight_ops_sink tmc_etr_sink_ops = {
	.enable		= tmc_enable_etr_sink,
	.disable	= tmc_disable_etr_sink,
	.abort		= tmc_abort_etr_sink,
};

const struct coresight_ops tmc_etr_cs_ops = {
	.sink_ops	= &tmc_etr_sink_ops,
};

int tmc_read_prepare_etr(struct tmc_drvdata *drvdata)
{
	int ret = 0;
	unsigned long flags;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;

	mutex_lock(&drvdata->mem_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		ret = -EINVAL;
		goto out;
	}
	/* Don't interfere if operated from Perf */
	if (drvdata->mode == CS_MODE_PERF) {
		ret = -EINVAL;
		goto out;
	}

	/* If drvdata::buf is NULL the trace data has been read already */
	if (drvdata->buf == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (drvdata->byte_cntr && drvdata->byte_cntr->enable) {
		ret = -EINVAL;
		goto out;
	}

	/* Disable the TMC if need be */
	if (drvdata->mode == CS_MODE_SYSFS)
		tmc_etr_disable_hw(drvdata);

	drvdata->reading = true;
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	mutex_unlock(&drvdata->mem_lock);

	return ret;
}

int tmc_read_unprepare_etr(struct tmc_drvdata *drvdata)
{
	unsigned long flags;
	dma_addr_t paddr;
	void __iomem *vaddr = NULL;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;
	mutex_lock(&drvdata->mem_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);

	/* RE-enable the TMC if need be */
	if (drvdata->mode == CS_MODE_SYSFS) {
		/*
		 * The trace run will continue with the same allocated trace
		 * buffer. The trace buffer is cleared in tmc_etr_enable_hw(),
		 * so we don't have to explicitly clear it. Also, since the
		 * tracer is still enabled drvdata::buf can't be NULL.
		 */
		tmc_etr_enable_hw(drvdata);
	} else {
		/*
		 * The ETR is not tracing and the buffer was just read.
		 * As such prepare to free the trace buffer.
		 */
		vaddr = drvdata->vaddr;
		paddr = drvdata->paddr;
		drvdata->buf = drvdata->vaddr = NULL;
	}

	drvdata->reading = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	/* Free allocated memory out side of the spinlock */
	if (vaddr)
		tmc_etr_free_mem(drvdata);


	mutex_unlock(&drvdata->mem_lock);
	return 0;
}
