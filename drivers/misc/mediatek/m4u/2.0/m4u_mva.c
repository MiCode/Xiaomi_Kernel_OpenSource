// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include "m4u_priv.h"

/* ((va&0xfff)+size+0xfff)>>12 */
#define mva_pageOffset(mva) ((mva)&0xfff)

#define MVA_BLOCK_SIZE_ORDER     20	/* 1M */
#define MVA_MAX_BLOCK_NR        4095	/* 4GB */

#define MVA_BLOCK_SIZE      (1<<MVA_BLOCK_SIZE_ORDER)	/* 0x40000 */
#define MVA_BLOCK_ALIGN_MASK (MVA_BLOCK_SIZE-1)	/* 0x3ffff */
#define MVA_BLOCK_NR_MASK   (MVA_MAX_BLOCK_NR)	/* 0xfff */
#define MVA_BUSY_MASK       (1<<15)	/* 0x8000 */

#define MVA_IS_BUSY(index) ((mvaGraph[index]&MVA_BUSY_MASK) != 0)
#define MVA_SET_BUSY(index) (mvaGraph[index] |= MVA_BUSY_MASK)
#define MVA_SET_FREE(index) (mvaGraph[index] & (~MVA_BUSY_MASK))
#define MVA_GET_NR(index)   (mvaGraph[index] & MVA_BLOCK_NR_MASK)

#define MVAGRAPH_INDEX(mva) (mva>>MVA_BLOCK_SIZE_ORDER)

static short mvaGraph[MVA_MAX_BLOCK_NR + 1];
static void *mvaInfoGraph[MVA_MAX_BLOCK_NR + 1];
static DEFINE_SPINLOCK(gMvaGraph_lock);

void m4u_mvaGraph_init(void *priv_reserve)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	memset(mvaGraph, 0, sizeof(short) * (MVA_MAX_BLOCK_NR + 1));
	memset(mvaInfoGraph, 0, sizeof(void *) * (MVA_MAX_BLOCK_NR + 1));
	mvaGraph[0] = 1 | (short)MVA_BUSY_MASK;
	mvaInfoGraph[0] = priv_reserve;
	mvaGraph[1] = MVA_MAX_BLOCK_NR;
	mvaInfoGraph[1] = priv_reserve;
	mvaGraph[MVA_MAX_BLOCK_NR] = MVA_MAX_BLOCK_NR;
	mvaInfoGraph[MVA_MAX_BLOCK_NR] = priv_reserve;

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
}

void m4u_mvaGraph_dump_raw(void)
{
	int i;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	m4u_info("[M4U_K] dump raw data of mvaGraph:============>\n");
	for (i = 0; i < MVA_MAX_BLOCK_NR + 1; i++)
		m4u_high_info("0x%4x: 0x%08x\n", i, mvaGraph[i]);
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
}

void m4u_mvaGraph_dump(void)
{
	unsigned int addr = 0, size = 0;
	unsigned short index = 1, nr = 0;
	int i, max_bit, is_busy;
	short frag[12] = { 0 };
	short nr_free = 0, nr_alloc = 0;
	unsigned long irq_flags;

	m4u_info("[M4U_K] mva allocation info dump:====================>\n");
	m4u_info("start      size     blocknum    busy\n");

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		addr = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(index);
		size = nr << MVA_BLOCK_SIZE_ORDER;
		if (MVA_IS_BUSY(index)) {
			is_busy = 1;
			nr_alloc += nr;
		} else {		/* mva region is free */
			is_busy = 0;
			nr_free += nr;

			max_bit = 0;
			for (i = 0; i < 12; i++) {
				if (nr & (1 << i))
					max_bit = i;
			}
			frag[max_bit]++;
		}

		m4u_info("0x%08x  0x%08x  %4d    %d\n",
			 addr, size, nr, is_busy);
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	m4u_info("\n");
	m4u_info("[M4U_K] mva alloc summary: (unit: blocks)========>\n");
	m4u_info("free: %d , alloc: %d, total: %d\n",
		 nr_free, nr_alloc, nr_free + nr_alloc);
	m4u_info("[M4U_K] free region fragments in 2^x blocks unit:=====\n");
	m4u_info("  0     1     2     3     4     5     6     7     8     9     10    11\n");
	m4u_info("%4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d\n",
		 frag[0], frag[1], frag[2], frag[3], frag[4], frag[5], frag[6],
		 frag[7], frag[8], frag[9], frag[10], frag[11]);
	m4u_info("[M4U_K] mva alloc dump done=========================<\n");
}

