// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#define MVA_RESERVED_MASK       (1<<14)	/* 0x4000 */

#define MVA_IS_BUSY(domain_idx, index) \
		((mvaGraph[domain_idx][index]&MVA_BUSY_MASK) != 0)
#define MVA_SET_BUSY(domain_idx, index) \
		(mvaGraph[domain_idx][index] |= MVA_BUSY_MASK)
#define MVA_SET_FREE(domain_idx, index) \
		(mvaGraph[domain_idx][index] & (~MVA_BUSY_MASK))
#define MVA_GET_NR(domain_idx, index)   \
		(mvaGraph[domain_idx][index] & MVA_BLOCK_NR_MASK)

/*the macro is only use for vpu*/
#define MVA_IS_RESERVED(domain, index) \
	((mvaGraph[domain][index] & MVA_RESERVED_MASK) != 0)
#define MVA_SET_RESERVED(domain, index) \
	(mvaGraph[domain][index] |= MVA_RESERVED_MASK)

/*translate mva to mvaGraph index which mva belongs to*/
#define MVAGRAPH_INDEX(mva) ((mva) >> MVA_BLOCK_SIZE_ORDER)
#define GET_START_INDEX(end, nr) (end - nr + 1)
#define GET_END_INDEX(start, nr) (start + nr - 1)
#define GET_RANGE_SIZE(start, end) (end - start + 1)

/*calculate requeired block number with input mva*/
#define START_ALIGNED(mva) (mva & (~MVA_BLOCK_ALIGN_MASK))
#define END_ALIGNED(mva, nr) (GET_END_INDEX(mva, nr) | MVA_BLOCK_ALIGN_MASK)
#define MVA_GRAPH_BLOCK_NR_ALIGNED(size) \
	((size + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER)

#define MVA_GRAPH_NR_TO_SIZE(nr) (nr << MVA_BLOCK_SIZE_ORDER)

/*reserved mva region for vpu exclusive use*/
#if defined(CONFIG_MACH_MT6775) || \
	defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6779)
#define VPU_RESET_VECTOR_FIX_MVA_START   0x7DA00000
#define VPU_RESET_VECTOR_FIX_MVA_END     (0x82600000 - 1)
#else
#define VPU_RESET_VECTOR_FIX_MVA_START   0x50000000
#define VPU_RESET_VECTOR_FIX_MVA_END     0x5007FFFF
#endif
#define VPU_RESET_VECTOR_FIX_SIZE        \
	(VPU_RESET_VECTOR_FIX_MVA_END - VPU_RESET_VECTOR_FIX_MVA_START + 1)
#define VPU_RESET_VECTOR_BLOCK_NR        \
	MVA_GRAPH_BLOCK_NR_ALIGNED(VPU_RESET_VECTOR_FIX_SIZE)

#if defined(CONFIG_MACH_MT6775) || \
	defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6779)
#define VPU_FIX_MVA_START                0x7DA00000
#define VPU_FIX_MVA_END                  (0x82600000 - 1)
#else
#define VPU_FIX_MVA_START                0x60000000
#define VPU_FIX_MVA_END                  0x7CDFFFFF
#endif
#define VPU_FIX_MVA_SIZE                 \
	(VPU_FIX_MVA_END - VPU_FIX_MVA_START + 1)
#define VPU_FIX_BLOCK_NR                 \
	MVA_GRAPH_BLOCK_NR_ALIGNED(VPU_FIX_MVA_SIZE)

/*reserved ccu mva region*/
#define CCU_FIX_MVA_START			0x40000000
#define CCU_FIX_MVA_END				0x48000000

#define CCU_FIX_MVA_SIZE				\
	(CCU_FIX_MVA_END - CCU_FIX_MVA_START + 1)
#define CCU_FIX_BLOCK_NR                \
	MVA_GRAPH_BLOCK_NR_ALIGNED(CCU_FIX_MVA_SIZE)


#define MVA_COMMON_CONTIG_RETGION_START          0x80000000

static short mvaGraph[TOTAL_M4U_NUM][MVA_MAX_BLOCK_NR + 1];
static void *mvaInfoGraph[TOTAL_M4U_NUM][MVA_MAX_BLOCK_NR + 1];
/*just be used for single spinlock lock 2 graph*/
static DEFINE_SPINLOCK(gMvaGraph_lock);
/*be used for m4uGraph0*/
static DEFINE_SPINLOCK(gMvaGraph_lock0);
/*be used for m4uGraph1*/
static DEFINE_SPINLOCK(gMvaGraph_lock1);
enum graph_lock_tpye {
	SPINLOCK_MVA_GRAPH0,
	SPINLOCK_MVA_GRAPH1,
	SPINLOCK_COMMON,
	SPINLOCK_INVAILD
};

/*according to lock type, get mva graph lock.*/
static spinlock_t *get_mva_graph_lock(enum graph_lock_tpye type)
{
	switch (type) {
	case SPINLOCK_MVA_GRAPH0:
		return &gMvaGraph_lock0;
	case SPINLOCK_MVA_GRAPH1:
		return &gMvaGraph_lock1;
	case SPINLOCK_COMMON:
		return &gMvaGraph_lock;
	default:
		M4UMSG(
			"fatal error: invalid mva graph lock type(%d)!\n",
				(int)type);
		return NULL;
	}
}

