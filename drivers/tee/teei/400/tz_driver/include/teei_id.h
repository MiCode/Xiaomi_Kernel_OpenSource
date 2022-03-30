/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TEEI_ID_H_
#define __TEEI_ID_H_
#include <asm/cacheflush.h>

#define SMC_ENOMEM          7
#define SMC_EOPNOTSUPP      6
#define SMC_EINVAL_ADDR     5
#define SMC_EINVAL_ARG      4
#define SMC_ERROR           3
#define SMC_INTERRUPTED     2
#define SMC_PENDING         1
#define SMC_SUCCESS         0
/*extern void __flush_dcache_area(void *addr, size_t len);*/
extern unsigned long boot_soter_flag;
#define START_STATUS    (0)

/**
 * @brief Encoding data type
 */
enum teei_enc_data_type {
	TEEI_ENC_INVALID_TYPE = 0,
	TEEI_ENC_UINT32,
	TEEI_ENC_ARRAY,
	TEEI_MEM_REF,
	TEEI_SECURE_MEM_REF
};
/**
 * @brief Command ID's for global service
 */
enum _global_cmd_id {
	TEEI_GLOBAL_CMD_ID_INVALID = 0x0,
	TEEI_GLOBAL_CMD_ID_BOOT_ACK,
	/* add by lodovico */
	TEEI_GLOBAL_CMD_ID_INIT_CONTEXT,
	/* add end */
	TEEI_GLOBAL_CMD_ID_OPEN_SESSION,
	TEEI_GLOBAL_CMD_ID_CLOSE_SESSION,
	TEEI_GLOBAL_CMD_ID_RESUME_ASYNC_TASK,
	TEEI_GLOBAL_CMD_ID_UNKNOWN         = 0x7FFFFFFE,
	TEEI_GLOBAL_CMD_ID_MAX             = 0x7FFFFFFF
};

/* add by lodovico */
/* void printff(); */


int service_smc_call(u32 teei_cmd_type, u32 dev_file_id, u32 svc_id,
			u32 cmd_id, u32 context, u32 enc_id,
			const void *cmd_buf,
			size_t cmd_len,
			void *resp_buf,
			size_t resp_len,
			const void *meta_data,
			int *ret_resp_len,
			void *wq,
			void *arg_lock, int *error_code);


enum teei_cmd_type {
	TEEI_CMD_TYPE_INVALID = 0x0,
	TEEI_CMD_TYPE_SOCKET_INIT,
	TEEI_CMD_TYPE_INITIALIZE_CONTEXT,
	TEEI_CMD_TYPE_FINALIZE_CONTEXT,
	TEEI_CMD_TYPE_OPEN_SESSION,
	TEEI_CMD_TYPE_CLOSE_SESSION,
	TEEI_CMD_TYPE_INVOKE_COMMAND,
	TEEI_CMD_TYPE_UNKNOWN         = 0x7FFFFFFE,
	TEEI_CMD_TYPE_MAX             = 0x7FFFFFFF
};

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

#define Cache_line_size 32
/****************************************************************
 * @brief:
 *     Flush_Dcache_By_Area
 * @param:
 *     start - mva start
 *     end   - mva end
 * @return:
 * ***************************************************************/
static inline void Flush_Dcache_By_Area(unsigned long start, unsigned long end)
{
//	if (boot_soter_flag == START_STATUS) {
//#ifdef CONFIG_ARM64
//		__flush_dcache_area((void *)start, (end - start));
//#else
//		__cpuc_flush_dcache_area((void *)start, (end - start));
//#endif
//	}


}

/******************************************************************
 * @brief:
 *     Invalidate_Dcache_By_Area
 * @param:
 *     start - mva start
 *     end   - mva end
 * @return:
 * *****************************************************************/

static inline void __Invalidate_Dcache_By_Area(unsigned long start,
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


static inline void Invalidate_Dcache_By_Area(unsigned long start,
						unsigned long end)
{
	if (boot_soter_flag == START_STATUS)
		__Invalidate_Dcache_By_Area(start, end);
}

/* add end */
#endif /* __OPEN_OTZ_ID_H_ */