void *mva_get_priv_ext(unsigned int mva)
{
	void *priv = NULL;
	unsigned int index;
	unsigned long irq_flags;

	index = MVAGRAPH_INDEX(mva);
	if (index == 0 || index > MVA_MAX_BLOCK_NR) {
		m4u_err("mvaGraph index is 0. mva=0x%x\n", mva);
		return NULL;
	}

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	/* find prev head/tail of this region */
	while (mvaGraph[index] == 0)
		index--;

	if (MVA_IS_BUSY(index))
		priv = mvaInfoGraph[index];

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return priv;
}

int mva_foreach_priv(mva_buf_fn_t *fn, void *data)
{
	unsigned short index = 1, nr = 0;
	unsigned int mva;
	void *priv;
	unsigned long irq_flags;
	int ret;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		mva = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(index);
		if (MVA_IS_BUSY(index)) {
			priv = mvaInfoGraph[index];
			ret = fn(priv, mva, mva + nr * MVA_BLOCK_SIZE, data);
			if (ret)
				break;
		}
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return 0;
}

unsigned int get_first_valid_mva(void)
{
	unsigned short index = 1, nr = 0;
	unsigned int mva;
	void *priv;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		mva = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(index);
		if (MVA_IS_BUSY(index)) {
			priv = mvaInfoGraph[index];
			break;
		}
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return mva;
}

void *mva_get_priv(unsigned int mva)
{
	void *priv = NULL;
	unsigned int index;
	unsigned long irq_flags;

	index = MVAGRAPH_INDEX(mva);
	if (index == 0 || index > MVA_MAX_BLOCK_NR) {
		m4u_err("mvaGraph index is 0. mva=0x%x\n", mva);
		return NULL;
	}

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	if (MVA_IS_BUSY(index))
		priv = mvaInfoGraph[index];

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return priv;
}

unsigned int m4u_do_mva_alloc(unsigned long va, unsigned int size, void *priv)
{
	unsigned short s, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;

	if (size == 0)
		return 0;

	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	/* ----------------------------------------------- */
	/* find first match free region */
	for (s = 1; (s < (MVA_MAX_BLOCK_NR + 1)) && (mvaGraph[s] < nr);
	     s += (mvaGraph[s] & MVA_BLOCK_NR_MASK))
		;
	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		m4u_err("%s: no available MVA region for %d blocks!\n",
			__func__, nr);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
				 MMPROFILE_FLAG_PULSE, size, s);
#endif

		return 0;
	}
	/* ----------------------------------------------- */
	/* alloc a mva region */
	end = s + mvaGraph[s] - 1;

	if (unlikely(nr == mvaGraph[s])) {
		MVA_SET_BUSY(s);
		MVA_SET_BUSY(end);
		mvaInfoGraph[s] = priv;
		mvaInfoGraph[end] = priv;
	} else {
		new_end = s + nr - 1;
		new_start = new_end + 1;
		/* note: new_start may equals to end */
		mvaGraph[new_start] = (mvaGraph[s] - nr);
		mvaGraph[new_end] = nr | MVA_BUSY_MASK;
		mvaGraph[s] = mvaGraph[new_end];
		mvaGraph[end] = mvaGraph[new_start];

		mvaInfoGraph[s] = priv;
		mvaInfoGraph[new_end] = priv;
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

unsigned int
m4u_do_mva_alloc_fix(unsigned long va, unsigned int mva,
		     unsigned int size, void *priv)
{
	unsigned short nr = 0;
	unsigned int startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short endIdx;
	unsigned short region_start, region_end;

	if (size == 0)
		return 0;
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		m4u_err("mvaGraph index is 0. index=0x%x\n", startIdx);
		return 0;
	}

	mva = mva | (va & M4U_PAGE_MASK);
	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = mva & (~MVA_BLOCK_ALIGN_MASK);
	endRequire = (mva + size - 1) | MVA_BLOCK_ALIGN_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	region_start = startIdx;
	/* find prev head of this region */
	while (mvaGraph[region_start] == 0)
		region_start--;

	if (MVA_IS_BUSY(region_start) ||
	    (MVA_GET_NR(region_start) < nr + startIdx - region_start)) {
		m4u_err("mva is inuse index=0x%x, mvaGraph=0x%x\n",
			region_start, mvaGraph[region_start]);
		mva = 0;
		goto out;
	}

	/* carveout startIdx~startIdx+nr-1 out of region_start */
	endIdx = startIdx + nr - 1;
	region_end = region_start + MVA_GET_NR(region_start) - 1;

	if (startIdx == region_start && endIdx == region_end) {
		MVA_SET_BUSY(startIdx);
		MVA_SET_BUSY(endIdx);
	} else if (startIdx == region_start) {
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
	} else if (endIdx == region_end) {
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
	} else {
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
	}

	mvaInfoGraph[startIdx] = priv;
	mvaInfoGraph[endIdx] = priv;

out:
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	return mva;
}