void m4u_mvaGraph_init(void *priv_reserve, int domain_idx)
{
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	int i;
	//ccu_fix_blk_s: ccu_fix_block_start
	unsigned int ccu_fix_blk_s = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_blk_e = MVAGRAPH_INDEX(CCU_FIX_MVA_END);
	unsigned int ccu_nr;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	spin_lock_irqsave(mva_graph_lock, irq_flags);
	memset(&mvaGraph[domain_idx], 0,
			sizeof(short) * (MVA_MAX_BLOCK_NR + 1));
	memset(mvaInfoGraph[domain_idx], 0,
			sizeof(void *) * (MVA_MAX_BLOCK_NR + 1));
	mvaGraph[domain_idx][0] = (short)(1 | MVA_BUSY_MASK);
	mvaInfoGraph[domain_idx][0] = priv_reserve;
	mvaGraph[domain_idx][1] = MVA_MAX_BLOCK_NR;
	mvaInfoGraph[domain_idx][1] = priv_reserve;

	if (domain_idx == 0) {
		/* free:[1,1023] */
		mvaGraph[domain_idx][1] = (short)(GET_RANGE_SIZE(1,
			(ccu_fix_blk_s - 1)));
		mvaGraph[domain_idx][ccu_fix_blk_s - 1] =
			 (short)(GET_RANGE_SIZE(1, (ccu_fix_blk_s - 1)));
		mvaInfoGraph[domain_idx][1] = priv_reserve;
		mvaInfoGraph[domain_idx][ccu_fix_blk_s - 1] = priv_reserve;
		M4UINFO("%d domian:%d, mvaGraph[1]:0x%x,mvaGraph[%u]:0x%x\n",
			__LINE__, domain_idx,
			mvaGraph[domain_idx][1], (ccu_fix_blk_s - 1),
			mvaGraph[domain_idx][ccu_fix_blk_s - 1]);

		/*ccu:[1024,1152] reserved */
		ccu_nr = MVA_GRAPH_BLOCK_NR_ALIGNED(
			CCU_FIX_MVA_END - CCU_FIX_MVA_START + 1);
		for (i = 0; i < ccu_nr; i++)
			MVA_SET_RESERVED(domain_idx, ccu_fix_blk_s + i);
		mvaGraph[domain_idx][ccu_fix_blk_s] =
			MVA_RESERVED_MASK | ccu_nr;
		mvaGraph[domain_idx][ccu_fix_blk_e] =
			MVA_RESERVED_MASK | ccu_nr;
		mvaInfoGraph[domain_idx][ccu_fix_blk_s] = priv_reserve;
		mvaInfoGraph[domain_idx][ccu_fix_blk_e] = priv_reserve;
		M4UINFO("%d domian:%d, mvaGraph[%u]:0x%x,mvaGraph[%u]:0x%x\n",
			__LINE__, domain_idx,
			ccu_fix_blk_s,
			mvaGraph[domain_idx][ccu_fix_blk_s],
			ccu_fix_blk_e,
			mvaGraph[domain_idx][ccu_fix_blk_e]);

		/*free:[1153,4095]*/
		mvaGraph[domain_idx][ccu_fix_blk_e + 1] =
			GET_RANGE_SIZE((ccu_fix_blk_e + 1), MVA_MAX_BLOCK_NR);
		mvaGraph[domain_idx][MVA_MAX_BLOCK_NR] =
			GET_RANGE_SIZE((ccu_fix_blk_e + 1), MVA_MAX_BLOCK_NR);
		mvaInfoGraph[domain_idx][ccu_fix_blk_e + 1] = priv_reserve;
		mvaInfoGraph[domain_idx][MVA_MAX_BLOCK_NR] = priv_reserve;
		M4UINFO("domian:%d, mvaGraph[%u]: 0x%x, mvaGraph[%u]: 0x%x\n",
			domain_idx, (ccu_fix_blk_e + 1),
			mvaGraph[domain_idx][ccu_fix_blk_e + 1],
			MVA_MAX_BLOCK_NR,
			mvaGraph[domain_idx][MVA_MAX_BLOCK_NR]);

	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
}

int is_in_ccu_region(unsigned int index, unsigned int nr)
{
	unsigned int ccu_fix_blk_s = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_blk_e = MVAGRAPH_INDEX(CCU_FIX_MVA_END);

	if (index >= ccu_fix_blk_s &&
		GET_END_INDEX(index, nr) <= ccu_fix_blk_e)
		return 1;

	return 0;
}

int is_intersected_with_ccu_region(unsigned int start, unsigned int nr)
{
	unsigned int ccu_fix_blk_s = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	unsigned int ccu_fix_blk_e = MVAGRAPH_INDEX(CCU_FIX_MVA_END);
	int ret = 0;

	M4ULOG_LOW("%s:start = 0x%x, end = 0x%x nr = %x.\n",
		__func__, start, GET_END_INDEX(start, nr), nr);

	/*case 1: 1 <= start < 0x200 && 0x300<=end<=0xFFF*/
	if ((start >= 1 && start < ccu_fix_blk_s)
	    && (GET_END_INDEX(start, nr) >= ccu_fix_blk_s &&
	    GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;
	/*case 2: 1 <=start < 0x800 && 0x800<=end<=0xFFF*/
	if ((start >= 1 && start < (ccu_fix_blk_e + 1))
		&& (GET_END_INDEX(start, nr) >= (ccu_fix_blk_e + 1)
			&& GET_END_INDEX(start, nr) <= MVA_MAX_BLOCK_NR))
		ret = 1;

	if (ret)
		M4ULOG_LOW("input region intersects to ccu region\n");

	return ret;

}

int check_reserved_region_integrity(
	unsigned int domain_idx, unsigned int start, unsigned int nr)
{
	int i, integrity = 0;

	for (i = 0; i < nr; i++) {
		if (!MVA_IS_RESERVED(domain_idx, start + i))
			break;
	}
	if (i == nr)
		integrity = 1;
	else {
		M4UMSG(
			"domain:%u,reserved blocks[0x%x-0x%x] corruptted at 0x%x\n",
			domain_idx, start, GET_END_INDEX(start, nr), i);
	}
	return integrity;
}

void m4u_mvaGraph_dump_raw(void)
{
	int i, j;
	unsigned long irq_flags;
	spinlock_t *mva_graph_lock = get_mva_graph_lock(SPINLOCK_COMMON);

	spin_lock_irqsave(mva_graph_lock, irq_flags);
	M4ULOG_HIGH("[M4U_K] dump raw data of mvaGraph:============>\n");
	for (i = 0; i < MVA_DOMAIN_NR; i++)
		for (j = 0; j < MVA_MAX_BLOCK_NR + 1; j++)
			M4ULOG_HIGH("0x%4x: 0x%08x\n", i, mvaGraph[i][j]);
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
}

void m4u_mvaGraph_dump(unsigned int domain_idx)
{
	unsigned int addr = 0, size = 0;
	unsigned short index = 1, nr = 0;
	int i, max_bit, is_busy;
	short frag[12] = { 0 };
	short nr_free = 0, nr_alloc = 0;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	M4ULOG_HIGH(
		"[M4U_K] mva allocation info dump: domain=%u ==================>\n",
		domain_idx);
	M4ULOG_HIGH("start      size     blocknum    busy\n");

	spin_lock_irqsave(mva_graph_lock, irq_flags);
	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		addr = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(domain_idx, index);
		size = nr << MVA_BLOCK_SIZE_ORDER;
		if (MVA_IS_BUSY(domain_idx, index)) {
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

		M4ULOG_HIGH("0x%08x  0x%08x  %4d    %d\n",
			addr, size, nr, is_busy);
	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	M4ULOG_HIGH("\n");
	M4ULOG_HIGH(
		"[M4U_K] mva alloc summary: (unit: blocks)========================>\n");
	M4ULOG_HIGH(
		"free: %d , alloc: %d, total: %d\n",
			nr_free, nr_alloc, nr_free + nr_alloc);
	M4ULOG_HIGH(
		"[M4U_K] free region fragments in 2^x blocks unit:===============\n");
	M4ULOG_HIGH(
		"  0     1     2     3     4     5     6     7     8     9     10    11\n");
	M4ULOG_HIGH(
		"%4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d  %4d\n",
			frag[0], frag[1], frag[2], frag[3],
			frag[4], frag[5], frag[6],
			frag[7], frag[8], frag[9], frag[10], frag[11]);
	M4ULOG_HIGH(
		"[M4U_K] mva alloc dump done=========================<\n");
}

void *mva_get_priv_ext(unsigned int domain_idx, unsigned int mva)
{
	void *priv = NULL;
	unsigned int index;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return NULL;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	index = MVAGRAPH_INDEX(mva);
	if (index == 0 || index > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. mva=0x%x, domain=%u\n",
			mva, domain_idx);
		return NULL;
	}

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	/* find prev head/tail of this region */
	while (mvaGraph[domain_idx][index] == 0)
		index--;

	if (MVA_IS_BUSY(domain_idx, index))
		priv = mvaInfoGraph[domain_idx][index];

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
	return priv;
}

int mva_foreach_priv(mva_buf_fn_t *fn, void *data,
		unsigned int domain_idx)
{
	unsigned short index = 1, nr = 0;
	unsigned int mva;
	void *priv;
	unsigned long irq_flags;
	int ret;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return -1;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		mva = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(domain_idx, index);
		if (MVA_IS_BUSY(domain_idx, index)) {
			priv = mvaInfoGraph[domain_idx][index];
			ret = fn(priv, mva, mva + nr * MVA_BLOCK_SIZE, data);
			if (ret)
				break;
		}
	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
	return 0;
}

unsigned int get_first_valid_mva(unsigned int domain_idx)
{
	unsigned short index = 1, nr = 0;
	unsigned int mva;
	void *priv;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	for (index = 1; index < MVA_MAX_BLOCK_NR + 1; index += nr) {
		mva = index << MVA_BLOCK_SIZE_ORDER;
		nr = MVA_GET_NR(domain_idx, index);
		if (MVA_IS_BUSY(domain_idx, index)) {
			priv = mvaInfoGraph[domain_idx][index];
			break;
		}
	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
	return mva;
}

/*
 * return 1: it is ccu port, and alloc ccu region  --right
 * return 0: it isn't ccu port, and alloc non-ccu region --right
 * return -1: the port is ccu region, but it isn't ccu port; -- wrong
 * the region is not ccu region, but it  insersected with ccu region -- wrong
 */
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
#elif defined(CONFIG_MACH_MT6779)
	is_ccu_port = (pMvaInfo->port == M4U_PORT_CCU0) ||
		(pMvaInfo->port == M4U_PORT_CCU1) ||
			(pMvaInfo->port == M4U_PORT_CAM_CCUI) ||
			(pMvaInfo->port == M4U_PORT_CAM_CCUO);
#else
	return 0;
#endif
	/*check if input mva region is in ccu region.
	 *if it's in ccu region, we will check if it's non-ccu port
	 */
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

	/* if the port is not ccu, it will check
	 * if input mva region is intersected with ccu region
	 */
	is_interseted = is_intersected_with_ccu_region(startIdx, nr);
	/*return 0:means other port normal alloction.
	 * return 1:it isn't in ccu region but insersected with ccu region.
	 */
	if (!is_in && !is_interseted)
		return 0;
	M4UINFO(
		"[0x%x - 0x%x] requested by port(%d) intersects to ccu region!\n",
		startIdx, GET_END_INDEX(startIdx, nr),
		pMvaInfo->port);
	return -1;
}

void *mva_get_priv(unsigned int mva, unsigned int domain_idx)
{
	void *priv = NULL;
	unsigned int index;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
					__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	index = MVAGRAPH_INDEX(mva);
	if (index == 0 || index > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. mva=0x%x, domain=%u\n",
			mva, domain_idx);
		return NULL;
	}

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	if (MVA_IS_BUSY(domain_idx, index))
		priv = mvaInfoGraph[domain_idx][index];

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);
	return priv;
}

unsigned int __m4u_do_mva_alloc(unsigned int domain_idx,
	unsigned long va, unsigned int size, void *priv)
{
	unsigned short s, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;
	int   region_status = 0;
	const short ccu_fix_index_start = MVAGRAPH_INDEX(CCU_FIX_MVA_START);
	const short ccu_fix_index_end = MVAGRAPH_INDEX(CCU_FIX_MVA_END);

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
					__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

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

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	/* first: [1,ccu_start)*/
	for (s = 1; (s < ccu_fix_index_start) &&
		(mvaGraph[domain_idx][s] < nr);
		s += (mvaGraph[domain_idx][s] & MVA_BLOCK_NR_MASK))
		;

	/* second: if don't have free mva, find mva in (ccu_end, 4095]*/
	if (s == ccu_fix_index_start) {
		s = ccu_fix_index_end + 1;
		for (; (s < (MVA_MAX_BLOCK_NR + 1)) &&
		(mvaGraph[domain_idx][s] < nr);
		s += (mvaGraph[domain_idx][s] & MVA_BLOCK_NR_MASK))
			;

		if (s > MVA_MAX_BLOCK_NR) {
			spin_unlock_irqrestore(mva_graph_lock,
								irq_flags);
			M4UMSG(
				"mva_alloc error: no available MVA region for %d blocks! domain=%u\n",
					nr, domain_idx);
#ifdef M4U_PROFILE
			mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
				MMPROFILE_FLAG_PULSE, size, s);
#endif

			return 0;
		}
	}

	region_status = __check_ccu_mva_region(s, nr, priv);
	if (region_status) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_ALLOC_MVA],
			MMPROFILE_FLAG_END, mvaGraph[domain_idx][0x2f8], 0xf1);
		M4UMSG(
			"mva_alloc error: fault cursor(0x%x) access CCU region\n",
			s);
		return 0;
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_ALLOC_MVA],
				MMPROFILE_FLAG_END,
				mvaGraph[domain_idx][0x2f8], 0xf2);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks!\n",
				nr);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
			MMPROFILE_FLAG_PULSE, size, s);
