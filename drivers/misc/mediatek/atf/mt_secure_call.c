/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/kernel.h>
#include <mt-plat/mtk_secure_api.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#endif

#if defined(CONFIG_ARM64)
#define LOCAL_REG_SET_DECLARE \
	register size_t reg0 __asm__("x0") = function_id; \
	register size_t reg1 __asm__("x1") = arg0; \
	register size_t reg2 __asm__("x2") = arg1; \
	register size_t reg3 __asm__("x3") = arg2; \
	register size_t reg4 __asm__("x4") = arg3; \
	size_t ret
#else
#define LOCAL_REG_SET_DECLARE \
	register size_t reg0 __asm__("r0") = function_id; \
	register size_t reg1 __asm__("r1") = arg0; \
	register size_t reg2 __asm__("r2") = arg1; \
	register size_t reg3 __asm__("r3") = arg2; \
	register size_t reg4 __asm__("r4") = arg3; \
	size_t ret
#endif

size_t mt_secure_call_all(size_t function_id,
	size_t arg0, size_t arg1, size_t arg2,
	size_t arg3, size_t *r1, size_t *r2, size_t *r3)
{
	LOCAL_REG_SET_DECLARE;

#ifdef CONFIG_ARM64
	__asm__ volatile ("smc #0x0\n" : "+r"(reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3), "+r"(reg4));
#else
	__asm__ volatile (__SMC(0) : "+r"(reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3), "+r"(reg4));
#endif
	ret = reg0;
	if (r1 != NULL)
		*r1 = reg1;
	if (r2 != NULL)
		*r2 = reg2;
	if (r3 != NULL)
		*r3 = reg3;
	return ret;
}
