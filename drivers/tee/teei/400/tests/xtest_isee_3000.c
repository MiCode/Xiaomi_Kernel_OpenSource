// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/cacheflush.h>

#define IMSG_TAG "[tz_test_drv]"
#include <imsg_log.h>
#include "ta_core_api.h"

#define DRM_04_UUID \
	{ 0x09010000, 0x0000, 0x0000, \
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }
struct TEEC_Context xtest_isee_ctx_3000;
const struct TEEC_UUID isee_test_ta_uuid_3000 = DRM_04_UUID;

#define Cache_line_size 32
static inline void Flush_Dcache_By_Area(unsigned long start, unsigned long end)
{
#ifdef CONFIG_ARM64
		__flush_dcache_area((void *)start, (end - start));
#else
		__cpuc_flush_dcache_area((void *)start, (end - start));
#endif
}
static inline void Invalidate_Dcache_By_Area(unsigned long start,
						unsigned long end)
{
#ifdef CONFIG_ARM64
	uint64_t temp[2];

	temp[0] = start;
	temp[1] = end;
	__asm__ volatile(
		"ldr x0, [%[temp], #0]\n\t"
		"ldr x1, [%[temp], #8]\n\t"
		"mrs    x3, ctr_el0\n\t"
		"ubfm   x3, x3, #16, #19\n\t"
		"mov	x2, #4\n\t"
		"lsl	x2, x2, x3\n\t"
		"dsb	sy\n\t"
		"sub	x3, x2, #1\n\t"
		"bic	x0, x0, x3\n\t"
		/* invalidate D line / unified line */
		"1:	dc      ivac, x0\n\t"
		"add	x0, x0, x2\n\t"
		"cmp	x0, x1\n\t"
		"b.lo	1b\n\t"
		"dsb	sy\n\t"
		: :
		[temp] "r" (temp)
		: "x0", "x1", "x2", "x3", "memory");
#else
	__asm__ __volatile__ ("dsb" : : : "memory"); /* dsb */
	__asm__ __volatile__ (
		/* Invalidate Data Cache Line (using MVA) Register */
		"1:  mcr p15, 0, %[i], c7, c6, 1\n"
		"    add %[i], %[i], %[clsz]\n"
		"    cmp %[i], %[end]\n"
		"    blo 1b\n"
		:
		[i]    "=&r" (start)
		:      "0"   ((unsigned long)start & (~(Cache_line_size - 1))),
		[end]  "r"   (end),
		[clsz] "i"   (Cache_line_size)
		: "memory");

	/* invalidate btc */
	asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory");
	__asm__ __volatile__ ("dsb" : : : "memory"); /* dsb */

#endif
}

int xtest_isee_test_3030(void)
{
	uint32_t TEMP_BUFFER_TEST_SIZE = 4096;
	unsigned int res;
	struct TEEC_Session session = {0};
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.params[1].value.a = TEMP_BUFFER_TEST_SIZE;
	op.params[1].value.b = 0;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_NONE,
					TEEC_NONE);
	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
			DRV_CORE_API_CMD_MSEE_CLEAN_INVALIDATE_DCACHE_RANGE,
			&op, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_3040(void)
{
	uint32_t TEMP_BUFFER_TEST_SIZE = 4096;
	unsigned int res;
	struct TEEC_Session session = {0};
	unsigned char *temp_buffer = 0;
	uint64_t temp_buffer_paddr = 0;
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;
	uint32_t x[2];
	unsigned long addr_start;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)tz_malloc(TEMP_BUFFER_TEST_SIZE);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto buffer_alloc_failed;
	}

	MEMSET_UINT32(temp_buffer, TA_CORE_API_PARAM_TYPE_Test_PatternF,
				TEMP_BUFFER_TEST_SIZE);
	addr_start = (unsigned long)(temp_buffer);
	Flush_Dcache_By_Area(addr_start, addr_start+TEMP_BUFFER_TEST_SIZE);
	temp_buffer_paddr = virt_to_phys((void *)temp_buffer);
	memcpy(x, &temp_buffer_paddr, sizeof(temp_buffer_paddr));
	op.params[0].value.a = x[0];
	op.params[0].value.b = x[1];
	op.params[1].value.a = TEMP_BUFFER_TEST_SIZE;
	op.params[1].value.b = 0;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_NONE,
					TEEC_NONE);
	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
				__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
				__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
				__func__, TEMP_BUFFER_TEST_SIZE);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
				__func__, TEMP_BUFFER_TEST_SIZE);
	if (!ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				DRV_CORE_API_CMD_MSEE_CLEAN_DCACHE_RANGE,
				&op, &ret_orig)))
		goto buffer_alloc_failed;

	Invalidate_Dcache_By_Area(addr_start,
				addr_start + TEMP_BUFFER_TEST_SIZE);
	if (!ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
					ARRAY_MATCH_PATTERN(temp_buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern5,
					TEMP_BUFFER_TEST_SIZE)))
		case_res = DRV_ERROR_MSEE_CLEAN_DCACHE_RANGE;