#endif
		return 0;
	}
	/* alloc a mva region */
	end = s + mvaGraph[domain_idx][s] - 1;

	if (unlikely(nr == mvaGraph[domain_idx][s])) {
		MVA_SET_BUSY(domain_idx, s);
		MVA_SET_BUSY(domain_idx, end);
		mvaInfoGraph[domain_idx][s] = priv;
		mvaInfoGraph[domain_idx][end] = priv;
	} else {
		new_end = s + nr - 1;
		new_start = new_end + 1;
		/* note: new_start may equals to end */
		mvaGraph[domain_idx][new_start] =
				(mvaGraph[domain_idx][s] - nr);
		mvaGraph[domain_idx][new_end] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][s] = mvaGraph[domain_idx][new_end];
		mvaGraph[domain_idx][end] = mvaGraph[domain_idx][new_start];

		mvaInfoGraph[domain_idx][s] = priv;
		mvaInfoGraph[domain_idx][new_end] = priv;
	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

unsigned int m4u_do_mva_alloc(unsigned int domain_idx,
	unsigned long va, unsigned int size, void *priv)
{
	unsigned short s, end;
	unsigned short new_start, new_end;
	unsigned short nr = 0;
	unsigned int mvaRegionStart;
	unsigned long startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0) {
		unsigned int mva = 0;

		mva = __m4u_do_mva_alloc(domain_idx, va, size, priv);
		return mva;
	}

	if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
					__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

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

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	/* ----------------------------------------------- */
	/* find first match free region */
	for (s = 1; (s < (MVA_MAX_BLOCK_NR + 1)) &&
				(mvaGraph[domain_idx][s] < nr);
		s += (mvaGraph[domain_idx][s] & MVA_BLOCK_NR_MASK))
		;
	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock,
							irq_flags);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks! domain=%u\n",
				nr, domain_idx);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
			MMPROFILE_FLAG_PULSE, size, s);
