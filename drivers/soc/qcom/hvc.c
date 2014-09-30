/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/export.h>
#include <linux/err.h>

#include <asm/compiler.h>

#include <soc/qcom/hvc.h>

#define HVC_RET_SUCCESS				0
#define HVC_RET_ERROR				-1
#define HVC_RET_EFUNCNOSUPPORT			-2
#define HVC_RET_EINVALARCH			-3
#define HVC_RET_EMEMMAP				-4
#define HVC_RET_EMEMUNMAP			-5
#define HVC_RET_EMEMPERM			-6

static int hvc_to_linux_errno(int errno)
{
	switch (errno) {
	case HVC_RET_SUCCESS:
		return 0;
	case HVC_RET_ERROR:
		return -EIO;
	case HVC_RET_EFUNCNOSUPPORT:
		return -EOPNOTSUPP;
	case HVC_RET_EINVALARCH:
	case HVC_RET_EMEMMAP:
	case HVC_RET_EMEMUNMAP:
		return -EINVAL;
	case HVC_RET_EMEMPERM:
		return -EPERM;
	};

	return -EINVAL;
}

#ifdef CONFIG_ARM64
static int __hvc(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5,
		 u64 x6, u64 x7, u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u64 r0 asm("x0") = x0;
	register u64 r1 asm("x1") = x1;
	register u64 r2 asm("x2") = x2;
	register u64 r3 asm("x3") = x3;
	register u64 r4 asm("x4") = x4;
	register u64 r5 asm("x5") = x5;
	register u64 r6 asm("x6") = x6;
	register u64 r7 asm("x7") = x7;

	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			__asmeq("%4", "x4")
			__asmeq("%5", "x5")
			__asmeq("%6", "x6")
			__asmeq("%7", "x7")
			"hvc	#0\n"
		: "+r" (r0), "+r" (r1), "+r" (r2), "+r" (r3)
		: "r" (r4), "r" (r5), "r" (r6), "r" (r7));

	*ret1 = r1;
	*ret2 = r2;
	*ret3 = r3;

	return r0;
}
#else
static int __hvc(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5,
		 u64 x6, u64 x7, u64 *ret1, u64 *ret2, u64 *ret3)
{
	return 0;
}
#endif

int hvc(u64 func_id, struct hvc_desc *desc)
{
	int ret;

	if (!desc)
		return -EINVAL;

	ret = __hvc(func_id, desc->arg[0], desc->arg[1], desc->arg[2],
		    desc->arg[3], desc->arg[4], desc->arg[5], 0,
		    &desc->ret[0], &desc->ret[1], &desc->ret[2]);

	return hvc_to_linux_errno(ret);
}
EXPORT_SYMBOL(hvc);