buffer_alloc_failed:
	TEEC_CloseSession(&session);
	if (temp_buffer)
		tz_free(temp_buffer, TEMP_BUFFER_TEST_SIZE);
	return case_res;
}

int xtest_isee_test_3050(void)
{
	unsigned int res;
	struct TEEC_Session session = {0};
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	/*
	 * 1. map register 0x08000000
	 * 2. read offset 0x0, 0x4, 0x8, 0xC
	 */
	if (!ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				DRV_CORE_API_CMD_MSEE_MMAP_REGION_TEST1,
				&op, &ret_orig)))
		return DRV_ERROR_MSEE_MAP_REGION_TEST1;

	case_res = TEEC_SUCCESS;

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_3051(void)
{
	uint32_t TEMP_BUFFER_TEST_SIZE = 4096;
	unsigned int res;
	struct TEEC_Session session = {0};
	unsigned char *temp_buffer = 0;
	uint64_t temp_buffer_paddr = 0;
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;
	uint32_t x[2];
	unsigned long addr_start;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)tz_malloc(TEMP_BUFFER_TEST_SIZE);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto exit;
	}

	/*
	 * 1. kernel CA alloc memory and write data
	 * 2. pass physical address to ta/drv
	 * 3. map read only
	 * 4. compare data
	 */
	MEMSET_UINT32(temp_buffer, TA_CORE_API_PARAM_TYPE_Test_PatternF,
			TEMP_BUFFER_TEST_SIZE);
	addr_start = (unsigned long)(temp_buffer);
	Flush_Dcache_By_Area(addr_start, addr_start+TEMP_BUFFER_TEST_SIZE);
	memset(&op, 0, sizeof(op));
	temp_buffer_paddr = virt_to_phys((void *)temp_buffer);
	memcpy(x, &temp_buffer_paddr, sizeof(temp_buffer_paddr));
	op.params[0].value.a = x[0];
	op.params[0].value.b = x[1];
	op.params[1].value.a = TEMP_BUFFER_TEST_SIZE;
	op.params[1].value.b = 0;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_NONE,
					TEEC_NONE);

	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
				__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
				__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
				__func__, TEMP_BUFFER_TEST_SIZE);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
				__func__, TEMP_BUFFER_TEST_SIZE);

	if (!ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				DRV_CORE_API_CMD_MSEE_MMAP_REGION_TEST2,
				&op, &ret_orig)))
		goto exit;

	case_res = TEEC_SUCCESS;
exit:
	TEEC_CloseSession(&session);
	if (temp_buffer)
		tz_free(temp_buffer, TEMP_BUFFER_TEST_SIZE);
	return case_res;
}