#endif

		return 0;
	}
	/* ----------------------------------------------- */
	/* alloc a mva region */
	end = s + mvaGraph[domain_idx][s] - 1;

	if (unlikely(nr == mvaGraph[domain_idx][s])) {
		MVA_SET_BUSY(domain_idx, s);
		MVA_SET_BUSY(domain_idx, end);
		mvaInfoGraph[domain_idx][s] = priv;
		mvaInfoGraph[domain_idx][end] = priv;
	} else {
		new_end = s + nr - 1;
		new_start = new_end + 1;
		/* note: new_start may equals to end */
		mvaGraph[domain_idx][new_start] =
				(mvaGraph[domain_idx][s] - nr);
		mvaGraph[domain_idx][new_end] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][s] = mvaGraph[domain_idx][new_end];
		mvaGraph[domain_idx][end] = mvaGraph[domain_idx][new_start];

		mvaInfoGraph[domain_idx][s] = priv;
		mvaInfoGraph[domain_idx][new_end] = priv;
	}

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

unsigned int __m4u_do_mva_alloc_fix(unsigned int domain_idx,
		unsigned long va, unsigned int mva,
		unsigned int size, void *priv)
{
	unsigned short nr = 0;
	unsigned int startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short endIdx;
	unsigned short region_start, region_end;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;
	int ccu_region_status, is_in_ccu_region = 0;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	if (size == 0) {
		M4UMSG("%s: invalid size\n", __func__);
		return 0;
	}
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. index=0x%x, domain=%u\n",
			startIdx, domain_idx);
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

	ccu_region_status = __check_ccu_mva_region(startIdx, nr, priv);
	if (ccu_region_status == -1) {
		M4UMSG(
			"%s error alloc mva fail! is in ccu reserved region\n",
			__func__);
		return 0;
	} else if (ccu_region_status == 1)
		is_in_ccu_region = 1;

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	region_start = startIdx;
	/* find prev head of this region */
	while (mvaGraph[domain_idx][region_start] == 0)
		region_start--;

	if (MVA_IS_BUSY(domain_idx, region_start) ||
		(MVA_GET_NR(domain_idx, region_start) <
			nr + startIdx - region_start)) {
		M4UMSG(
			"%s mva is inuse index=0x%x, mvaGraph=0x%x, domain=%u\n",
			__func__, region_start,
			mvaGraph[domain_idx][region_start],
			domain_idx);
		mva = 0;
		goto out;
	}

	/* carveout startIdx~startIdx+nr-1 out of region_start */
	endIdx = startIdx + nr - 1;
	region_end = region_start + MVA_GET_NR(domain_idx, region_start) - 1;

	if (startIdx == region_start && endIdx == region_end) {
		MVA_SET_BUSY(domain_idx, startIdx);
		MVA_SET_BUSY(domain_idx, endIdx);
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
		}
	} else if (startIdx == region_start) {
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
			MVA_SET_RESERVED(domain_idx, endIdx + 1);
			MVA_SET_RESERVED(domain_idx, region_end);
		}
	} else if (endIdx == region_end) {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, region_start);
			MVA_SET_RESERVED(domain_idx, startIdx - 1);
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
		}
	} else {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, region_start);
			MVA_SET_RESERVED(domain_idx, startIdx - 1);
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
			MVA_SET_RESERVED(domain_idx, endIdx + 1);
			MVA_SET_RESERVED(domain_idx, region_end);
		}
	}

	mvaInfoGraph[domain_idx][startIdx] = priv;
	mvaInfoGraph[domain_idx][endIdx] = priv;

out:
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	return mva;
}

