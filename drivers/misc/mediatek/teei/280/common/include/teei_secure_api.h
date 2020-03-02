/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TEEI_KERN_API_
#define _TEEI_KERN_API_

#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#define ARCH_EXTENSION_SEC
#endif

#define TEEI_FC_CPU_ON			(0xb4000080)
#define TEEI_FC_CPU_OFF			(0xb4000081)
#define TEEI_FC_CPU_DORMANT		(0xb4000082)
#define TEEI_FC_CPU_DORMANT_CANCEL	(0xb4000083)
#define TEEI_FC_CPU_ERRATA_802022	(0xb4000084)

#ifdef CONFIG_ARM64
/* SIP SMC Call 64 */
static inline long teei_secure_call(u64 function_id,
	u64 arg0, u64 arg1, u64 arg2)
{
	register u64 reg0 __asm__("x0") = function_id;
	register u64 reg1 __asm__("x1") = arg0;
	register u64 reg2 __asm__("x2") = arg1;
	register u64 reg3 __asm__("x3") = arg2;
	int ret = 0;

	asm volatile ("smc    #0\n" : "+r" (reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3) : :
		"x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
		"x13", "x14", "x15", "x16", "x17");

	/* set response */
	ret = (long)reg0;
	return ret;
}

#else
static inline uint32_t teei_secure_call(u32 function_id,
	u32 arg0, u32 arg1, u32 arg2)
{
	register u32 reg0 __asm__("r0") = function_id;
	register u32 reg1 __asm__("r1") = arg0;
	register u32 reg2 __asm__("r2") = arg1;
	register u32 reg3 __asm__("r3") = arg2;
	int ret = 0;

	__asm__ volatile (
#ifdef ARCH_EXTENSION_SEC
		/* This pseudo op is supported and required from
		 * binutils 2.21 on
		 */
		".arch_extension sec\n"
#endif
		"smc 0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);

	/* set response */
	ret = reg0;
	return ret;
}
#endif

#endif /* _TEEI_KERN_API_ */