unsigned int
m4u_do_mva_alloc_start_from(unsigned long va, unsigned int mva,
			    unsigned int size, void *priv)
{
	unsigned short s = 0, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	short region_start, region_end, next_region_start = 0;

	if (size == 0)
		return 0;

	startIdx = (mva + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;

	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	m4u_mid_info("%s mva:0x%x, startIdx=%d, size = %d, nr= %d\n",
		     __func__, mva, startIdx, size, nr);

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	/* find this region */
	for (region_start = 1; (region_start < (MVA_MAX_BLOCK_NR + 1));
	     region_start += (MVA_GET_NR(region_start) & MVA_BLOCK_NR_MASK)) {

		if ((mvaGraph[region_start] & MVA_BLOCK_NR_MASK) == 0) {
			m4u_mvaGraph_dump();
			m4u_aee_err("%s: s=%d, 0x%x\n",
				    __func__, s, mvaGraph[region_start]);
		}
		if ((region_start + MVA_GET_NR(region_start)) > startIdx) {
			next_region_start = region_start +
					    MVA_GET_NR(region_start);
			break;
		}
	}

	if (region_start > MVA_MAX_BLOCK_NR) {
		m4u_err("%s:alloc mva fail,no available MVA for %d blocks\n",
			__func__, nr);
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		return 0;
	}

	region_end = region_start + MVA_GET_NR(region_start) - 1;

	if (next_region_start == 0) {
		m4u_aee_err("%s: region start-end:%d-%d, region=%d\n",
			    __func__, region_start, region_end,
			    MVA_GET_NR(region_start));
	}

	if (MVA_IS_BUSY(region_start)) {
		m4u_err("mva is inuse index=%d, mvaGraph=0x%x\n",
			region_start, mvaGraph[region_start]);
		s = region_start;
	} else {
		if ((region_end - startIdx + 1) < nr)
			s = next_region_start;
		else
			m4u_err("mva is free region_start=%d, s=%d\n",
				region_start, s);
	}

	m4u_info("region start-end:%d-%d, region= %d, next_region_start= %d, search start: %d\n",
		region_start, region_end, MVA_GET_NR(region_start),
		next_region_start, s);

	/* ----------------------------------------------- */
	if (s != 0) {
		/* find first match free region */
		for (; (s < (MVA_MAX_BLOCK_NR + 1)) && (mvaGraph[s] < nr);
				s += (mvaGraph[s] & MVA_BLOCK_NR_MASK)) {
			if ((mvaGraph[s] & MVA_BLOCK_NR_MASK) == 0) {
				m4u_aee_err("%s: s=%d, 0x%x\n",
					    __func__, s, mvaGraph[s]);
				m4u_mvaGraph_dump();
			}
		}
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		m4u_err("%s: no available MVA region for %d blocks!\n",
			__func__, nr);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
				 MMPROFILE_FLAG_PULSE, size, s);
#endif

		return 0;
	}
	/* ----------------------------------------------- */
	if (s == 0) {
		/* same as m4u_do_mva_alloc_fix */
		short endIdx = startIdx + nr - 1;

		region_end = region_start + MVA_GET_NR(region_start) - 1;
		m4u_err("region_start: %d, region_end= %d, startIdx: %d, endIdx= %d\n",
			region_start, region_end, startIdx, endIdx);

		if (startIdx == region_start && endIdx == region_end) {
			MVA_SET_BUSY(startIdx);
			MVA_SET_BUSY(endIdx);

	} else if (startIdx == region_start) {
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
	} else if (endIdx == region_end) {
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
	} else {
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
	}

	mvaInfoGraph[startIdx] = priv;
	if (endIdx > 0)
		mvaInfoGraph[endIdx] = priv;
	s = startIdx;
	} else {
		/* alloc a mva region */
		end = s + mvaGraph[s] - 1;

		if (unlikely(nr == mvaGraph[s])) {
			MVA_SET_BUSY(s);
			MVA_SET_BUSY(end);
			mvaInfoGraph[s] = priv;
			mvaInfoGraph[end] = priv;
		} else {
			new_end = s + nr - 1;
			new_start = new_end + 1;
			/* note: new_start may equals to end */
			mvaGraph[new_start] = (mvaGraph[s] - nr);
			mvaGraph[new_end] = nr | MVA_BUSY_MASK;
			mvaGraph[s] = mvaGraph[new_end];
			mvaGraph[end] = mvaGraph[new_start];

			mvaInfoGraph[s] = priv;
			mvaInfoGraph[new_end] = priv;
		}
	}
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

#define RightWrong(x) ((x) ? "correct" : "error")
int m4u_do_mva_free(unsigned int mva, unsigned int size)
{
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short nr;
	unsigned short endIdx;
	unsigned int startRequire, endRequire, sizeRequire;
	short nrRequire;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

		m4u_err("mvaGraph index is 0. mva=0x%x\n", mva);
		return -1;
	}
	nr = mvaGraph[startIdx] & MVA_BLOCK_NR_MASK;
	endIdx = startIdx + nr - 1;

	/* --------------------------------
	 * check the input arguments
	 * right condition:
	 * startIdx is not NULL && region is busy && right module && right size
	 */
	startRequire = mva & (unsigned int)(~M4U_PAGE_MASK);
	endRequire = (mva + size - 1) | (unsigned int)M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nrRequire =
		(sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	if (!(startIdx != 0 && MVA_IS_BUSY(startIdx) && (nr == nrRequire))) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		m4u_err("error to free mva========================>\n");
		m4u_err("BufSize=%d(unit:0x%xBytes) (expect %d) [%s]\n",
			nrRequire, MVA_BLOCK_SIZE, nr,
			RightWrong(nrRequire == nr));
		m4u_err("mva=0x%x, (IsBusy?)=%d (expect %d) [%s]\n",
			mva, MVA_IS_BUSY(startIdx), 1,
			RightWrong(MVA_IS_BUSY(startIdx)));
		m4u_mvaGraph_dump();
		/* m4u_mvaGraph_dump_raw(); */
		return -1;
	}

	mvaInfoGraph[startIdx] = NULL;
	mvaInfoGraph[endIdx] = NULL;

	/* -------------------------------- */
	/* merge with followed region */
	if ((endIdx + 1 <= MVA_MAX_BLOCK_NR) && (!MVA_IS_BUSY(endIdx + 1))) {
		nr += mvaGraph[endIdx + 1];
		mvaGraph[endIdx] = 0;
		mvaGraph[endIdx + 1] = 0;
	}
	/* -------------------------------- */
	/* merge with previous region */
	if ((startIdx - 1 > 0) && (!MVA_IS_BUSY(startIdx - 1))) {
		int pre_nr = mvaGraph[startIdx - 1];

		mvaGraph[startIdx] = 0;
		mvaGraph[startIdx - 1] = 0;
		startIdx -= pre_nr;
		nr += pre_nr;
	}
	/* -------------------------------- */
	/* set region flags */
	mvaGraph[startIdx] = nr;
	mvaGraph[startIdx + nr - 1] = nr;

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	return 0;
}