unsigned int m4u_do_mva_alloc_fix(unsigned int domain_idx,
		unsigned long va, unsigned int mva,
		unsigned int size, void *priv)
{
	unsigned short nr = 0;
	unsigned int startRequire, endRequire, sizeRequire;
	unsigned long irq_flags;
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short endIdx;
	unsigned short region_start, region_end;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0) {
		unsigned int fix_mva = 0;

		fix_mva = __m4u_do_mva_alloc_fix(domain_idx,
			va, mva, size, priv);
		return fix_mva;
	}

	if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	if (size == 0)
		return 0;
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		M4UMSG("mvaGraph index is 0. index=0x%x, domain=%u\n",
			startIdx, domain_idx);
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

	M4ULOG_MID(
		"%s mva:0x%x, startIdx=%d, size = %d, nr= %d, domain=%u\n",
		__func__,
		mva, startIdx, size, nr, domain_idx);

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	region_start = startIdx;
	/* find prev head of this region */
	while (mvaGraph[domain_idx][region_start] == 0)
		region_start--;

	if (MVA_IS_BUSY(domain_idx, region_start) ||
		(MVA_GET_NR(domain_idx, region_start) <
			nr + startIdx - region_start)) {
		M4UMSG(
			"%s mva is inuse index=0x%x, mvaGraph=0x%x, domain=%u\n",
			__func__, region_start,
			mvaGraph[domain_idx][region_start],
			domain_idx);
		mva = 0;
		goto out;
	}

	/* carveout startIdx~startIdx+nr-1 out of region_start */
	endIdx = startIdx + nr - 1;
	region_end = region_start + MVA_GET_NR(domain_idx, region_start) - 1;

	if (startIdx == region_start && endIdx == region_end) {
		MVA_SET_BUSY(domain_idx, startIdx);
		MVA_SET_BUSY(domain_idx, endIdx);
	} else if (startIdx == region_start) {
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
	} else if (endIdx == region_end) {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
	} else {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
	}

	mvaInfoGraph[domain_idx][startIdx] = priv;
	mvaInfoGraph[domain_idx][endIdx] = priv;

out:
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	return mva;
}

unsigned int __m4u_do_mva_alloc_start_from(
		unsigned int domain_idx,
		unsigned long va, unsigned int mva,
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
	int ccu_region_status, is_in_ccu_region = 0;

	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	if (size == 0) {
		M4UMSG("%s: invalid size\n", __func__);
		return 0;
	}

	startIdx = (mva + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;

	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	/*check [startIdx, endIdx] status*/
	ccu_region_status = __check_ccu_mva_region(startIdx, nr, priv);
	if (ccu_region_status == -1) {
		M4UMSG(
			"%s error alloc mva fail! is in ccu reserved region\n",
			__func__);
		return 0;
	} else if (ccu_region_status == 1)
		is_in_ccu_region = 1;

	M4ULOG_MID(
		"m4u_do_mva_alloc_start_from mva:0x%x, startIdx=%d, size = %d, nr= %d, domain=%u\n",
		mva, startIdx, size, nr, domain_idx);

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	/* find this region */
	for (region_start = 1; (region_start < (MVA_MAX_BLOCK_NR + 1));
		 region_start += (MVA_GET_NR(domain_idx, region_start) &
			MVA_BLOCK_NR_MASK)) {
		if ((mvaGraph[domain_idx][region_start] &
					MVA_BLOCK_NR_MASK) == 0) {
			m4u_mvaGraph_dump(domain_idx);
			m4u_aee_print("%s: s=%d, 0x%x, domain=%u\n",
				__func__, s,
				mvaGraph[domain_idx][region_start],
				domain_idx);
		}
		if ((region_start + MVA_GET_NR(domain_idx,
					region_start)) > startIdx) {
			next_region_start = region_start +
					MVA_GET_NR(domain_idx, region_start);
			break;
		}
	}

	if (region_start > MVA_MAX_BLOCK_NR) {
		M4UMSG(
			"%s:alloc mva fail,no available MVA for %d blocks, domain=%u\n",
				__func__, nr, domain_idx);
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		return 0;
	}

	region_end = region_start + MVA_GET_NR(domain_idx, region_start) - 1;

	if (next_region_start == 0) {
		M4UMSG("no enough mva to allocate.\n");
		m4u_aee_print(
			"%s: region_start: %d, region_end= %d, region= %d, domain=%u\n",
			__func__, region_start, region_end,
			MVA_GET_NR(domain_idx, region_start),
			domain_idx);
	}

	if (MVA_IS_BUSY(domain_idx, region_start)) {
		M4UMSG("%s mva is inuse index=%d, mvaGraph=0x%x, domain=%u\n",
			__func__, region_start,
			mvaGraph[domain_idx][region_start],
			domain_idx);
		s = region_start;
	} else {
		if ((region_end - startIdx + 1) < nr)
			s = next_region_start;
		else
			M4UMSG("mva is free region_start=%d, s=%d, domain=%u\n",
				region_start, s, domain_idx);
	}

	M4ULOG_MID(
		"region_start: %d, region_end= %d, region= %d, next_region_start= %d, search start: %d, domain=%u\n",
		region_start, region_end,
		MVA_GET_NR(domain_idx, region_start), next_region_start, s,
		domain_idx);

	/* ----------------------------------------------- */
	if (s != 0) {
		/* find first match free region */
		for (; (s < (MVA_MAX_BLOCK_NR + 1));
				s += (mvaGraph[domain_idx][s] &
					MVA_BLOCK_NR_MASK)) {
			/*error check*/
			if ((mvaGraph[domain_idx][s] &
					MVA_BLOCK_NR_MASK) == 0) {
				spin_unlock_irqrestore(mva_graph_lock,
						irq_flags);
				m4u_mvaGraph_dump(domain_idx);
				m4u_aee_print("%s: s=%d, 0x%x, domain=%u\n",
					__func__, s, mvaGraph[domain_idx][s],
					domain_idx);
				return 0;
			}
			if (MVA_GET_NR(domain_idx, s) > nr &&
					!MVA_IS_BUSY(domain_idx, s)) {
				/*check [s, s + MVA_GET_NR(s) -1] status*/
				ccu_region_status = __check_ccu_mva_region(s,
					MVA_GET_NR(domain_idx, s), priv);
				if (ccu_region_status == -1)
					continue;
				else if (ccu_region_status == 1) {
					is_in_ccu_region = 1;
					break;
				} else if (ccu_region_status == 0)
					break;
			}
		}
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks!, domain=%u\n",
					nr, domain_idx);
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

		region_end =
			region_start + MVA_GET_NR(domain_idx, region_start) - 1;
		M4UMSG(
			"region_start: %d, region_end= %d, startIdx: %d, endIdx= %d, domain=%u\n",
			region_start, region_end, startIdx, endIdx,
			domain_idx);

		if (startIdx == region_start && endIdx == region_end) {
			MVA_SET_BUSY(domain_idx, startIdx);
			MVA_SET_BUSY(domain_idx, endIdx);
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(domain_idx, startIdx);
				MVA_SET_RESERVED(domain_idx, endIdx);
			}

	} else if (startIdx == region_start) {
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
			MVA_SET_RESERVED(domain_idx, endIdx + 1);
			MVA_SET_RESERVED(domain_idx, region_end);
		}
	} else if (endIdx == region_end) {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, region_start);
			MVA_SET_RESERVED(domain_idx, startIdx - 1);
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
		}
	} else {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
		if (is_in_ccu_region) {
			MVA_SET_RESERVED(domain_idx, region_start);
			MVA_SET_RESERVED(domain_idx, startIdx - 1);
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, endIdx);
			MVA_SET_RESERVED(domain_idx, endIdx + 1);
			MVA_SET_RESERVED(domain_idx, region_end);
		}
	}

	mvaInfoGraph[domain_idx][startIdx] = priv;
	mvaInfoGraph[domain_idx][endIdx] = priv;
	s = startIdx;
	} else {
		/* alloc a mva region */
		end = s + MVA_GET_NR(domain_idx, s) - 1;

		/*check [startIdx, endIdx] status*/
		is_in_ccu_region = 0;
		ccu_region_status = __check_ccu_mva_region(s, nr, priv);
		if (ccu_region_status == 1)
			is_in_ccu_region = 1;

		if (unlikely(nr == MVA_GET_NR(domain_idx, s))) {
			MVA_SET_BUSY(domain_idx, s);
			MVA_SET_BUSY(domain_idx, end);
			mvaInfoGraph[domain_idx][s] = priv;
			mvaInfoGraph[domain_idx][end] = priv;
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(domain_idx, s);
				MVA_SET_RESERVED(domain_idx, end);
			}
		} else {
			new_end = s + nr - 1;
			new_start = new_end + 1;
			/* note: new_start may equals to end */
			mvaGraph[domain_idx][new_start] =
					(MVA_GET_NR(domain_idx, s) - nr);
			mvaGraph[domain_idx][new_end] = nr | MVA_BUSY_MASK;
			mvaGraph[domain_idx][s] = mvaGraph[domain_idx][new_end];
			mvaGraph[domain_idx][end] =
					mvaGraph[domain_idx][new_start];
			if (is_in_ccu_region) {
				MVA_SET_RESERVED(domain_idx, new_start);
				MVA_SET_RESERVED(domain_idx, new_end);
				MVA_SET_RESERVED(domain_idx, s);
				MVA_SET_RESERVED(domain_idx, end);
			}

			mvaInfoGraph[domain_idx][s] = priv;
			mvaInfoGraph[domain_idx][new_end] = priv;
		}
	}
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

