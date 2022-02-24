// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include "m4u_mva.h"
#include "m4u_priv.h"

#if defined(CONFIG_MACH_MT6758)
int M4U_VPU_PORT_NAME = M4U_PORT_VPU;
#else
int M4U_VPU_PORT_NAME = M4U_PORT_VPU1;
#endif

void verify_ret(int ret, unsigned int start, unsigned int end)
{
	switch (ret) {
	case -1:
		M4UMSG("test range [0x%x, 0x%x] config error\n", start, end);
		break;
	case 1:
		M4UMSG(
			"test range [0x%x, 0x%x] is in the vpu region\n",
			start, end);
		break;
	case 0:
		M4UMSG(
			"test range [0x%x, 0x%x] is out of vpu region\n",
			start, end);
		break;
	default:
		break;
	}
}

#define case_nr 5
void test_case_check_mva_region(void)
{
	int ret = 0;
	int i = 0, j = 0;
	unsigned int test_mva_start[case_nr] = {0x40000000,
			VPU_RESET_VECTOR_FIX_MVA_END,
			0x50080000, VPU_FIX_MVA_END, 0x500FFFFF};
	unsigned int test_mva_end[case_nr] = {
		VPU_RESET_VECTOR_FIX_MVA_START, 0x5FFFFFFF,
		VPU_FIX_MVA_START, 0xFFFFFFFF, VPU_FIX_MVA_START};
	unsigned int size, size_aligned;
	unsigned int nr;
	unsigned int startIdx;
	struct m4u_buf_info_t tmp;
	struct m4u_buf_info_t *pMvaInfo = &tmp;

	pMvaInfo->port = M4U_VPU_PORT_NAME;/*M4U_PORT_DISP_OVL0, M4U_PORT_VPU*/

	M4UMSG(
		"%s========================>\n", __func__);
	/*test case that input range intersects to vpu.
	 *should print config error.
	 */
	/*4 cases:
	 *   |--------------|------|---------|------------|----------|
	 *  0x0           0x500  0x501      0x600        0x800      0xFFF
	 *case 1:   start   |  end
	 *case 2:             start|   end
	 *case 3:                      start |   end
	 *case 4:                                 start   |  end
	 */
	M4UMSG(
	"<*test case that input range intersects to vpu: =>should print config error.*>\n");
	for (i = 0; i < case_nr; i++) {
		size = test_mva_end[i] - test_mva_start[i] + 1;
		size_aligned =
			GET_RANGE_SIZE(START_ALIGNED(test_mva_start[i]),
			END_ALIGNED(test_mva_start[i], size));
		nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
		startIdx = MVAGRAPH_INDEX(test_mva_start[i]);
		ret = m4u_check_mva_region(startIdx, nr, pMvaInfo);
		verify_ret(ret, test_mva_start[i], test_mva_end[i]);
	}

	/*test case: input range is in the vpu region
	 *case 1: in and port id is rgiht. should print belongs to vpu.
	 *case 2: in and port id isn's rgiht. should print config error.
	 */
	M4UMSG(
	"<*test case: input range is in the vpu region: =>should print in vpu region & config error*>\n");
	test_mva_start[0] = VPU_RESET_VECTOR_FIX_MVA_START;
	test_mva_end[0] = VPU_RESET_VECTOR_FIX_MVA_END;
	test_mva_start[1] = VPU_FIX_MVA_START;
	test_mva_end[1] = VPU_FIX_MVA_END;
	for (i = 0; i < 2; i++) {
		if (i == 0)
			pMvaInfo->port = M4U_VPU_PORT_NAME;
		else
			pMvaInfo->port = M4U_PORT_DISP_OVL0;
		for (j = 0; j < 2; j++) {
			size = test_mva_end[j] - test_mva_start[j] + 1;
			size_aligned =
				GET_RANGE_SIZE(START_ALIGNED(test_mva_start[j]),
				END_ALIGNED(test_mva_start[j], size));
			nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
			startIdx = MVAGRAPH_INDEX(test_mva_start[j]);
			ret = m4u_check_mva_region(startIdx, nr, pMvaInfo);
			verify_ret(ret, test_mva_start[j], test_mva_end[j]);
		}
	}
	/*test case: input range is out of vpu region.*/
	M4UMSG(
	"<*test case: input range is out of vpu region: =>should print out of vpu region*>\n");
	test_mva_start[0] = 0x100000;
	test_mva_end[0] = 0x4FFFFFFF;
	/*0x50080000 is out of vpu mva region, but in the vpu mvaGraph,
	 *we think input range intersects to vpu region
	 */
	test_mva_start[1] = 0x50100000;
	test_mva_end[1] = 0x5FFFFFFF;
	test_mva_start[2] = 0x80000000;
	test_mva_end[2] = 0x8FFFFFFF;
	for (i = 0; i < 3; i++) {
		size = test_mva_end[i] - test_mva_start[i] + 1;
		size_aligned =
			GET_RANGE_SIZE(START_ALIGNED(test_mva_start[i]),
			END_ALIGNED(test_mva_start[i], size));
		nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
		startIdx = MVAGRAPH_INDEX(test_mva_start[i]);
		ret = m4u_check_mva_region(startIdx, nr, pMvaInfo);
		verify_ret(ret, test_mva_start[i], test_mva_end[i]);
	}
}

