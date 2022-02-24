// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __M4U_MVA_H__
#define __M4U_MVA_H__

/* ((va&0xfff)+size+0xfff)>>12 */
#define mva_pageOffset(mva) ((mva)&0xfff)

#define MVA_BLOCK_SIZE_ORDER     20	/* 1M */
#define MVA_MAX_BLOCK_NR        4095	/* 4GB */

#define MVA_BLOCK_SIZE      (1<<MVA_BLOCK_SIZE_ORDER)	/* 0x100000 */
#define MVA_BLOCK_ALIGN_MASK (MVA_BLOCK_SIZE-1)	/* 0xfffff */
#define MVA_BLOCK_NR_MASK   (MVA_MAX_BLOCK_NR)	/* 0xfff */
#define MVA_BUSY_MASK       (1<<15)	/* 0x8000 */
#define MVA_RESERVED_MASK       (1<<14)	/* 0x4000 */

#define MVA_IS_BUSY(index) ((mvaGraph[index]&MVA_BUSY_MASK) != 0)
#define MVA_SET_BUSY(index) (mvaGraph[index] |= MVA_BUSY_MASK)
#define MVA_SET_FREE(index) (mvaGraph[index] & (~MVA_BUSY_MASK))
#define MVA_GET_NR(index)   (mvaGraph[index] & MVA_BLOCK_NR_MASK)
/*the macro is only use for vpu*/
#define MVA_IS_RESERVED(index) ((mvaGraph[index] & MVA_RESERVED_MASK) != 0)
#define MVA_SET_RESERVED(index) (mvaGraph[index] |= MVA_RESERVED_MASK)

/*translate mva to mvaGraph index which mva belongs to*/
#define MVAGRAPH_INDEX(mva) ((mva) >> MVA_BLOCK_SIZE_ORDER)
#define GET_START_INDEX(end, nr) (end - nr + 1)
#define GET_END_INDEX(start, nr) (start + nr - 1)
#define GET_RANGE_SIZE(start, end) (end - start + 1)

/*caculate requeired block number with input mva*/
#define START_ALIGNED(mva) (mva & (~MVA_BLOCK_ALIGN_MASK))
#define END_ALIGNED(mva, nr) (GET_END_INDEX(mva, nr) | MVA_BLOCK_ALIGN_MASK)
#define MVA_GRAPH_BLOCK_NR_ALIGNED(size) \
	((size + MVA_BLOCK_ALIGN_MASK) >> MVA_BLOCK_SIZE_ORDER)

#define MVA_GRAPH_NR_TO_SIZE(nr) (nr << MVA_BLOCK_SIZE_ORDER)

/*reserved mva region for vpu exclusive use*/
#if defined(CONFIG_MACH_MT6775) || defined(CONFIG_MACH_MT6771)
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

#if defined(CONFIG_MACH_MT6775) || defined(CONFIG_MACH_MT6771)
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
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#define CCU_FIX_MVA_START			0x40000000
#define CCU_FIX_MVA_END				0x48000000
#else
#define CCU_FIX_MVA_START			0x2f800000
#define CCU_FIX_MVA_END				0x35800000
#endif
#define MVA_COMMON_CONTIG_RETGION_START          0x80000000

int check_reserved_region_integrity(unsigned int start, unsigned int nr);
int m4u_check_mva_region(unsigned int startIdx, unsigned int nr, void *priv);
unsigned int m4u_do_mva_alloc(unsigned long va, unsigned int size, void *priv);
unsigned int m4u_do_mva_alloc_fix(unsigned long va,
	unsigned int mva, unsigned int size, void *priv);
unsigned int m4u_do_mva_alloc_start_from(unsigned long va,
	unsigned int mva, unsigned int size, void *priv);
unsigned int get_last_free_graph_idx_in_stage1_region(void);
#endif