unsigned int m4u_do_mva_alloc_start_from(
		unsigned int domain_idx,
		unsigned long va, unsigned int mva,
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
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0) {
		unsigned int mva_from = 0;

		mva_from = __m4u_do_mva_alloc_start_from(domain_idx,
				va, mva, size, priv);
		return mva_from;
	}

	if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return 0;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	if (size == 0)
		return 0;

	startIdx = (mva + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;

	/* ----------------------------------------------------- */
	/* calculate mva block number */
	startRequire = va & (~M4U_PAGE_MASK);
	endRequire = (va + size - 1) | M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nr = (sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */

	M4ULOG_MID(
		"%s mva:0x%x, startIdx=%d, size = %d, nr= %d, domain=%u\n",
		__func__, mva, startIdx, size, nr, domain_idx);

	spin_lock_irqsave(mva_graph_lock, irq_flags);

	/* find this region */
	for (region_start = 1; (region_start < (MVA_MAX_BLOCK_NR + 1));
		 region_start += (MVA_GET_NR(domain_idx, region_start) &
			MVA_BLOCK_NR_MASK)) {
		if ((mvaGraph[domain_idx][region_start] &
					MVA_BLOCK_NR_MASK) == 0) {
			m4u_mvaGraph_dump(domain_idx);
			m4u_aee_print("%s: s=%d, 0x%x, domain=%u\n",
				__func__, s,
				mvaGraph[domain_idx][region_start],
				domain_idx);
		}
		if ((region_start + MVA_GET_NR(domain_idx,
					region_start)) > startIdx) {
			next_region_start = region_start +
					MVA_GET_NR(domain_idx, region_start);
			break;
		}
	}

	if (region_start > MVA_MAX_BLOCK_NR) {
		M4UMSG(
			"%s:alloc mva fail,no available MVA for %d blocks, domain=%u\n",
				__func__, nr, domain_idx);
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		return 0;
	}

	region_end = region_start + MVA_GET_NR(domain_idx, region_start) - 1;

	if (next_region_start == 0) {
		m4u_aee_print(
			"%s: region_start: %d, region_end= %d, region= %d, domain=%u\n",
			__func__, region_start, region_end,
			MVA_GET_NR(domain_idx, region_start),
			domain_idx);
	}

	if (MVA_IS_BUSY(domain_idx, region_start)) {
		M4UMSG("%s mva is inuse index=%d, mvaGraph=0x%x, domain=%u\n",
			__func__, region_start,
			mvaGraph[domain_idx][region_start],
			domain_idx);
		s = region_start;
	} else {
		if ((region_end - startIdx + 1) < nr)
			s = next_region_start;
		else
			M4UMSG("mva is free region_start=%d, s=%d, domain=%u\n",
				region_start, s, domain_idx);
	}

	M4ULOG_MID(
		"region_start: %d, region_end= %d, region= %d, next_region_start= %d, search start: %d, domain=%u\n",
		region_start, region_end,
		MVA_GET_NR(domain_idx, region_start), next_region_start, s,
		domain_idx);

	/* ----------------------------------------------- */
	if (s != 0) {
		/* find first match free region */
		for (; (s < (MVA_MAX_BLOCK_NR + 1)) &&
						(mvaGraph[domain_idx][s] < nr);
				s += (mvaGraph[domain_idx][s] &
					MVA_BLOCK_NR_MASK)) {
			if ((mvaGraph[domain_idx][s] &
					MVA_BLOCK_NR_MASK) == 0) {
				m4u_aee_print("%s: s=%d, 0x%x, domain=%u\n",
					__func__, s, mvaGraph[domain_idx][s],
					domain_idx);
				m4u_mvaGraph_dump(domain_idx);
			}
		}
	}

	if (s > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG(
			"mva_alloc error: no available MVA region for %d blocks!, domain=%u\n",
					nr, domain_idx);
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

		region_end =
			region_start + MVA_GET_NR(domain_idx, region_start) - 1;
		M4UMSG(
			"region_start: %d, region_end= %d, startIdx: %d, endIdx= %d, domain=%u\n",
			region_start, region_end, startIdx, endIdx,
			domain_idx);

		if (startIdx == region_start && endIdx == region_end) {
			MVA_SET_BUSY(domain_idx, startIdx);
			MVA_SET_BUSY(domain_idx, endIdx);

	} else if (startIdx == region_start) {
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
	} else if (endIdx == region_end) {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
	} else {
		mvaGraph[domain_idx][region_start] = startIdx - region_start;
		mvaGraph[domain_idx][startIdx - 1] =
				mvaGraph[domain_idx][region_start];
		mvaGraph[domain_idx][startIdx] = nr | MVA_BUSY_MASK;
		mvaGraph[domain_idx][endIdx] = mvaGraph[domain_idx][startIdx];
		mvaGraph[domain_idx][endIdx + 1] = region_end - endIdx;
		mvaGraph[domain_idx][region_end] =
				mvaGraph[domain_idx][endIdx + 1];
	}

	mvaInfoGraph[domain_idx][startIdx] = priv;
	mvaInfoGraph[domain_idx][endIdx] = priv;
		s = startIdx;
	} else {
		/* alloc a mva region */
		end = s + mvaGraph[domain_idx][s] - 1;

		if (unlikely(nr == mvaGraph[domain_idx][s])) {
			MVA_SET_BUSY(domain_idx, s);
			MVA_SET_BUSY(domain_idx, end);
			mvaInfoGraph[domain_idx][s] = priv;
			mvaInfoGraph[domain_idx][end] = priv;
		} else {
			new_end = s + nr - 1;
			new_start = new_end + 1;
			/* note: new_start may equals to end */
			mvaGraph[domain_idx][new_start] =
					(mvaGraph[domain_idx][s] - nr);
			mvaGraph[domain_idx][new_end] = nr | MVA_BUSY_MASK;
			mvaGraph[domain_idx][s] = mvaGraph[domain_idx][new_end];
			mvaGraph[domain_idx][end] =
					mvaGraph[domain_idx][new_start];

			mvaInfoGraph[domain_idx][s] = priv;
			mvaInfoGraph[domain_idx][new_end] = priv;
		}
	}
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	mvaRegionStart = (unsigned int)s;

	return (mvaRegionStart << MVA_BLOCK_SIZE_ORDER) + mva_pageOffset(va);
}

#define RightWrong(x) ((x) ? "correct" : "error")

int __m4u_do_mva_free(unsigned int domain_idx,
		unsigned int mva, unsigned int size)
{
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short nr;
	unsigned short endIdx;
	unsigned int startRequire, endRequire, sizeRequire;
	short nrRequire, nr_tmp = 0;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;
	int ccu_region_status, is_in_ccu_region_flag = 0;
	struct m4u_buf_info_t *p_mva_info;
	int ret = 0;
	int port;

	if (domain_idx == 0)
		lock_type = SPINLOCK_MVA_GRAPH0;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return -1;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	spin_lock_irqsave(mva_graph_lock, irq_flags);
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG("mvaGraph index is 0. mva=0x%x, domain=%u\n",
			mva, domain_idx);
		return -1;
	}
	nr = mvaGraph[domain_idx][startIdx] & MVA_BLOCK_NR_MASK;
	endIdx = startIdx + nr - 1;

	p_mva_info =
		(struct m4u_buf_info_t *)mvaInfoGraph[domain_idx][startIdx];

	if (size == 0 || nr == 0 || p_mva_info == NULL) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG("%s error: the input size = %d nr = %d mva_info = %p.\n",
			__func__, size, nr, p_mva_info);
		if (p_mva_info)
			M4UMSG("error port id = %d\n", p_mva_info->port);
		m4u_mvaGraph_dump(domain_idx);
		return -1;
	}
	port = p_mva_info->port;
	ccu_region_status = __check_ccu_mva_region(startIdx,
				nr, (void *)p_mva_info);
	if (ccu_region_status == -1) {
		M4UMSG("%s error ccu region\n", __func__);
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		return -1;
	} else if (ccu_region_status == 1)
		is_in_ccu_region_flag = 1;

	/* -------------------------------- */
	/* check the input arguments */
	/* right condition: startIdx is not NULL &&
	 * region is busy && right module && right size
	 */
	startRequire = mva & (unsigned int)(~M4U_PAGE_MASK);
	endRequire = (mva + size - 1) | (unsigned int)M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nrRequire =
		(sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */
	if (!(startIdx != 0	/* startIdx is not NULL */
		&& MVA_IS_BUSY(domain_idx, startIdx)
		&& (nr == nrRequire))) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG(
			"error to free mva , domain=%u========================>\n",
			domain_idx);
		M4UMSG("BufSize=%d(unit:0x%xBytes) (expect %d) [%s]\n",
		       nrRequire, MVA_BLOCK_SIZE,
		       nr, RightWrong(nrRequire == nr));
		M4UMSG("mva=0x%x, (IsBusy?)=%d (expect %d) [%s]\n",
		       mva, MVA_IS_BUSY(domain_idx, startIdx), 1,
		       RightWrong(MVA_IS_BUSY(domain_idx, startIdx)));
		m4u_mvaGraph_dump(domain_idx);
		/* m4u_mvaGraph_dump_raw(); */
		return -1;
	}

	mvaInfoGraph[domain_idx][startIdx] = NULL;
	mvaInfoGraph[domain_idx][endIdx] = NULL;

	if (is_in_ccu_region_flag) {
		/* merge with followed region.
		 * if endIdx + 1 is in vpu region, do the merge as before.
		 * or, since we have set the reserved bit
		 * when alloc, it's ok to do nothing.
		 */
		nr_tmp = MVA_GET_NR(domain_idx, endIdx + 1);
		if (is_in_ccu_region(endIdx + 1, nr_tmp) &&
				!MVA_IS_BUSY(domain_idx, endIdx + 1)) {
			nr += nr_tmp;
			mvaGraph[domain_idx][endIdx] = 0;
			mvaGraph[domain_idx][endIdx + 1] = 0;
			MVA_SET_RESERVED(domain_idx, endIdx);
			MVA_SET_RESERVED(domain_idx, endIdx + 1);
		}
		/* merge with previous region
		 * same operation as merging with followed region
		 */
		nr_tmp = MVA_GET_NR(domain_idx, startIdx - 1);
		if (is_in_ccu_region(GET_START_INDEX((startIdx - 1),
				nr_tmp), nr_tmp)
			&& (!MVA_IS_BUSY(domain_idx, startIdx - 1))) {
			int pre_nr = nr_tmp;

			mvaGraph[domain_idx][startIdx] = 0;
			mvaGraph[domain_idx][startIdx - 1] = 0;
			MVA_SET_RESERVED(domain_idx, startIdx);
			MVA_SET_RESERVED(domain_idx, startIdx - 1);
			startIdx -= pre_nr;
			nr += pre_nr;
		}
		/* -------------------------------- */
		/* set region flags */
		mvaGraph[domain_idx][startIdx] = nr;
		mvaGraph[domain_idx][startIdx + nr - 1] = nr;
		MVA_SET_RESERVED(domain_idx, startIdx);
		MVA_SET_RESERVED(domain_idx, startIdx + nr - 1);
	} else {

		/* merge with followed region.*/
		nr_tmp = MVA_GET_NR(domain_idx, endIdx + 1);
		if ((endIdx + 1 <= MVA_MAX_BLOCK_NR) &&
			(!MVA_IS_BUSY(domain_idx, endIdx + 1))) {
			/* check if the followed region is in vpu region.
			 * yes -> do nothing
			 * no  -> do the merging
			 */
			if (!MVA_IS_RESERVED(domain_idx, endIdx + 1)) {
				nr += nr_tmp;
				mvaGraph[domain_idx][endIdx] = 0;
				mvaGraph[domain_idx][endIdx + 1] = 0;
			}
		}

		/* merge with previous region
		 * same operation as merging with followed region
		 */
		nr_tmp = MVA_GET_NR(domain_idx, startIdx - 1);
		if ((startIdx - 1) > 0 &&
			(!MVA_IS_BUSY(domain_idx, startIdx - 1))) {
			if (!MVA_IS_RESERVED(domain_idx, startIdx - 1)) {
				int pre_nr = nr_tmp;

				mvaGraph[domain_idx][startIdx] = 0;
				mvaGraph[domain_idx][startIdx - 1] = 0;
				startIdx -= pre_nr;
				nr += pre_nr;
			}
		}
		/* -------------------------------- */
		/* set region flags */
		mvaGraph[domain_idx][startIdx] = nr;
		mvaGraph[domain_idx][startIdx + nr - 1] = nr;
	}
	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	/*for debug*/
	ret = check_reserved_region_integrity(domain_idx,
		MVAGRAPH_INDEX(CCU_FIX_MVA_START),
		CCU_FIX_BLOCK_NR);
	if (!ret)
		M4UMSG(
		"CCU region is corruptted when port(%d) free mva(0x%x)\n",
		port, mva);

	return 0;
}