#define REGION_NR 15
#define IN_INDEX_START 5
#define IN_INDEX_END 6
#define INTERSECT_INDEX_START 0
#define INTERSECT_INDEX_END 4
#define OUT_INDEX_START 7
#define OUT_INDEX_END 9
#define SUB_INDEX_START 10
#define SUB_INDEX_END 14

/*0-3: intersect to vpu fix region.
 *4-5: in vpu fix region
 *6-8: out of vpu fix region
 */
unsigned int gtest_mva_start[REGION_NR] = {0x40000000,
	0x5007FFFF, 0x50080000, VPU_FIX_MVA_END, 0x500FFFFF,
						0x50000000, 0x60000000,
						0x40000000, 0x50100000,
						0x80000000, 0x60000000,
						0x60f00000, 0x62100000,
						0x70100000, 0x7c000000};
unsigned int gtest_mva_end[REGION_NR] = {0x50000000,
			0x5FFFFFFF, 0x60000000, 0xFFFFFFFF, 0x60000000,
						0x5007FFFF, VPU_FIX_MVA_END,
						0x4FFFFFFF, 0x5FFFFFFF,
						0x8FFFFFFF, 0x60e00000,
						0x62000000, 0x62f00000,
						0x7b000000, VPU_FIX_MVA_END};
typedef unsigned int (*mva_alloc_from_fix_region)(
		unsigned long, unsigned int, unsigned int, void *);

static void verify_m4u_do_mva_alloc_fake(unsigned int port_id)
{
	int i = 0;
	unsigned int size, size_aligned;
	unsigned int nr;
	unsigned int startIdx;
	struct m4u_buf_info_t tmp;
	struct m4u_buf_info_t *pMvaInfo = &tmp;
	unsigned int ret = 0;

	pMvaInfo->port = port_id;/*M4U_PORT_DISP_OVL0, M4U_PORT_VPU*/

	for (; i < REGION_NR; i++) {
		size = gtest_mva_end[i] - gtest_mva_start[i] + 1;
		M4UMSG("test %dth region [0x%x, 0x%x].\n",
				i, gtest_mva_start[i], gtest_mva_end[i]);
		size_aligned =
			GET_RANGE_SIZE(START_ALIGNED(gtest_mva_start[i]),
			END_ALIGNED(gtest_mva_start[i], size));
		nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
		startIdx = MVAGRAPH_INDEX(gtest_mva_start[i]);
		ret = m4u_check_mva_region(startIdx, nr, pMvaInfo);
		if (ret)
			M4UMSG(
			"%dth buf port: %d m4u_do_mva_alloc region [0x%x, 0x%x] failed.\n",
				i, pMvaInfo->port,
				gtest_mva_start[i], gtest_mva_end[i]);
		else
			M4UMSG(
			"%dth buf port: %d m4u_do_mva_alloc region [0x%x, 0x%x] success.\n",
				i, pMvaInfo->port,
				gtest_mva_start[i], gtest_mva_end[i]);
	}
}
void test_case_m4u_do_mva_alloc(void)
{
	M4UMSG(
		"%s========================>\n", __func__);
	/*since we are only care that if input mva
	 *region is in or intersects vpu fix region.
	 *if each of the 2 cases occur, m4u_do_mva_alloc will failed.
	 *so, it's ok to only test if m4u_check_mva_region return 0.
	 */

	/*vpu port should not m4u_do_mva_alloc vpu fix region,
	 *but it can m4u_do_mva_alloc non-vpu fix region.
	 */
	M4UMSG(
	"<*test case: if vpu want to alloc vpu fix region,");
	M4UMSG(
		"it should not use m4u_do_mva_alloc: => 7th&9th buffer should print success*>\n");
	verify_m4u_do_mva_alloc_fake(M4U_VPU_PORT_NAME);

	/*non-vpu port can m4u_do_mva_alloc non-vpu fix region.
	 *the 7th & 9th buffer should print success. others should print failed
	 */
	M4UMSG(
	"<*test case: non-vpu port can m4u_do_mva_alloc non-vpu fix region:");
	M4UMSG(
		"=>7th&9th buffer should print success*>\n");
	verify_m4u_do_mva_alloc_fake(M4U_PORT_DISP_OVL0);
}

