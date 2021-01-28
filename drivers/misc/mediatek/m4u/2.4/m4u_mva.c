/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/spinlock.h>
#include "m4u_priv.h"
#include "m4u_mva.h"


static short mvaGraph[MVA_MAX_BLOCK_NR + 1];
static void *mvaInfoGraph[MVA_MAX_BLOCK_NR + 1];
static DEFINE_SPINLOCK(gMvaGraph_lock);

void m4u_mvaGraph_init(void *priv_reserve)
{
	int i;
	unsigned int vpu_reset_block_start =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START);
	unsigned int vpu_reset_block_end =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_END);
	unsigned int vpu_fix_block_start =
		MVAGRAPH_INDEX(VPU_FIX_MVA_START);
	unsigned int vpu_fix_block_end =
		MVAGRAPH_INDEX(VPU_FIX_MVA_END);
	unsigned int ccu_fix_block_start = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_block_end = MVAGRAPH_INDEX(CCU_FIX_MVA_END);
	unsigned int ccu_nr;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	memset(mvaGraph, 0, sizeof(short) * (MVA_MAX_BLOCK_NR + 1));
	memset(mvaInfoGraph, 0, sizeof(void *) * (MVA_MAX_BLOCK_NR + 1));
	mvaGraph[0] = (short)(1 | MVA_BUSY_MASK);
	mvaInfoGraph[0] = priv_reserve;
	/*need to reserve two mva block region for vpu.
	 *[1,0x200],[0x360,0x500): shared with all ports.
	 *[0x500, 0x501): reserved for vpu reset vector.
	 *[0x501, 0x600): shared with all ports.
	 *[0x600, 0x7E0): reserved for vpu.
	 *[0x7E0, 0xFFF]: shared with all ports.
	 */
	/*[0x1,0x200],[0x360, 0x500]*/
	mvaGraph[1] = GET_RANGE_SIZE(1, (ccu_fix_block_start - 1));
	mvaGraph[ccu_fix_block_start - 1] =
		 GET_RANGE_SIZE(1, (ccu_fix_block_start - 1));
	mvaInfoGraph[1] = priv_reserve;
	mvaInfoGraph[ccu_fix_block_start - 1] = priv_reserve;
	M4UINFO("[0x1, 0x200): start: 1 end: 0x%x mvaGraph: 0x%x\n",
		(ccu_fix_block_start - 1), mvaGraph[1]);

	/*CCU:[0x200,0x360]*/
	ccu_nr = MVA_GRAPH_BLOCK_NR_ALIGNED(
		CCU_FIX_MVA_END - CCU_FIX_MVA_START + 1);
	for (i = 0; i < ccu_nr; i++)
		MVA_SET_RESERVED(ccu_fix_block_start + i);
	mvaGraph[ccu_fix_block_start] =
		MVA_RESERVED_MASK | ccu_nr;
	mvaGraph[ccu_fix_block_end] =
		MVA_RESERVED_MASK | ccu_nr;
	mvaInfoGraph[ccu_fix_block_start] = priv_reserve;
	mvaInfoGraph[ccu_fix_block_end] = priv_reserve;
	M4UINFO("%s,start:0x%x,end:0x%x,mvaGraph:0x%x\n",
			__func__, ccu_fix_block_start, ccu_fix_block_end,
			mvaGraph[ccu_fix_block_start]);

	mvaGraph[ccu_fix_block_end + 1] =
		GET_RANGE_SIZE((ccu_fix_block_end + 1),
		(vpu_reset_block_start - 1));
	mvaGraph[vpu_reset_block_start - 1] =
		GET_RANGE_SIZE((ccu_fix_block_end + 1),
		(vpu_reset_block_start - 1));
	mvaInfoGraph[ccu_fix_block_end + 1] = priv_reserve;
	mvaInfoGraph[vpu_reset_block_start - 1] = priv_reserve;
	M4UINFO("[0x361, 0x500): start: 0x%x end: 0x%x mvaGraph: 0x%x\n",
		(ccu_fix_block_end + 1),
		(vpu_reset_block_start - 1),
		mvaGraph[ccu_fix_block_end + 1]);

	/*[0x500, 0x501)*/
	/*set 1 to each bit14 of mvaGraph in vpu reserved region.
	 *the operation can prevent allocating mva in vpu region
	 *from m4u_do_mva_alloc.
	 */
	for (i = 0; i < VPU_RESET_VECTOR_BLOCK_NR; i++)
		MVA_SET_RESERVED(vpu_reset_block_start + i);
	mvaGraph[vpu_reset_block_start] =
		MVA_RESERVED_MASK | VPU_RESET_VECTOR_BLOCK_NR;
	mvaGraph[vpu_reset_block_end] =
		MVA_RESERVED_MASK | VPU_RESET_VECTOR_BLOCK_NR;
	mvaInfoGraph[vpu_reset_block_start] = priv_reserve;
	mvaInfoGraph[vpu_reset_block_end] = priv_reserve;
	M4UINFO("[0x500, 0x501): start: 0x%x end: 0x%x mvaGraph: 0x%x\n",
		vpu_reset_block_start,
		vpu_reset_block_end,
		mvaGraph[vpu_reset_block_start]);

	/*[0x501, 0x600)*/
	mvaGraph[vpu_reset_block_end + 1] =
		GET_RANGE_SIZE((vpu_reset_block_end + 1),
		(vpu_fix_block_start - 1));
	mvaGraph[vpu_fix_block_start - 1] =
		GET_RANGE_SIZE((vpu_reset_block_end + 1),
		vpu_fix_block_start - 1);
	mvaInfoGraph[vpu_reset_block_end + 1] = priv_reserve;
	mvaInfoGraph[vpu_fix_block_start - 1] = priv_reserve;
	M4UINFO("[0x501, 0x600): start: 0x%x end: 0x%x mvaGraph: 0x%x\n",
		(vpu_reset_block_end + 1),
		(vpu_fix_block_start - 1),
		mvaGraph[vpu_reset_block_end + 1]);

	/*[0x600, 0x7E0)*/
	/*set 1 to each bit14 of mvaGraph in vpu reserved region*/
	for (i = 0; i < VPU_FIX_BLOCK_NR; i++)
		MVA_SET_RESERVED(vpu_fix_block_start + i);
	mvaGraph[vpu_fix_block_start] =
		MVA_RESERVED_MASK | VPU_FIX_BLOCK_NR;
	mvaGraph[vpu_fix_block_end] =
		MVA_RESERVED_MASK | VPU_FIX_BLOCK_NR;
	mvaInfoGraph[vpu_fix_block_start] = priv_reserve;
	mvaInfoGraph[vpu_fix_block_end] = priv_reserve;
	M4UINFO("[0x600, 0x7E0): start: 0x%x end: 0x%x mvaGraph: 0x%x\n",
		vpu_fix_block_start,
		vpu_fix_block_end,
		mvaGraph[vpu_fix_block_start]);

	/*[0x7E0, 0xFFF]*/
	mvaGraph[vpu_fix_block_end + 1] =
		GET_RANGE_SIZE((vpu_fix_block_end + 1), MVA_MAX_BLOCK_NR);
	mvaGraph[MVA_MAX_BLOCK_NR] =
		GET_RANGE_SIZE((vpu_fix_block_end + 1), MVA_MAX_BLOCK_NR);
	mvaInfoGraph[vpu_fix_block_end + 1] = priv_reserve;
	mvaInfoGraph[MVA_MAX_BLOCK_NR] = priv_reserve;
	M4UINFO("[0x7E0, 0xFFF]: start: 0x%x end: 0x%x mvaGraph: 0x%x\n",
		(vpu_fix_block_end + 1),
		MVA_MAX_BLOCK_NR,
		mvaGraph[vpu_fix_block_end + 1]);

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
}