int m4u_do_mva_free(unsigned int domain_idx,
		unsigned int mva, unsigned int size)
{
	unsigned short startIdx = mva >> MVA_BLOCK_SIZE_ORDER;
	unsigned short nr;
	unsigned short endIdx;
	unsigned int startRequire, endRequire, sizeRequire;
	short nrRequire;
	unsigned long irq_flags;
	enum graph_lock_tpye lock_type;
	spinlock_t *mva_graph_lock;

	if (domain_idx == 0) {
		int ret = 0;

		ret = __m4u_do_mva_free(domain_idx, mva, size);
		return ret;
	}

	if (domain_idx == 1)
		lock_type = SPINLOCK_MVA_GRAPH1;
	else {
		M4UMSG("%s error: invalid m4u domain_idx(%d)!\n",
				__func__, domain_idx);
		return -1;
	}
	mva_graph_lock = get_mva_graph_lock(lock_type);

	spin_lock_irqsave(mva_graph_lock, irq_flags);
	if (startIdx == 0 || startIdx > MVA_MAX_BLOCK_NR) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);

		M4UMSG("mvaGraph index is 0. mva=0x%x, domain=%u\n",
			mva, domain_idx);
		return -1;
	}
	nr = mvaGraph[domain_idx][startIdx] & MVA_BLOCK_NR_MASK;
	endIdx = startIdx + nr - 1;

	/* -------------------------------- */
	/* check the input arguments */
	/* right condition: startIdx is not NULL &&
	 * region is busy && right module && right size
	 */
	startRequire = mva & (unsigned int)(~M4U_PAGE_MASK);
	endRequire = (mva + size - 1) | (unsigned int)M4U_PAGE_MASK;
	sizeRequire = endRequire - startRequire + 1;
	nrRequire =
		(sizeRequire + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER;
	/* (sizeRequire>>MVA_BLOCK_SIZE_ORDER) +
	 * ((sizeRequire&MVA_BLOCK_ALIGN_MASK)!=0);
	 */
	if (!(startIdx != 0	/* startIdx is not NULL */
		&& MVA_IS_BUSY(domain_idx, startIdx)
		&& (nr == nrRequire))) {
		spin_unlock_irqrestore(mva_graph_lock, irq_flags);
		M4UMSG(
			"error to free mva , domain=%u========================>\n",
			domain_idx);
		M4UMSG("BufSize=%d(unit:0x%xBytes) (expect %d) [%s]\n",
		       nrRequire, MVA_BLOCK_SIZE,
		       nr, RightWrong(nrRequire == nr));
		M4UMSG("mva=0x%x, (IsBusy?)=%d (expect %d) [%s]\n",
		       mva, MVA_IS_BUSY(domain_idx, startIdx), 1,
		       RightWrong(MVA_IS_BUSY(domain_idx, startIdx)));
		m4u_mvaGraph_dump(domain_idx);
		/* m4u_mvaGraph_dump_raw(); */
		return -1;
	}

	mvaInfoGraph[domain_idx][startIdx] = NULL;
	mvaInfoGraph[domain_idx][endIdx] = NULL;

	/* -------------------------------- */
	/* merge with followed region */
	if ((endIdx + 1 <= MVA_MAX_BLOCK_NR) &&
			(!MVA_IS_BUSY(domain_idx, endIdx + 1))) {
		nr += mvaGraph[domain_idx][endIdx + 1];
		mvaGraph[domain_idx][endIdx] = 0;
		mvaGraph[domain_idx][endIdx + 1] = 0;
	}
	/* -------------------------------- */
	/* merge with previous region */
	if ((startIdx - 1 > 0) && (!MVA_IS_BUSY(domain_idx, startIdx - 1))) {
		int pre_nr = mvaGraph[domain_idx][startIdx - 1];

		mvaGraph[domain_idx][startIdx] = 0;
		mvaGraph[domain_idx][startIdx - 1] = 0;
		startIdx -= pre_nr;
		nr += pre_nr;
	}
	/* -------------------------------- */
	/* set region flags */
	mvaGraph[domain_idx][startIdx] = nr;
	mvaGraph[domain_idx][startIdx + nr - 1] = nr;

	spin_unlock_irqrestore(mva_graph_lock, irq_flags);

	return 0;
}