/*m4u_do_mva_alloc_fix should notice the following:
 *(1) vpu fix region is protected by m4u_check_mva_region.
 *(2) if vpu port alloc vpu fix region by this, it must make sure reserved bit
 *    mustn't be destroyed.
 *(3)There is no protection to non-vpu fix region now. if other fix regions are
 *taken by some ports, only m4u user can check this.
 *(4)because formal parameter [mva] == fix mva region start,
 *va + size - 1 should be
 *not more than fix mva region end.
 */
static void verify_m4u_do_mva_alloc_fix(unsigned int test_region_start,
					unsigned int test_region_end,
					unsigned int port_id,
					mva_alloc_from_fix_region callback,
					struct m4u_buf_info_t *pinfo_array[])
{
	unsigned int va_offset = 0xff, actual_mva_start;
	int i = 0;
	unsigned int size, size_aligned;
	unsigned int nr;
	unsigned int startIdx;
	unsigned int ret = 0;

	for (i = test_region_start; i <= test_region_end; i++) {
		pinfo_array[i] = vmalloc(sizeof(struct m4u_buf_info_t));
		/*M4U_PORT_DISP_OVL0, M4U_PORT_VPU*/
		pinfo_array[i]->port = port_id;

		/*because formal parameter of
		 *m4u_do_mva_alloc_fix [mva] == fix mva region start,
		 *va + size - 1 should be not more than fix mva region end.
		 */
		actual_mva_start = gtest_mva_start[i] + va_offset;
		size = gtest_mva_end[i] - actual_mva_start + 1;
		M4UMSG("test region [0x%x, 0x%x].\n",
			actual_mva_start, gtest_mva_end[i]);

		ret = callback(va_offset,
			gtest_mva_start[i], size, pinfo_array[i]);
		if (!ret) {
			M4UMSG(
				"port: %d m4u_do_mva_alloc_fix region [0x%x, 0x%x] failed.\n",
				pinfo_array[i]->port,
				actual_mva_start, gtest_mva_end[i]);
			vfree(pinfo_array[i]);
		} else if (ret == actual_mva_start)
			M4UMSG(
			"port: %d m4u_do_mva_alloc_fix region [0x%x, 0x%x] success.\n",
				pinfo_array[i]->port,
				actual_mva_start, gtest_mva_end[i]);
		else {
			M4UMSG(
				"port: %d m4u_do_mva_alloc_fix region [0x%x, 0x%x].",
				pinfo_array[i]->port,
				actual_mva_start, gtest_mva_end[i]);
			M4UMSG(
				"returned 0x%x is not equal to expection one(0x%x).\n",
				ret, actual_mva_start);
			m4u_do_mva_free(actual_mva_start, size);
			vfree(pinfo_array[i]);
		}

		size_aligned = GET_RANGE_SIZE(START_ALIGNED(actual_mva_start),
				END_ALIGNED(actual_mva_start, size));
		nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
		startIdx = MVAGRAPH_INDEX(actual_mva_start);
		ret = check_reserved_region_integrity(startIdx, nr);
		if (ret)
			M4UMSG("check [0x%x - 0x%x] integrity pass\n",
				startIdx, GET_END_INDEX(startIdx, nr));
		else
			M4UMSG("check [0x%x - 0x%x] integrity failed\n",
				startIdx, GET_END_INDEX(startIdx, nr));
	}
}