int xtest_isee_test_3052(void)
{
	uint32_t TEMP_BUFFER_TEST_SIZE = 4096;
	unsigned int res;
	struct TEEC_Session session = {0};
	unsigned char *temp_buffer = 0;
	uint64_t temp_buffer_paddr = 0;
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;
	uint32_t x[2];
	unsigned long addr_start;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)tz_malloc(TEMP_BUFFER_TEST_SIZE);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto exit;
	}

	/*
	 * 1. kernel CA alloc memory and write data
	 * 2. pass physical address to ta/drv
	 * 3. map write only
	 * 4. compare should NOT match
	 */
	MEMSET_UINT32(temp_buffer, TA_CORE_API_PARAM_TYPE_Test_PatternE,
				TEMP_BUFFER_TEST_SIZE);

	addr_start = (unsigned long)(temp_buffer);
	Flush_Dcache_By_Area(addr_start, addr_start+TEMP_BUFFER_TEST_SIZE);
	memset(&op, 0, sizeof(op));
	temp_buffer_paddr = virt_to_phys((void *)temp_buffer);
	memcpy(x, &temp_buffer_paddr, sizeof(temp_buffer_paddr));
	op.params[0].value.a = x[0];
	op.params[0].value.b = x[1];
	op.params[1].value.a = TEMP_BUFFER_TEST_SIZE;
	op.params[1].value.b = 0;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_NONE,
					TEEC_NONE);

	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
					__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
					__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
					__func__, TEMP_BUFFER_TEST_SIZE);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
					__func__, TEMP_BUFFER_TEST_SIZE);

	if (!ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				DRV_CORE_API_CMD_MSEE_MMAP_REGION_TEST3,
				&op, &ret_orig)))
		goto exit;

	case_res = TEEC_SUCCESS;
exit:
	TEEC_CloseSession(&session);
	if (temp_buffer)
		tz_free(temp_buffer, TEMP_BUFFER_TEST_SIZE);
	return case_res;
}

int xtest_isee_test_3053(void)
{
	uint32_t TEMP_BUFFER_TEST_SIZE = 4096;
	unsigned int res;
	struct TEEC_Session session = {0};
	unsigned char *temp_buffer = 0;
	uint64_t temp_buffer_paddr = 0;
	struct TEEC_Operation op = {0};
	uint32_t ret_orig;
	uint32_t x[2];
	unsigned long addr_start;

	res = xtest_teec_open_session(&xtest_isee_ctx_3000, &session,
				&isee_test_ta_uuid_3000, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)tz_malloc(TEMP_BUFFER_TEST_SIZE);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto exit;
	}

	/*
	 * 1. kernel CA alloc memory
	 * 2. pass physical address to ta/drv
	 * 3. map write only and write data
	 * 4. kernel CA compare data
	 */
	MEMSET_UINT32(temp_buffer, TA_CORE_API_PARAM_TYPE_Test_PatternD,
				TEMP_BUFFER_TEST_SIZE);
	addr_start = (unsigned long)(temp_buffer);
	Flush_Dcache_By_Area(addr_start, addr_start+TEMP_BUFFER_TEST_SIZE);
	memset(&op, 0, sizeof(op));
	temp_buffer_paddr = virt_to_phys((void *)temp_buffer);
	memcpy(x, &temp_buffer_paddr, sizeof(temp_buffer_paddr));
	op.params[0].value.a = x[0];
	op.params[0].value.b = x[1];
	op.params[1].value.a = TEMP_BUFFER_TEST_SIZE;
	op.params[1].value.b = 0;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_NONE,
					TEEC_NONE);

	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
					__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]temp_buffer_paddr=0x%llx\n",
					__func__, temp_buffer_paddr);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
					__func__, TEMP_BUFFER_TEST_SIZE);
	IMSG_DEBUG("[%s]TEMP_BUFFER_TEST_SIZE=0x%x\n",
					__func__, TEMP_BUFFER_TEST_SIZE);

	if (!ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				DRV_CORE_API_CMD_MSEE_MMAP_REGION_TEST4,
				&op, &ret_orig)))
		goto exit;

	Invalidate_Dcache_By_Area(addr_start,
				addr_start + TEMP_BUFFER_TEST_SIZE);

	if (!ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
					ARRAY_MATCH_PATTERN(temp_buffer,
					TA_CORE_API_PARAM_TYPE_Test_PatternC,
					TEMP_BUFFER_TEST_SIZE)))
		case_res = DRV_ERROR_MSEE_MAP_REGION_TEST4;

	case_res = TEEC_SUCCESS;
exit:
	TEEC_CloseSession(&session);
	if (temp_buffer)
		tz_free(temp_buffer, TEMP_BUFFER_TEST_SIZE);
	return case_res;
}