void m4u_mvaGraph_dump_raw(void)
{
	int i;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	M4ULOG_HIGH("[M4U_K] dump raw data of mvaGraph:============>\n");
	for (i = 0; i < MVA_MAX_BLOCK_NR + 1; i++)
		M4ULOG_HIGH("0x%4x: 0x%08x\n", i, mvaGraph[i]);
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
}

int is_in_vpu_region(unsigned int index, unsigned int nr)
{
	unsigned int vpu_reset_block_start =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START);
	unsigned int vpu_reset_block_end =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_END);
	unsigned int vpu_fix_block_start =
		MVAGRAPH_INDEX(VPU_FIX_MVA_START);
	unsigned int vpu_fix_block_end =
		MVAGRAPH_INDEX(VPU_FIX_MVA_END);
	int ret = 0;
#if 0
	M4UINFO("start = 0x%x, end = 0x%x nr = %x.\n",
		index, GET_END_INDEX(index, nr), nr);
#endif
	if ((index >= vpu_reset_block_start
		&& GET_END_INDEX(index, nr) <= vpu_reset_block_end)
		|| (index >= vpu_fix_block_start
		&& GET_END_INDEX(index, nr) <= vpu_fix_block_end))
		ret = 1;
#if 0
	if (ret)
		M4UINFO("input region[0x%x - 0x%x] is in the vpu region.\n",
			index, GET_END_INDEX(index, nr));
#endif
	return ret;
}

int is_intersected_with_vpu_region(unsigned int start, unsigned int nr)
{
	unsigned int vpu_reset_block_start =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START);
	unsigned int vpu_reset_block_end =
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_END);
	unsigned int vpu_fix_block_start =
		MVAGRAPH_INDEX(VPU_FIX_MVA_START);
	unsigned int vpu_fix_block_end =
		MVAGRAPH_INDEX(VPU_FIX_MVA_END);
	int ret = 0;

	/*4 cases:
	 *   |--------------|------|---------|------------|----------|
	 *  0x0           0x500  0x501      0x600        0x800      0xFFF
	 *case 1:   start   |  end
	 *case 2:             start|   end
	 *case 3:                      start |   end
	 *case 4:                                 start   |  end
	 */
	/*case 1: 1 <= start < 0x500 && 0x500<=end<=0xFFF*/
#if 0
	M4UINFO("start = 0x%x, end = 0x%x nr = %x.\n",
		start, GET_END_INDEX(start, nr), nr);
