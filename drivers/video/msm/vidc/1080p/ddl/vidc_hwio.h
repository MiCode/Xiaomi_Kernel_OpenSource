/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _VIDC_HWIO_H_
#define _VIDC_HWIO_H_

#include "vidc_hwio_reg.h"

#ifdef VIDC_REGISTER_LOG
#define VIDC_REG_OUT(x...)  printk(KERN_DEBUG x)
#define VIDC_REG_IN(x...)   printk(KERN_DEBUG x)
#else
#define VIDC_REG_OUT(x...)
#define VIDC_REG_IN(x...)
#endif

#define __inpdw(port) __raw_readl(port)
#define __outpdw(port, val) __raw_writel(val, port)

#define in_dword(addr) (__inpdw(addr))
#define in_dword_masked(addr, mask) (__inpdw(addr) & (mask))
#define out_dword(addr, val) __outpdw(addr, val)

#define out_dword_masked(io, mask, val, shadow) \
do { \
	shadow = (shadow & (u32)(~(mask))) | ((u32)((val) & (mask))); \
	out_dword(io, shadow); \
} while (0)
#define out_dword_masked_ns(io, mask, val, current_reg_content) \
	out_dword(io, ((current_reg_content & (u32)(~(mask))) | \
	((u32)((val) & (mask)))))

#define HWIO_IN(hwiosym)  HWIO_##hwiosym##_IN
#define HWIO_INI(hwiosym, index)  HWIO_##hwiosym##_INI(index)
#define HWIO_INM(hwiosym, mask)   HWIO_##hwiosym##_INM(mask)
#define HWIO_INF(hwiosym, field)  (HWIO_INM(hwiosym, \
	HWIO_FMSK(hwiosym, field)) >> HWIO_SHFT(hwiosym, field))

#define HWIO_OUT(hwiosym, val)  HWIO_##hwiosym##_OUT(val)
#define HWIO_OUTI(hwiosym, index, val)  HWIO_##hwiosym##_OUTI(index, val)
#define HWIO_OUTM(hwiosym, mask, val)  HWIO_##hwiosym##_OUTM(mask, val)
#define HWIO_OUTF(hwiosym, field, val)  HWIO_OUTM(hwiosym, \
	HWIO_FMSK(hwiosym, field), (u32)(val) << HWIO_SHFT(hwiosym, field))

#define HWIO_SHFT(hwio_regsym, hwio_fldsym) \
	HWIO_##hwiosym##_##hwiofldsym##_SHFT
#define HWIO_FMSK(hwio_regsym, hwio_fldsym) \
	HWIO_##hwiosym##_##hwiofldsym##_BMSK

#define VIDC_SETFIELD(val, shift, mask) \
	(((val) << (shift)) & (mask))
#define VIDC_GETFIELD(val, mask, shift) \
	(((val) & (mask)) >> (shift))

#define VIDC_HWIO_OUT(hwiosym, val) \
do { \
	VIDC_REG_OUT("\n(0x%x:"#hwiosym"=0x%x)", \
	HWIO_##hwiosym##_ADDR - VIDC_BASE_PTR, val); \
	mb(); \
	HWIO_OUT(hwiosym, val); \
} while (0)
#define VIDC_HWIO_OUTI(hwiosym, index, val) \
do { \
	VIDC_REG_OUT("\n(0x%x:"#hwiosym"(%d)=0x%x)", \
	HWIO_##hwiosym##_ADDR(index) - VIDC_BASE_PTR, index, val); \
	mb(); \
	HWIO_OUTI(hwiosym, index, val); \
} while (0)
#define VIDC_HWIO_OUTF(hwiosym, field, val) \
do { \
	VIDC_REG_OUT("\n(0x%x:"#hwiosym":0x%x:=0x%x)" , \
	HWIO_##hwiosym##_ADDR - VIDC_BASE_PTR, \
	HWIO_##hwiosym##_##field##_BMSK, val) \
	mb(); \
	HWIO_OUTF(hwiosym, field, val); \
} while (0)
#define VIDC_OUT_DWORD(addr, val) \
do { \
	VIDC_REG_OUT("\n(0x%x:"#addr"=0x%x)", \
	addr - VIDC_BASE_PTR, val); \
	mb(); \
	out_dword(addr, val); \
} while (0)
#define VIDC_HWIO_IN(hwiosym, pval) \
do { \
	mb(); \
	*pval = (u32) HWIO_IN(hwiosym); \
	VIDC_REG_IN("\n(0x%x:"#hwiosym"=0x%x)", \
	HWIO_##hwiosym##_ADDR - VIDC_BASE_PTR, *pval);\
} while (0)
#define VIDC_HWIO_INI(hwiosym, index, pval) \
do { \
	mb(); \
	*pval = (u32) HWIO_INI(hwiosym, index); \
	VIDC_REG_IN("(0x%x:"#hwiosym"(%d)==0x%x)", \
	HWIO_##hwiosym##_ADDR(index) - VIDC_BASE_PTR, index, *pval); \
} while (0)
#define VIDC_HWIO_INF(hwiosym, mask, pval) \
do { \
	mb(); \
	*pval = HWIO_INF(hwiosym, mask); \
	VIDC_REG_IN("\n(0x%x:"#hwiosym"=0x%x)", \
	HWIO_##hwiosym##_ADDR - VIDC_BASE_PTR, *pval); \
} while (0)
#endif