unsigned int gtest_mva_start_normal[REGION_NR] = {0};
unsigned int gtest_mva_end_normal[REGION_NR] = {0};
static void verify_m4u_do_mva_alloc(unsigned int test_region_start,
					unsigned int test_region_end,
					unsigned int port_id,
					struct m4u_buf_info_t *pinfo_array[])
{
	unsigned int va_offset = 0xff, actual_mva_start;
	unsigned int size;
	unsigned int i;
	unsigned int ret = 0;

	for (i = test_region_start; i <= test_region_end; i++) {
		pinfo_array[i] = vmalloc(
			sizeof(struct m4u_buf_info_t));
		/*M4U_PORT_DISP_OVL0, M4U_PORT_VPU*/
		pinfo_array[i]->port = port_id;
		size = gtest_mva_end[i] - gtest_mva_start[i] + 1;
		actual_mva_start = gtest_mva_start[i] | va_offset;
		ret = m4u_do_mva_alloc(va_offset, size, (void *)pinfo_array[i]);
		if (!ret) {
			M4UMSG("port: %d m4u_do_mva_alloc 0x%x bytes failed.\n",
				pinfo_array[i]->port, size);
			vfree(pinfo_array[i]);
		} else {
			M4UMSG(
				"port: %d m4u_do_mva_alloc  0x%x bytes in [0x%x, 0x%x] success.\n",
				pinfo_array[i]->port, size,
				ret, (ret + size - 1));
			gtest_mva_start_normal[i] = ret;
			gtest_mva_end_normal[i] = ret + size - 1;
		}
	}

}

/*verify_m4u_do_mva_free only free non-vpu region.
 *it doesn't need integrity-checking.
 */
static void verify_m4u_do_mva_free(unsigned int test_region_start,
		unsigned int test_region_end,
		struct m4u_buf_info_t *pinfo_array[])
{
	unsigned int size;
	unsigned int i;
	unsigned int ret = 0;

	for (i = test_region_start; i <= test_region_end; i++) {
		M4UMSG("free region [0x%x, 0x%x].\n",
			gtest_mva_start_normal[i],
			gtest_mva_end_normal[i]);
		size = gtest_mva_end_normal[i] - gtest_mva_start_normal[i] + 1;
		ret = m4u_do_mva_free(gtest_mva_start_normal[i], size);
		if (ret)
			M4UMSG("m4u_do_mva_free region [0x%x, 0x%x] failed.\n",
				gtest_mva_start_normal[i],
				gtest_mva_end_normal[i]);
		else
			M4UMSG("m4u_do_mva_free region [0x%x, 0x%x] success.\n",
				gtest_mva_start_normal[i],
				gtest_mva_end_normal[i]);
		vfree(pinfo_array[i]);
	}
}

/*free vpu region. need to check integrity*/
static void verify_m4u_do_mva_free_fix(unsigned int test_region_start,
		unsigned int test_region_end,
		struct m4u_buf_info_t *pinfo_array[])
{
	unsigned int va_offset = 0xff, actual_mva_start;
	unsigned int size, size_aligned, nr, startIdx;
	unsigned int i;
	unsigned int ret = 0;

	for (i = test_region_start; i <= test_region_end; i++) {
		actual_mva_start = gtest_mva_start[i] + va_offset;

		M4UMSG("free region [0x%x, 0x%x].\n",
				actual_mva_start, gtest_mva_end[i]);

		size = gtest_mva_end[i] - actual_mva_start + 1;
		ret = m4u_do_mva_free(actual_mva_start, size);
		if (ret)
			M4UMSG("m4u_do_mva_free region [0x%x, 0x%x] failed.\n",
				actual_mva_start, gtest_mva_end[i]);
		else
			M4UMSG("m4u_do_mva_free region [0x%x, 0x%x] success.\n",
				actual_mva_start, gtest_mva_end[i]);
		vfree(pinfo_array[i]);

		size_aligned = GET_RANGE_SIZE(START_ALIGNED(actual_mva_start),
				END_ALIGNED(actual_mva_start, size));
		nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size_aligned);
		startIdx = MVAGRAPH_INDEX(actual_mva_start);
		ret = check_reserved_region_integrity(startIdx, nr);
		if (ret)
			M4UMSG("check [0x%x - 0x%x] integrity pass\n",
				startIdx, GET_END_INDEX(startIdx, nr));
		else
			M4UMSG("check [0x%x - 0x%x] integrity failed\n",
				startIdx, GET_END_INDEX(startIdx, nr));
	}
}