#endif
	if ((start >= 1 && start < vpu_reset_block_start)
		&& (GET_END_INDEX(start, nr) >= vpu_reset_block_start
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;

	/*case 2: 1 <= start < 0x501 && 0x501<=end<=0xFFF*/
	if ((start >= 1 && start < (vpu_reset_block_end + 1))
		&& (GET_END_INDEX(start, nr) >= (vpu_reset_block_end + 1)
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;
	/*case 3: 1 <= start < 0x600 && 0x600<=end<=0xFFF*/
	if ((start >= 1 && start < vpu_fix_block_start)
		&& (GET_END_INDEX(start, nr) >= vpu_fix_block_start
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;
	/*case 4: 1 <=start < 0x800 && 0x800<=end<=0xFFF*/
	if ((start >= 1 && start < (vpu_fix_block_end + 1))
		&& (GET_END_INDEX(start, nr) >= (vpu_fix_block_end + 1)
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;
#if 0
	if (ret)
		M4UINFO("input region intersects to vpu region\n");
#endif
	return ret;

}

int is_in_ccu_region(unsigned int index, unsigned int nr)
{
	unsigned int ccu_fix_block_start = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_block_end = MVAGRAPH_INDEX(CCU_FIX_MVA_END);

	if (index >= ccu_fix_block_start &&
		GET_END_INDEX(index, nr) <= ccu_fix_block_end)
		return 1;

	return 0;
}

int is_intersected_with_ccu_region(unsigned int start, unsigned int nr)
{
	unsigned int ccu_fix_block_start = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_block_end = MVAGRAPH_INDEX(CCU_FIX_MVA_END);
	int ret = 0;

	M4ULOG_LOW("%s:start = 0x%x, end = 0x%x nr = %x.\n",
		__func__, start, GET_END_INDEX(start, nr), nr);

	/*case 1: 1 <= start < 0x200 && 0x300<=end<=0xFFF*/
	if ((start >= 1 && start < ccu_fix_block_start)
		&& (GET_END_INDEX(start, nr) >= ccu_fix_block_start
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;
	/*case 2: 1 <=start < 0x800 && 0x800<=end<=0xFFF*/
	if ((start >= 1 && start < (ccu_fix_block_end + 1))
		&& (GET_END_INDEX(start, nr) >= (ccu_fix_block_end + 1)
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;

	if (ret)
		M4ULOG_LOW("input region intersects to ccu region\n");

	return ret;
}
/*1: y; 0: n*/
int check_reserved_region_integrity(unsigned int start, unsigned int nr)
{
	int i, integrity = 0;

	for (i = 0; i < nr; i++) {
		if (!MVA_IS_RESERVED(start + i))
			break;
	}
	if (i == nr)
		integrity = 1;
	else {
		M4UMSG("reserved blocks[0x%x-0x%x] corruptted at 0x%x\n",
			start, GET_END_INDEX(start, nr), i);
	}
	return integrity;
}


/*need to print integrity of vpu region*/
void m4u_mvaGraph_dump(void)
{
	unsigned int start = 0, end = 0, size = 0;
	unsigned short index = 1, nr = 0;
	int i, max_bit, is_busy, is_reserve, integrity = 0;
	short frag[12] = { 0 };
	unsigned short nr_free = 0, nr_alloc = 0;

	M4ULOG_HIGH(
		"[M4U_2.4] mva allocation info dump:====================>\n");
	M4ULOG_HIGH(
		"start       end        size     blocknum    busy    reserve    integrity\n");

	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		start = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(index);
		size = nr << MVA_BLOCK_SIZE_ORDER;
		end = start + size - 1;
		/* DO NOT call aee here directly to avoid recursive dump. */
		if (nr == 0 || end <= start) {
			M4ULOG_HIGH("%s err: nr=%d, start=0x08x\n",
				    __func__, nr, start);
			break;
		}
		if (MVA_IS_BUSY(index)) {
			is_busy = 1;
			if (MVA_IS_RESERVED(index))
				is_reserve = 1;
			else
				is_reserve = 0;
			nr_alloc += nr;
		} else {		/* mva region is free */
			/* mva region is free */
			is_busy = 0;
			if (MVA_IS_RESERVED(index))
				is_reserve = 1;
			else
				is_reserve = 0;
			nr_free += nr;

			max_bit = 0;
			for (i = 0; i < 12; i++) {
				if (nr & (1 << i))
					max_bit = i;
			}
			frag[max_bit]++;
		}
		/*verify if the reserve bit of each block in vpu region is set*/
		if (is_in_ccu_region(index, nr)) {
			for (i = 0; i < nr; i++) {
				if (!MVA_IS_RESERVED(index + i))
					break;
			}
			if (i == nr)
				integrity = 1;
		} else if (is_in_vpu_region(index, nr)) {
			for (i = 0; i < nr; i++) {
				if (!MVA_IS_RESERVED(index + i))
					break;
			}
			if (i == nr)
				integrity = 1;
		}

		M4ULOG_HIGH(
			"0x%08x  0x%08x  0x%08x  %4d    %d    %4d    %8d\n",
			start, end, size, nr, is_busy, is_reserve, integrity);
		integrity = 0;
	}

	M4ULOG_HIGH("\n");
	M4ULOG_HIGH(
		"[M4U_2.4] mva alloc summary: (unit: blocks)========================>\n");
	M4ULOG_HIGH("free: %d , alloc: %d, total: %d\n",
		nr_free, nr_alloc, nr_free + nr_alloc);
	M4ULOG_HIGH(
		"[M4U_2.4] free region fragments in 2^x blocks unit:===============\n");
	M4ULOG_HIGH(
		"  0     1     2     3     4     5     6     7     8     9     10    11\n");
	M4ULOG_HIGH(
		"%4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d\n",
			frag[0], frag[1], frag[2], frag[3],
			frag[4], frag[5], frag[6],
			frag[7], frag[8], frag[9], frag[10], frag[11]);
	M4ULOG_HIGH(
		"[M4U_2.4] mva alloc dump done=========================<\n");
}

void *mva_get_priv_ext(unsigned int mva)
{
	void *priv = NULL;
	unsigned int index;
	unsigned long irq_flags;

	index = MVAGRAPH_INDEX(mva);
	if (index == 0 || index > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. mva=0x%x\n", mva);
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
	int ret;
	unsigned long irq_flags;

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
		M4UMSG("mvaGraph index is 0. mva=0x%x\n", mva);
		return NULL;
	}

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	if (MVA_IS_BUSY(index))
		priv = mvaInfoGraph[index];

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return priv;
}

/*make sure @param priv is not NULL before call this function.
 *return 0 means non-vpu region access.
 *return -1 means input mva region
 *allocated by non-vpu port is in vpu reserved region.
 *or input mva region intersectst to vpu reserved region.
 *return 1 means vpu port alloc mva in vpu reserved region.
 */
int m4u_check_mva_region(unsigned int startIdx, unsigned int nr, void *priv)
{
	struct m4u_buf_info_t *pMvaInfo = (struct m4u_buf_info_t *)priv;
	int is_in = 0, is_interseted = 0;
	int is_vpu_port = 0;
#if defined(CONFIG_MACH_MT6775) || defined(CONFIG_MACH_MT6771)
	is_vpu_port = (pMvaInfo->port == M4U_PORT_VPU0) ||
		(pMvaInfo->port == M4U_PORT_VPU1);
#else
	is_vpu_port = (pMvaInfo->port == M4U_PORT_VPU);
#endif

	/*check if input mva region is in vpu region.
	 *if it's in vpu region, we check if it's non-vpu port
	 */
#if 0
	M4UINFO("%s: [0x%x - 0x%x]\n",
		__func__, startIdx, GET_END_INDEX(startIdx, nr));
#endif
	is_in = is_in_vpu_region(startIdx, nr);
	if (is_in && is_vpu_port)
		return 1;
	else if (is_in && !is_vpu_port) {
		M4UINFO(
			"[0x%x - 0x%x] requested by port(%d) is in vpu reserved region!\n",
			startIdx, GET_END_INDEX(startIdx, nr),
			pMvaInfo->port);
		return -1;
	}

	/*check if input mva region is intersected with vpu region*/
	is_interseted = is_intersected_with_vpu_region(startIdx, nr);
	/*return 0 means other port normal alloction.
	 *if it isn't in vpu region & insersected with vpu region.
	 *it's non-vpu port alloc non-vpu reserved region. then return 0.
	 */
	if (!is_in && !is_interseted)
		return 0;
	M4UINFO(
		"[0x%x - 0x%x] requested by port(%d) intersects to vpu region!\n",
		startIdx, GET_END_INDEX(startIdx, nr),
		pMvaInfo->port);
	return -1;
}

static int __check_ccu_mva_region(
	unsigned int startIdx, unsigned int nr, void *priv)
{
	struct m4u_buf_info_t *pMvaInfo = (struct m4u_buf_info_t *)priv;
	int is_in = 0, is_interseted = 0;
	int is_ccu_port = 0;

#if defined(CONFIG_MACH_MT6771)
	is_ccu_port = (pMvaInfo->port == M4U_PORT_CCU0) ||
			(pMvaInfo->port == M4U_PORT_CCU1) ||
			(pMvaInfo->port == M4U_PORT_CAM_CCUI) ||
			(pMvaInfo->port == M4U_PORT_CAM_CCUG)
			|| (pMvaInfo->port == M4U_PORT_CAM_CCUO);
#else
	is_ccu_port = (pMvaInfo->port == M4U_PORT_CAM_CCUI) ||
	(pMvaInfo->port == M4U_PORT_CAM_CCUG)
			|| (pMvaInfo->port == M4U_PORT_CAM_CCUO);
#endif
	/*check if input mva region is in ccu region.
	 *if it's in ccu region, we check if it's non-ccu port
	 */
	M4ULOG_LOW("%s: [0x%x - 0x%x]\n",
		__func__, startIdx, GET_END_INDEX(startIdx, nr));

	is_in = is_in_ccu_region(startIdx, nr);
	if (is_in && is_ccu_port)
		return 1;
	else if (is_in && !is_ccu_port) {
		M4ULOG_MID(
			"[0x%x - 0x%x] requested by port(%d) is in ccu reserved region!\n",
			startIdx, GET_END_INDEX(startIdx, nr),
			pMvaInfo->port);
		return -1;
	}

	/*check if input mva region is intersected with vpu region*/
	is_interseted = is_intersected_with_ccu_region(startIdx, nr);
	/*return 0 means other port normal alloction.
	 *if it isn't in ccu region & insersected with ccu region.
	 *it's non-ccu port alloc non-ccu reserved region. then return 0.
	 */
	if (!is_in && !is_interseted)
		return 0;
	M4ULOG_LOW(
		"[0x%x - 0x%x] requested by port(%d) intersects to ccu region!\n",
		startIdx, GET_END_INDEX(startIdx, nr),
		pMvaInfo->port);
	return -1;
}

/*m4u_do_mva_alloc should meet the following rules:
 *(1) vpu port should not m4u_do_mva_alloc vpu fix region,
 *    but it can m4u_do_mva_alloc non-vpu fix region.
 *(2) non-vpu port can m4u_do_mva_alloc non-vpu fix region.
 */
unsigned int m4u_do_mva_alloc(unsigned long va, unsigned int size, void *priv)
{
	unsigned short s, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	int   region_status = 0;
	const short fix_index0 = MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START);
	const short fix_index1 = MVAGRAPH_INDEX(VPU_FIX_MVA_START);
	short gap_start_idx = GET_END_INDEX(fix_index0,
		VPU_RESET_VECTOR_BLOCK_NR) + 1;
	short gap_end_idx = fix_index1 - 1;
	short gap_nr = GET_RANGE_SIZE(gap_start_idx, gap_end_idx);
	const short ccu_fix_index_start = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	const short ccu_fix_index_end = MVAGRAPH_INDEX(CCU_FIX_MVA_END);
	const short ccu_gap_nr = GET_RANGE_SIZE(ccu_fix_index_end, fix_index0);
	unsigned long irq_flags;

	if (size == 0)
		return 0;

	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	if (fix_index1 == fix_index0) {
		gap_start_idx = MVAGRAPH_INDEX(VPU_FIX_MVA_START);
		gap_end_idx = GET_END_INDEX(fix_index0,
			VPU_RESET_VECTOR_BLOCK_NR) + 1;
		gap_nr = GET_RANGE_SIZE(gap_start_idx, gap_end_idx);
	}
	/* ----------------------------------------------- */
	/* find the proper mva graph on 3 stages:
	 * stage 1: find it in graph range [0x1-0x4FF ]
	 * every mva graph is neighbouring.
	 *cursor s navigate with plus the block nr in
	 * each graph from start to end.
	 *if there is one index whose graph's value is bigger
	 * than the number we need, that means we found the requeired region.
	 */
	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	/*find mva graph in range [0x1, 0x200]*/
	for (s = 1; (s < ccu_fix_index_start) && (mvaGraph[s] < nr);
		s += (mvaGraph[s] & MVA_BLOCK_NR_MASK))
		;
	/*if we didn't find the proper graph on stage 1,
	 *we will come to stage 2.
	 * in the case, all graph in [0x1-0x1FF ]
	 *is busy. jump ccu mva region.
	 */
	if (s == ccu_fix_index_start && nr <= ccu_gap_nr) {
		s = ccu_fix_index_end + 1;
		for (; (s < fix_index0) && (mvaGraph[s] < nr);
			s += (mvaGraph[s] & MVA_BLOCK_NR_MASK))
			;
	}

	if (s == fix_index0 && gap_nr >= 1) {
		/* stage 2: jump vpu reserved region
		 *and find it in graph range [0x501-0x5FF ]
		 * MUST check if block number of gap region is enough to alloc.
		 * if not, we need to alloc from common region
		 * only vinson have stage2,for vpu reserved region num is 2.
		 * so for mt6775 and mt6771 we skip stage2.
		 */
#if defined(CONFIG_MACH_MT6758)
		if (nr <= gap_nr) {
			M4ULOG_LOW(
				"stage 2: stopped cursor(%d) on stage 1\n",
				s);
			s = gap_start_idx;
			for (; (s <= gap_end_idx) && (mvaGraph[s] < nr);
				s += (mvaGraph[s] & MVA_BLOCK_NR_MASK))
				;
		} else
			goto stage3;
#else
		goto stage3;
#endif
	}
	/*if we didn't find the proper graph on stage 2,
	 *we will come to stage 3.
	 * in the case, the requeired nr may be
	 *more than gap nr.
	 * Or, all graph in grap region is busy.
	 */
	if (s == fix_index1) {
		/* stage 3: jump vpu reserved region and find it
		 *in graph range [VPU_FIX_MVA_END + 1-0xFFF ]
		 * allocate from common region directly.
		 */
stage3:
		M4ULOG_LOW("stage 3: stopped cursor(%d) on stage 2\n", s);
		/*workaround for disp fb*/
#ifdef WORKAROUND_FOR_DISPLAY_FB
		s = MVAGRAPH_INDEX(VPU_FIX_MVA_END + 1) +
			(mvaGraph[MVAGRAPH_INDEX(VPU_FIX_MVA_END + 1)] &
				MVA_BLOCK_NR_MASK);
#else
		s = MVAGRAPH_INDEX(VPU_FIX_MVA_END + 1);
#endif
		for (; (s < (MVA_MAX_BLOCK_NR + 1)) && (mvaGraph[s] < nr);
			s += (mvaGraph[s] & MVA_BLOCK_NR_MASK))
			;
	}

	/*double check if mva region we got is in vpu reserved region. */
	region_status = m4u_check_mva_region(s, nr, priv) ||
		__check_ccu_mva_region(s, nr, priv);
	if (region_status) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		M4UMSG(
			"mva_alloc error: fault cursor(0x%x) access vpu region\n",
			s);
		return 0;
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks!\n",
			nr);
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
		if (new_end > MVA_MAX_BLOCK_NR ||
			new_start > MVA_MAX_BLOCK_NR) {
			M4UMSG(
				"mva_alloc error: mva region error! nr=%u, new_end=%u, s=%u, mvaGraph=0x%x\n",
				nr, new_end, s, mvaGraph[s]);
			return 0;
		}
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

/*m4u_do_mva_alloc_fix is different with m4u_do_mva_alloc which find the free
 * and suitable region from the first one to the last.
 *Instead, m4u_do_mva_alloc_fix
 * is only used to find the fix one started from
 *formal parameter [mva] plus formal
 * parameter [va]. It's similar to mmap.
 *no matter how many [va] is, it will allocates
 * the whole fix region. it should notice the following:
 *(1) vpu fix region is protected by m4u_check_mva_region.
 *(2) if vpu port alloc vpu fix region by this, it must make sure reserved bit
 * mustn't be destroyed.
 *(3)There is no protection to non-vpu fix region now. if other fix regions are
 * taken by some ports, only m4u user can check this.
 *(4)because formal parameter [mva] ==
 *fix mva region start, va + size - 1 should be
 *not more than fix mva region end.
 *@Param va[in]    virtual address used by cpu,
 *we use it to get cpu page table offset.
 *@Param mva[in]   modified virtual start address used
 *by m4u. user must provide the right one.
 *@Param size[in]      the size of requeired sub range.
 *@Param priv[in]      mva graph info.
 */
unsigned int m4u_do_mva_alloc_fix(unsigned long va,
						unsigned int mva,
						unsigned int size,
						void *priv)
{
	unsigned short nr = 0;
	unsigned int startRequire, endRequire, sizeRequire;
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short endIdx;
	unsigned short region_start, region_end;
	int   vpu_region_status = 0, is_in_vpu_region = 0;
	int ccu_region_status, is_in_ccu_region = 0;
	unsigned long irq_flags;

	if (size == 0) {
		M4UMSG("%s: size = %d\n", __func__, size);
		return 0;
	}

	mva = mva | (va & M4U_PAGE_MASK);
	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = mva & (~MVA_BLOCK_ALIGN_MASK);
	endRequire = (mva + size - 1) | MVA_BLOCK_ALIGN_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;

	ccu_region_status = __check_ccu_mva_region(startIdx, nr, priv);
	vpu_region_status = m4u_check_mva_region(startIdx, nr, priv);
	if (ccu_region_status == -1 && vpu_region_status == -1)
		return 0;
	else if (ccu_region_status == 1)
		is_in_ccu_region = 1;
	else if (vpu_region_status == 1)
		is_in_vpu_region = 1;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	region_start = startIdx;
	/* find prev head of this region. it may be the following relation:
	 *   |-----------|-------------|----------------|--------|----------|
	 *  0x0       region_start  startIdx         endIdx  region_end    0xFFF
	 */
	while (mvaGraph[region_start] == 0)
		region_start--;

	if (MVA_IS_BUSY(region_start)
		|| (MVA_GET_NR(region_start) < nr + startIdx - region_start)) {
		M4UMSG("mva is inuse index=0x%x, mvaGraph=0x%x\n",
			region_start, mvaGraph[region_start]);
		mva = 0;
		goto out;
	}

	/* carveout startIdx~startIdx+nr-1 out of region_start */
	endIdx = startIdx + nr - 1;
	region_end = region_start + MVA_GET_NR(region_start) - 1;

	if (startIdx == region_start && endIdx == region_end) {
		/* case 1:
		 *   |-----------|-----------------------------|-------------|
		 *  0x0       region_start                  region_end    0xFFF
		 *            startIdx                      endIdx
		 *           alloc(start)                   alloc(end)
		 */
		MVA_SET_BUSY(startIdx);
		MVA_SET_BUSY(endIdx);
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
		} else if (is_in_vpu_region) {
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
		}
	} else if (startIdx == region_start) {
		/* case 2:
		 *   |-----------|-------------------|----------|-------------|
		 *  0x0       region_start         endIdx    region_end    0xFFF
		 *            startIdx
		 *           alloc(start)         alloc(end)
		 */
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
			MVA_SET_RESERVED(endIdx + 1);
			MVA_SET_RESERVED(region_end);
		} else if (is_in_vpu_region) {
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
			MVA_SET_RESERVED(endIdx + 1);
			MVA_SET_RESERVED(region_end);
		}
	} else if (endIdx == region_end) {
		/* case 3:
		 *   |-----------|------------|---------------|-------------|
		 *  0x0       region_start startIdx       region_end      0xFFF
		 *                                          endIdx
		 *                         alloc(start)    alloc(end)
		 */
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(region_start);
			MVA_SET_RESERVED(startIdx - 1);
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
		} else if (is_in_vpu_region) {
			MVA_SET_RESERVED(region_start);
			MVA_SET_RESERVED(startIdx - 1);
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
		}
	} else {
		/* case 4:
		 *   |-----------|-----|-------|--------|------|
		 *  0x0   region_start  startIdx    endIdx  region_end    0xFFF
		 *                         alloc(start)    alloc(end)
		 */
		mvaGraph[region_start] = startIdx - region_start;
		mvaGraph[startIdx - 1] = mvaGraph[region_start];
		mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[endIdx] = mvaGraph[startIdx];
		mvaGraph[endIdx + 1] = region_end - endIdx;
		mvaGraph[region_end] = mvaGraph[endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(region_start);
			MVA_SET_RESERVED(startIdx - 1);
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
			MVA_SET_RESERVED(endIdx + 1);
			MVA_SET_RESERVED(region_end);
		} else if (is_in_vpu_region) {
			MVA_SET_RESERVED(region_start);
			MVA_SET_RESERVED(startIdx - 1);
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(endIdx);
			MVA_SET_RESERVED(endIdx + 1);
			MVA_SET_RESERVED(region_end);
		}

	}

	mvaInfoGraph[startIdx] = priv;
	mvaInfoGraph[endIdx] = priv;

out:
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	return mva;
}

/*m4u_do_mva_alloc_start_from is used to allocate
 *a mva region start from iova_start to iova_end
 * for CCU & VPU. if [iova_start, iova_end] is busy,
 *it will return alloc failed.
 * for example: if [iova_start, iova_end] =
 *[0x10000000, 0xffffffff] size = 0x100000,
 * we will find a size = 0x100000 region started from
 *0x10000000 to 0xfffffff, until there is
 * a right one.
 * NOTICE: vpu should not use the api.
 *@Param va[in]    virtual address used by cpu,
 *we use it to get cpu page table offset.
 *@Param mva[in]   modified virtual start address used by
 *m4u. user must provide the right one.
 *@Param size[in]      the size of requeired sub range.
 *@Param priv[in]      mva graph info.
 */
unsigned int m4u_do_mva_alloc_start_from(unsigned long va,
							unsigned int mva,
							unsigned int size,
							void *priv)
{
	unsigned short s = 0, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	unsigned short startIdx, endIdx;
	unsigned short region_start, region_end, next_region_start = 0;
	int   vpu_region_status = 0, is_in_vpu_region = 0;
	int ccu_region_status, is_in_ccu_region = 0;
	unsigned long irq_flags;

	if (size == 0 || priv == NULL) {
		M4UMSG("%s: invalid size & port info\n", __func__);
		return 0;
	}
	/*TODO:Need to check if sub range in fix
	 *range need to align to MVA_BLOCK_ALIGN_MASK
	 */
	/* find this region[startIdx ~ endIdx].
	 *  |-----------|-|-----|----|--------|
	 *  0x0    iova_start_idx       iova_end_idx     0xFFF
	 *             startIdx         endIdx
	 */
	startIdx = (mva + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;

	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	endIdx = startIdx + nr - 1;

	/*check [startIdx, endIdx] status*/
	ccu_region_status = __check_ccu_mva_region(startIdx, nr, priv);
	vpu_region_status = m4u_check_mva_region(startIdx, nr, priv);
	if (ccu_region_status == -1 && vpu_region_status == -1)
		return 0;
	else if (ccu_region_status == 1)
		is_in_ccu_region = 1;
	else if (vpu_region_status == 1)
		is_in_vpu_region = 1;


	M4ULOG_LOW(
		"%s: iova_start_idx:0x%x, startIdx=0x%x, endIdx = 0x%x, nr= 0x%x\n",
		__func__, MVAGRAPH_INDEX(mva), startIdx, endIdx, nr);

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	/* use cursor region_start to find the region after the
	 * region including the "startIdx"th block.
	 * if we find the startIdx's neighbour and graph, we
	 *maybe need to split it.
	 *  |-----|------|-|---|--------|------|----|-----|
	 *  0x0    iova_start_idx       iova_end_idx   0xFFF
	 *             startIdx              endIdx(may be here)
	 *     region start       next region start     next region end
	 */
	for (region_start = 1; region_start < (MVA_MAX_BLOCK_NR + 1);
		region_start += MVA_GET_NR(region_start)) {
		/*error check*/
		if ((mvaGraph[region_start] & MVA_BLOCK_NR_MASK) == 0) {
			m4u_mvaGraph_dump();
			m4u_aee_print("%s: s=%d, 0x%x\n",
				__func__, s, mvaGraph[region_start]);
		}
		if ((region_start + MVA_GET_NR(region_start)) > startIdx) {
			next_region_start =
				region_start + MVA_GET_NR(region_start);
			break;
		}
	}
	if (region_start > MVA_MAX_BLOCK_NR) {
		M4UMSG(
			"%s:alloc mva fail,no available MVA for %d blocks\n",
			__func__, nr);
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		return 0;
	}
	region_end = region_start + MVA_GET_NR(region_start) - 1;
	M4ULOG_LOW(
		"%s: found region_start(0x%x) region_end(0x%x) next_region_start(0x%x)\n",
		__func__, region_start, region_end, next_region_start);

	/*if not found, it means error.*/
	if (next_region_start == 0) {
		M4UMSG("no enough mva to allocate.\n");
		m4u_aee_print(
			"%s: region_start: %d, region_end= %d, region block count= %d\n",
			__func__, region_start,
			region_end, MVA_GET_NR(region_start));
	}

	/*check the found region status.*/
	if (MVA_IS_BUSY(region_start)) {
		/*if [region_start, region_end] is busy, need to
		 *use another cursor s to continue.
		 *  |-----|------|-----|---|-----|--|---|
		 *  0x0    iova_start_idx    iova_end_idx   0xFFF
		 *           startIdx      endIdx(may be here)
		 *    region start   next region start    next region end
		 *        s
		 */
		s = region_start;
		M4ULOG_LOW(
			"found region is busy. need to traverse again from s=0x%x.\n",
			s);
	} else {
		/*if [region_start, region_end] is free, need to check
		 *whether the nr of [region_start, region_end] is
		 *enough to alloc.
		 *if enough, alloc it. if not, need to continue with "s =
		 *next_region_start".
		 *  |-----|---|-----|---|------|----|-----|
		 *  0x0    iova_start_idx           iova_end_idx   0xFFF
		 *           startIdx                endIdx(may be here)
		 *    region start       next region start    next region end
		 *                             s
		 */
		if ((region_end - startIdx + 1) < nr) {
			s = next_region_start;
			M4UINFO(
				"the size of found region is not enough. need to traverse again from s=0x%x\n",
				s);
		} else
			M4UINFO(
			"found region is free. region_start=%d, s=%d\n",
				region_start, s);
	}

	/* now [region_start, region_end] is not free or enough to allocate.
	 * so traverse mvaGraph to find the first right one.
	 * here we should also need to traverse mvaGraph.
	 */
	if (s != 0) {
		/* find first match free region */
		for (; s < (MVA_MAX_BLOCK_NR + 1);
			s += (mvaGraph[s] & MVA_BLOCK_NR_MASK)) {
			/*error check*/
			if ((mvaGraph[s] & MVA_BLOCK_NR_MASK) == 0) {
				m4u_aee_print("%s: s=%d, 0x%x\n",
					__func__, s, mvaGraph[s]);
				m4u_mvaGraph_dump();
			}

			if (MVA_GET_NR(s) > nr && !MVA_IS_BUSY(s)) {
				/*check [s, s + MVA_GET_NR(s) -1] status*/
				ccu_region_status = __check_ccu_mva_region(
					s, MVA_GET_NR(s), priv);
				vpu_region_status = m4u_check_mva_region(
					s, MVA_GET_NR(s), priv);
				if (ccu_region_status == -1 &&
					vpu_region_status == -1)
					continue;
				else if (ccu_region_status == 1) {
					is_in_ccu_region = 1;
					break;
				} else if (vpu_region_status == 1) {
					is_in_vpu_region = 1;
					break;
				} else if (ccu_region_status == 0 &&
					vpu_region_status == 0)
					break;
			}
		}
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks!\n",
			nr);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
		MMPROFILE_FLAG_PULSE, size, s);
#endif

		return 0;
	}

	if (s != 0)
		M4ULOG_MID("after 2nd traverse, found region s = 0x%x\n", s);

	/* ----------------------------------------------- */
	/*s==0 means startIdx == mva_start >> 20*/
	if (s == 0) {
		/*[region_start, region_end] is free or enough to allocate.
		 *it's same to m4u_do_mva_alloc_fix
		 */
		if (startIdx == region_start && endIdx == region_end) {
			MVA_SET_BUSY(startIdx);
			MVA_SET_BUSY(endIdx);
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
			}

		} else if (startIdx == region_start) {
			mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
			mvaGraph[endIdx] = mvaGraph[startIdx];
			mvaGraph[endIdx + 1] = region_end - endIdx;
			mvaGraph[region_end] = mvaGraph[endIdx + 1];
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
				MVA_SET_RESERVED(endIdx + 1);
				MVA_SET_RESERVED(region_end);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
				MVA_SET_RESERVED(endIdx + 1);
				MVA_SET_RESERVED(region_end);
			}
		} else if (endIdx == region_end) {
			mvaGraph[region_start] = startIdx - region_start;
			mvaGraph[startIdx - 1] = mvaGraph[region_start];
			mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
			mvaGraph[endIdx] = mvaGraph[startIdx];
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(region_start);
				MVA_SET_RESERVED(startIdx - 1);
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(region_start);
				MVA_SET_RESERVED(startIdx - 1);
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
			}

		} else {
			mvaGraph[region_start] = startIdx - region_start;
			mvaGraph[startIdx - 1] = mvaGraph[region_start];
			mvaGraph[startIdx] = nr | MVA_BUSY_MASK;
			mvaGraph[endIdx] = mvaGraph[startIdx];
			mvaGraph[endIdx + 1] = region_end - endIdx;
			mvaGraph[region_end] = mvaGraph[endIdx + 1];
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(region_start);
				MVA_SET_RESERVED(startIdx - 1);
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
				MVA_SET_RESERVED(endIdx + 1);
				MVA_SET_RESERVED(region_end);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(region_start);
				MVA_SET_RESERVED(startIdx - 1);
				MVA_SET_RESERVED(startIdx);
				MVA_SET_RESERVED(endIdx);
				MVA_SET_RESERVED(endIdx + 1);
				MVA_SET_RESERVED(region_end);
			}

		}

		mvaInfoGraph[startIdx] = priv;
		mvaInfoGraph[endIdx] = priv;
		s = startIdx;
	} else {
		/* alloc a mva region */
		end = s + MVA_GET_NR(s) - 1;

		/*check [startIdx, endIdx] status*/
		is_in_ccu_region = 0;
		is_in_vpu_region = 0;
		ccu_region_status = __check_ccu_mva_region(s, nr, priv);
		if (ccu_region_status == 1)
			is_in_ccu_region = 1;
		vpu_region_status = m4u_check_mva_region(s,
			MVA_GET_NR(s), priv);
		if (vpu_region_status == 1)
			is_in_vpu_region = 1;

		if (unlikely(nr == MVA_GET_NR(s))) {
			MVA_SET_BUSY(s);
			MVA_SET_BUSY(end);
			mvaInfoGraph[s] = priv;
			mvaInfoGraph[end] = priv;
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(s);
				MVA_SET_RESERVED(end);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(s);
				MVA_SET_RESERVED(end);
			}
		} else {
			new_end = s + nr - 1;
			new_start = new_end + 1;
			/* note: new_start may equals to end */
			mvaGraph[new_start] = (MVA_GET_NR(s) - nr);
			mvaGraph[new_end] = nr | MVA_BUSY_MASK;
			mvaGraph[s] = mvaGraph[new_end];
			mvaGraph[end] = mvaGraph[new_start];
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(new_start);
				MVA_SET_RESERVED(new_end);
				MVA_SET_RESERVED(s);
				MVA_SET_RESERVED(end);
			} else if (is_in_vpu_region) {
				MVA_SET_RESERVED(new_start);
				MVA_SET_RESERVED(new_end);
				MVA_SET_RESERVED(s);
				MVA_SET_RESERVED(end);
			}

			mvaInfoGraph[s] = priv;
			mvaInfoGraph[new_end] = priv;
		}
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

/*m4u_do_mva_free needs to care if non-vpu port wants to free vpu fix region.*/
#define RightWrong(x) ((x) ? "correct" : "error")
int m4u_do_mva_free(unsigned int mva, unsigned int size)
{
	unsigned short startIdx;
	unsigned short nr;
	unsigned short endIdx;
	unsigned int startRequire, endRequire, sizeRequire;
	unsigned short nrRequire, nr_tmp = 0;
	int   vpu_region_status, is_in_vpu_region_flag = 0;
	int   ccu_region_status, is_in_ccu_region_flag = 0;
	int ret = 0;
	struct m4u_buf_info_t *p_mva_info;
	int port;
	unsigned long irq_flags;

	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);

	startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. mva=0x%x\n", mva);
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		return -1;
	}
	nr = mvaGraph[startIdx] & MVA_BLOCK_NR_MASK;
	endIdx = startIdx + nr - 1;
	p_mva_info = (struct m4u_buf_info_t *)mvaInfoGraph[startIdx];

	if (size == 0 || nr == 0 || p_mva_info == NULL) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		M4UMSG("%s error: the input size = %d nr = %d mva_info = %p.\n",
			__func__, size, nr, p_mva_info);
		if (p_mva_info)
			M4UMSG("error port id = %d\n", p_mva_info->port);
		m4u_mvaGraph_dump();
		return -1;
	}
	port = p_mva_info->port;

	/*check if reserved region meets free condition.*/
	ccu_region_status = __check_ccu_mva_region(
		startIdx, nr, (void *)p_mva_info);
	vpu_region_status = m4u_check_mva_region(
		startIdx, nr, (void *)p_mva_info);
	if (ccu_region_status == -1 && vpu_region_status == -1) {
		spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
		return -1;
	} else if (ccu_region_status == 1)
		is_in_ccu_region_flag = 1;
	else if (vpu_region_status == 1)
		is_in_vpu_region_flag = 1;

	/* -------------------------------- */
	/* check the input arguments */
	/* right condition: startIdx is not NULL &&
	 * region is busy && right module && right size
	 */
	startRequire = mva & (unsigned int)(~M4U_PAGE_MASK);
	endRequire = (mva + size - 1) | (unsigned int)M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nrRequire = (sizeRequire + MVA_BLOCK_ALIGN_MASK)
		>> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */
	if (!(startIdx != 0	/* startIdx is not NULL */
		&& MVA_IS_BUSY(startIdx)
		&& (nr == nrRequire))) {
		spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
		M4UMSG("error to free mva========================>\n");
		M4UMSG("BufSize=%d(unit:0x%xBytes) (expect %d) [%s]\n",
		       nrRequire, MVA_BLOCK_SIZE, nr,
		       RightWrong(nrRequire == nr));
		M4UMSG("mva=0x%x, (IsBusy?)=%d (expect %d) [%s]\n",
		       mva, MVA_IS_BUSY(startIdx), 1,
		       RightWrong(MVA_IS_BUSY(startIdx)));
		m4u_mvaGraph_dump();
		/* m4u_mvaGraph_dump_raw(); */
		return -1;
	}

	mvaInfoGraph[startIdx] = NULL;
	mvaInfoGraph[endIdx] = NULL;

	/*now do the mva deallocation. if mva to be freed is in vpu fix region,
	 *we should set reserved bit besides merging mva graph. if not in vpu
	 *fix region, we should make sure reserved bit
	 * is protected from destroying.
	 */
	if (is_in_ccu_region_flag || is_in_vpu_region_flag) {
		/* merge with followed region.
		 * if endIdx + 1 is in vpu region, do the merge as before.
		 * or, since we have set the reserved bit when alloc,
		 * it's ok to do nothing.
		 */
		nr_tmp = MVA_GET_NR(endIdx + 1);
		if (is_in_ccu_region(endIdx + 1, nr_tmp) &&
			!MVA_IS_BUSY(endIdx + 1)) {
			nr += nr_tmp;
			mvaGraph[endIdx] = 0;
			mvaGraph[endIdx + 1] = 0;
			MVA_SET_RESERVED(endIdx);
			MVA_SET_RESERVED(endIdx + 1);
		}
		/* merge with previous region
		 * same operation as merging with followed region
		 */
		nr_tmp = MVA_GET_NR(startIdx - 1);
		if (is_in_ccu_region(
			GET_START_INDEX((startIdx - 1), nr_tmp), nr_tmp)
			&& (!MVA_IS_BUSY(startIdx - 1))) {
			int pre_nr = nr_tmp;

			mvaGraph[startIdx] = 0;
			mvaGraph[startIdx - 1] = 0;
			MVA_SET_RESERVED(startIdx);
			MVA_SET_RESERVED(startIdx - 1);
			startIdx -= pre_nr;
			nr += pre_nr;
		}
		/* -------------------------------- */
		/* set region flags */
		mvaGraph[startIdx] = nr;
		mvaGraph[startIdx + nr - 1] = nr;
		MVA_SET_RESERVED(startIdx);
		MVA_SET_RESERVED(startIdx + nr - 1);
	} else {
		/* merge with followed region.*/
		nr_tmp = MVA_GET_NR(endIdx + 1);
		if ((endIdx + 1 <= MVA_MAX_BLOCK_NR) &&
			(!MVA_IS_BUSY(endIdx + 1))) {
			/* check if the followed region is in vpu region.
			 * yes -> do nothing
			 * no  -> do the merging
			 */
			if (!MVA_IS_RESERVED(endIdx + 1)) {
				nr += nr_tmp;
				mvaGraph[endIdx] = 0;
				mvaGraph[endIdx + 1] = 0;
			}
		}

		/* merge with previous region
		 * same operation as merging with followed region
		 */
		nr_tmp = MVA_GET_NR(startIdx - 1);
		if ((startIdx - 1) > 0 && (!MVA_IS_BUSY(startIdx - 1))) {
			if (!MVA_IS_RESERVED(startIdx - 1)) {
				int pre_nr = nr_tmp;

				mvaGraph[startIdx] = 0;
				mvaGraph[startIdx - 1] = 0;
				startIdx -= pre_nr;
				nr += pre_nr;
			}
		}
		/* -------------------------------- */
		/* set region flags */
		mvaGraph[startIdx] = nr;
		mvaGraph[startIdx + nr - 1] = nr;
	}

	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);

	/*for debug*/
	ret = check_reserved_region_integrity(
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START),
						VPU_RESET_VECTOR_BLOCK_NR);
	if (!ret)
		M4UMSG(
		"VPU_RESET_VECTOR region is corruptted when port(%d) free mva(0x%x)\n",
		port, mva);

	ret = check_reserved_region_integrity(MVAGRAPH_INDEX(VPU_FIX_MVA_START),
						VPU_FIX_BLOCK_NR);
	if (!ret)
		M4UMSG(
		"VPU reserved data region is corruptted when port(%d) free mva(0x%x)\n",
		port, mva);

	return 0;
}

unsigned int get_last_free_graph_idx_in_stage1_region(void)
{
	unsigned int index, nr;
	unsigned long irq_flags;

	index = MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START) - 1;
	spin_lock_irqsave(&gMvaGraph_lock, irq_flags);
	nr = MVA_GET_NR(index);
	index = GET_START_INDEX(index, nr);
	spin_unlock_irqrestore(&gMvaGraph_lock, irq_flags);
	return index;
}

unsigned int get_first_free_idx(void)
{
	int first_valid_mva, first_free_idx;

	first_valid_mva = get_first_valid_mva();
	first_free_idx = MVAGRAPH_INDEX(first_valid_mva) +
		MVA_GET_NR(first_valid_mva);
	return first_free_idx;
}