/*allocate 5-9 success. free 5-9 success & check integrity pass*/
void test_case_m4u_do_mva_alloc_fix(void)
{
	struct m4u_buf_info_t **p_info_array0 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(IN_INDEX_END - IN_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array1 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(IN_INDEX_END - IN_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array2 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(OUT_INDEX_END - OUT_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array3 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(INTERSECT_INDEX_END - INTERSECT_INDEX_START + 1));
	if (p_info_array0 == NULL || p_info_array1 == NULL ||
		p_info_array2 == NULL || p_info_array3 == NULL)
		M4UMSG("p_info_array == NULL");
	else {

		m4u_mvaGraph_dump();
		M4UMSG(
			"%s========================>\n", __func__);
		M4UMSG(
			"<*test case: CANNOT m4u_do_mva_alloc_fix vpu fix region by non-vpu port:");
		M4UMSG(
			"=> allocation print failed & integrity must pass*>\n");
		verify_m4u_do_mva_alloc_fix(
			IN_INDEX_START, IN_INDEX_END,
			M4U_PORT_DISP_OVL0,
			m4u_do_mva_alloc_fix, p_info_array0);

		M4UMSG(
			"<*test case: it's ok to m4u_do_mva_alloc_fix vpu fix region by vpu port:");
		M4UMSG(
			"=> allocation print success & integrity must pass*>\n");
		verify_m4u_do_mva_alloc_fix(
			IN_INDEX_START, IN_INDEX_END,
			M4U_VPU_PORT_NAME,
			m4u_do_mva_alloc_fix, p_info_array1);

		M4UMSG(
			"<*test case: it's ok to m4u_do_mva_alloc_fix non-vpu fix region by vpu port:");
		M4UMSG(
			"=> allocation print success & integrity is no requeired*>\n");
		verify_m4u_do_mva_alloc_fix(
			OUT_INDEX_START, OUT_INDEX_END,
			M4U_VPU_PORT_NAME,
			m4u_do_mva_alloc_fix, p_info_array2);

		M4UMSG(
			"<*test case: CANNOT m4u_do_mva_alloc_fix the one intersecting vpu fix region:");
		M4UMSG(
			"=> allocation print failed & integrity is no requeired*>\n");
		verify_m4u_do_mva_alloc_fix(
			INTERSECT_INDEX_START, INTERSECT_INDEX_END,
			M4U_VPU_PORT_NAME, m4u_do_mva_alloc_fix, p_info_array3);

		M4UMSG("free all allocated mva.\n");
		verify_m4u_do_mva_free_fix(IN_INDEX_START,
			IN_INDEX_END, p_info_array1);
		verify_m4u_do_mva_free_fix(OUT_INDEX_START,
			OUT_INDEX_END, p_info_array2);
	}
	vfree(p_info_array0);
	vfree(p_info_array1);
	vfree(p_info_array2);
	vfree(p_info_array3);
	m4u_mvaGraph_dump();
}

void test_case_m4u_do_mva_alloc_start_from(void)
{
	struct m4u_buf_info_t **p_info_array0 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(IN_INDEX_END - IN_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array1 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(IN_INDEX_END - IN_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array2 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(OUT_INDEX_END - OUT_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array3 =
			vmalloc(sizeof(struct m4u_buf_info_t **) *
			(INTERSECT_INDEX_END - INTERSECT_INDEX_START + 1));

	M4UMSG(
		"test_case_m4u_do_mva_alloc_fix========================>\n");
	if (p_info_array0 == NULL || p_info_array1 == NULL ||
		p_info_array2 == NULL || p_info_array3 == NULL)
		M4UMSG("p_info_array == NULL");
	else {
		M4UMSG(
			"<*test case: CANNOT m4u_do_mva_alloc_fix vpu fix region by non-vpu port:");
		M4UMSG(
			"=> allocation print failed & integrity must pass*>\n");
		verify_m4u_do_mva_alloc_fix(
			IN_INDEX_START, IN_INDEX_END, M4U_PORT_DISP_OVL0,
			m4u_do_mva_alloc_start_from, p_info_array0);

		M4UMSG(
			"<*test case: it's ok to m4u_do_mva_alloc_fix vpu fix region by vpu port:");
		M4UMSG(
			"=> allocation print success & integrity must pass*>\n");
		verify_m4u_do_mva_alloc_fix(
			IN_INDEX_START, IN_INDEX_END,
			M4U_VPU_PORT_NAME,
			m4u_do_mva_alloc_start_from,
			p_info_array1);

		M4UMSG(
			"<*test case: it's ok to m4u_do_mva_alloc_fix non-vpu fix region by vpu port:");
		M4UMSG(
			"=> allocation print success & integrity is no requeired*>\n");
		verify_m4u_do_mva_alloc_fix(
			OUT_INDEX_START, OUT_INDEX_END, M4U_VPU_PORT_NAME,
			m4u_do_mva_alloc_start_from, p_info_array2);

		M4UMSG(
			"<*test case: CANNOT m4u_do_mva_alloc_fix the one intersecting vpu fix region:");
		M4UMSG(
			"=> allocation print failed & integrity is no requeired*>\n");
		verify_m4u_do_mva_alloc_fix(
			INTERSECT_INDEX_START, INTERSECT_INDEX_END,
			M4U_VPU_PORT_NAME,
			m4u_do_mva_alloc_start_from, p_info_array3);

		M4UMSG("free all allocated mva.\n");
		verify_m4u_do_mva_free_fix(IN_INDEX_START,
			IN_INDEX_END, p_info_array1);
		verify_m4u_do_mva_free_fix(OUT_INDEX_START,
			OUT_INDEX_END, p_info_array1);
	}
	vfree(p_info_array0);
	vfree(p_info_array1);
	vfree(p_info_array2);
	vfree(p_info_array3);
}


/*(1)make sure m4u_do_mva_free non-vpu fix region or vpu fix region is passed.
 *(2)non-vpu port should not free vpu fix region.
 */
void test_case_m4u_do_mva_free(void)
{

	struct m4u_buf_info_t **p_info_array0 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(OUT_INDEX_START - OUT_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array1 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(IN_INDEX_END - IN_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array2 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(OUT_INDEX_END - OUT_INDEX_START + 1));
	struct m4u_buf_info_t **p_info_array3 =
		vmalloc(sizeof(struct m4u_buf_info_t **) *
		(SUB_INDEX_END - SUB_INDEX_START + 1));
	if (p_info_array0 == NULL || p_info_array1 == NULL ||
		p_info_array2 == NULL || p_info_array3 == NULL)
		M4UMSG("p_info_array == NULL");
	else {
		M4UMSG(
			"%s========================>\n",
			__func__);
		m4u_mvaGraph_dump();
		M4UMSG(
			"<*test case: non vpu port m4u_do_mva_alloc non-vpu region => print pass*>\n");
		/*non vpu port m4u_do_mva_alloc non-vpu region*/
		verify_m4u_do_mva_alloc(OUT_INDEX_START,
		OUT_INDEX_START, M4U_PORT_DISP_OVL0, p_info_array0);
		m4u_mvaGraph_dump();
		/*free non-vpu fix region*/
		M4UMSG(
		"<*test case: free non-vpu fix region allocated by m4u_do_mva_alloc => print pass*>\n");
		verify_m4u_do_mva_free(OUT_INDEX_START,
			OUT_INDEX_START, p_info_array0);
		m4u_mvaGraph_dump();

		/*use m4u_do_mva_alloc_start_from to alloc
		 *sub range in vpu fix region. and then free.
		 */
		M4UMSG(
		"<*test case: non-vpu port verify_m4u_do_mva_alloc_fix non-vpu fix region => print pass*>\n");
		verify_m4u_do_mva_alloc_fix(SUB_INDEX_START, SUB_INDEX_END,
			M4U_VPU_PORT_NAME,
						m4u_do_mva_alloc_start_from,
						p_info_array3);
		m4u_mvaGraph_dump();
		/*free sub-vpu fix region*/
		M4UMSG(
		"<*test case: free non-vpu fix region allocated by m4u_do_mva_alloc_fix => print pass*>\n");
		verify_m4u_do_mva_free_fix(SUB_INDEX_START,
			SUB_INDEX_END, p_info_array3);
		m4u_mvaGraph_dump();
	}
	vfree(p_info_array0);
	vfree(p_info_array1);
	vfree(p_info_array2);
	vfree(p_info_array3);
}

void test_dummy(void)
{
	int i = 0;
	unsigned int size;
	unsigned int nr;
	struct m4u_buf_info_t tmp;
	struct m4u_buf_info_t *pMvaInfo = &tmp;
	unsigned int ret = 0;
	unsigned int startIdx;

	/*M4U_PORT_DISP_OVL0, M4U_PORT_VPU*/
	pMvaInfo->port = M4U_VPU_PORT_NAME;
	size = gtest_mva_end[0] - gtest_mva_start[0] + 1;
	M4UMSG("test %dth region [0x%x, 0x%x].\n",
			i, gtest_mva_start[0], gtest_mva_end[0]);
	nr = MVA_GRAPH_BLOCK_NR_ALIGNED(size);
	startIdx = MVAGRAPH_INDEX(gtest_mva_start[0]);
	ret = m4u_check_mva_region(gtest_mva_start[0], nr, pMvaInfo);
	if (ret)
		M4UMSG(
		"%dth buf m4u_do_mva_alloc region [0x%x, 0x%x] failed.\n",
			0, gtest_mva_start[0], gtest_mva_end[0]);
	else
		M4UMSG(
		"%dth buf m4u_do_mva_alloc region [0x%x, 0x%x] success.\n",
			0, gtest_mva_start[0], gtest_mva_end[0]);
}

void test_m4u_do_mva_alloc_stage3(void)
{
	int last_free_index_in_stage1, size0, size1, size2, nr;
	struct m4u_buf_info_t *pinfo;
	unsigned int result_mva0, result_mva1, result_mva2;

	M4UMSG("start to %s\n", __func__);
	pinfo = vmalloc(sizeof(struct m4u_buf_info_t));
	if (pinfo == NULL) {
		M4UMSG("pinfo == NULL\n");
		vfree(pinfo);
		return;
	}
	pinfo->port = 0;
	last_free_index_in_stage1 =
		get_last_free_graph_idx_in_stage1_region();
	M4UMSG("cur_first_index = 0x%x\n",
		last_free_index_in_stage1);
	if (last_free_index_in_stage1 <
		MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START)) {
		nr = MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START) -
			1 - last_free_index_in_stage1 + 1;
		size0 = MVA_GRAPH_NR_TO_SIZE(nr);
		/*stage 1*/
		result_mva0 = m4u_do_mva_alloc(0, size0, pinfo);
		M4UMSG(
			"allocated all remained free blocks in stage 1, result_mva = 0x%x size = 0x%x\n",
			result_mva0, size0);

		nr = MVAGRAPH_INDEX(VPU_FIX_MVA_START) - 1
			- MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_END);
		size1 = MVA_GRAPH_NR_TO_SIZE(nr);
		/*stage 2*/
		result_mva1 = m4u_do_mva_alloc(0, size1, pinfo);
		M4UMSG(
			"allocated all remained free blocks in stage 2, result_mva = 0x%x size = 0x%x\n",
			result_mva1, size1);
		/*stage 3*/
		size2 = 0x5000000;
		result_mva2 = m4u_do_mva_alloc(0, size2, pinfo);
		if (result_mva2 <= VPU_FIX_MVA_END)
			M4UMSG(
			"result_mva = 0x%x is not in stage 3, case failed!\n",
			result_mva2);
		else
			M4UMSG(
			"result_mva = 0x%x is in stage 3, case succed!\n",
			result_mva2);
		m4u_mvaGraph_dump();
		m4u_do_mva_free(result_mva0, size0);
		m4u_do_mva_free(result_mva1, size1);
		m4u_do_mva_free(result_mva2, size2);
		m4u_mvaGraph_dump();
	} else
		M4UMSG(
		"after just boot, mva should not be allocated in stage 3!\n");
	vfree(pinfo);
}

void test_m4u_do_mva_alloc_start_from_V2p4(void)
{
	int last_free_index_in_stage1, size0, size1, size2, nr;
	struct m4u_buf_info_t *pinfo;
	unsigned int result_mva0, result_mva1, result_mva2;

	M4UMSG("start to test_m4u_do_mva_alloc_start_from_V2_4\n");
	pinfo = vmalloc(sizeof(struct m4u_buf_info_t));
	if (!pinfo) {
		M4UMSG("alloc info fail\n");
		return;
	}
	pinfo->port = 0;
	last_free_index_in_stage1 = get_last_free_graph_idx_in_stage1_region();

	if (last_free_index_in_stage1 <
			MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START)) {
		nr = MVAGRAPH_INDEX(VPU_RESET_VECTOR_FIX_MVA_START) -
			1 - last_free_index_in_stage1;
		size0 = MVA_GRAPH_NR_TO_SIZE(nr);
		M4UMSG(
			"alloc mva in stage 1, port=%d  index = 0x%x size = 0x%x\n",
			pinfo->port, last_free_index_in_stage1, size0);
		result_mva0 = m4u_do_mva_alloc(0, size0, pinfo);
		m4u_mvaGraph_dump();
		M4UMSG("alloc mva in stage 1, result_mva0 = 0x%x\n",
			result_mva0);

		pinfo->port = M4U_VPU_PORT_NAME;
		size1 = 0x12000000;
		M4UMSG("alloc mva in stage 2, port=%d, size = 0x%x\n",
			pinfo->port, size1);
		result_mva1 = m4u_do_mva_alloc_start_from(0,
			0x12000000, size1, pinfo);
		m4u_mvaGraph_dump();
		M4UMSG("alloc mva in stage 2, result_mva1 = 0x%x\n",
			result_mva1);

		pinfo->port = 0;
		size2 = 0x20000000;
		M4UMSG("alloc mva in stage 3, size = 0x%x\n", size2);
		result_mva2 = m4u_do_mva_alloc_start_from(0, 0x10000000,
			size2, pinfo);
		m4u_mvaGraph_dump();
		M4UMSG("alloc mva in stage 3, result_mva2 = 0x%x\n",
			result_mva2);

		pinfo->port = M4U_VPU_PORT_NAME;
		m4u_mvaGraph_dump();
		m4u_do_mva_free(result_mva0, size0);
		m4u_do_mva_free(result_mva1, size1);
		m4u_do_mva_free(result_mva2, size2);
		m4u_mvaGraph_dump();
	} else
		M4UMSG(
		"after just boot, mva should not be allocated out of stage 1!\n");
	vfree(pinfo);
}

void test_m4u_do_mva_alloc_start_from_V2p4_case1(void)
{
	int first_free_idx, size = 0x4000000;
	struct m4u_buf_info_t *pinfo;
	int i;
	unsigned int *ret_mva =
		kzalloc(sizeof(unsigned int) * 4096, GFP_KERNEL);

	M4UMSG(
		"start to %s\n", __func__);
	pinfo = vmalloc(sizeof(struct m4u_buf_info_t));

	if (pinfo == NULL) {
		M4UMSG("pinfo == NULL\n");
		vfree(pinfo);
		kfree(ret_mva);
		return;
	}
	pinfo->port = 0;
	first_free_idx = get_first_free_idx();
	M4UMSG("get 1st free index = 0x%x\n", first_free_idx);
	/*alloc non-vpu region*/
	for (i = 0; i < 4096; i++) {
		*(ret_mva + i) = m4u_do_mva_alloc_start_from(0,
			(first_free_idx << 20), size, pinfo);
		if (!*(ret_mva + i)) {
			M4UMSG(
				"non vpu region has been allocated over, the last valid %d.\n",
				i-1);
			break;
		}
	}
	m4u_mvaGraph_dump();
	/*alloc vpu region*/
	pinfo->port = M4U_VPU_PORT_NAME;
	for (; i < 4096; i++) {
		*(ret_mva + i) = m4u_do_mva_alloc_start_from(0,
			0x60000000, size, pinfo);
		if (!*(ret_mva + i)) {
			M4UMSG(
				"vpu region has been allocated over, the last valid %d.\n",
				i-1);
			break;
		}
	}
	m4u_mvaGraph_dump();
	/*free all*/
	for (i = i - 1 ; i >= 0; i--) {
		if (*(ret_mva + i))
			m4u_do_mva_free(*(ret_mva + i), size);
	}
	m4u_mvaGraph_dump();
	vfree(pinfo);
	kfree(ret_mva);
}

