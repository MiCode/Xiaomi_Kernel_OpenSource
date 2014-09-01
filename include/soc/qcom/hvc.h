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

#ifndef __MSM_HVC_H
#define __MSM_HVC_H

#ifdef CONFIG_ARM64
#define HVC_FN_ARM_BASE				0xC0000000
#define HVC_FN_CPU_BASE				0xC1000000
#define HVC_FN_SIP_BASE				0xC2000000
#define HVC_FN_OEM_BASE				0xC3000000
#define HVC_FN_APP_BASE				0xF0000000
#define HVC_FN_OS_BASE				0xF2000000
#else
#define HVC_FN_ARM_BASE				0x80000000
#define HVC_FN_CPU_BASE				0x81000000
#define HVC_FN_SIP_BASE				0x82000000
#define HVC_FN_OEM_BASE				0x83000000
#define HVC_FN_APP_BASE				0xB0000000
#define HVC_FN_OS_BASE				0xB2000000
#endif

#define HVC_FN_ARM(n)				(HVC_FN_ARM_BASE + (n))
#define HVC_FN_CPU(n)				(HVC_FN_CPU_BASE + (n))
#define HVC_FN_SIP(n)				(HVC_FN_SIP_BASE + (n))
#define HVC_FN_OEM(n)				(HVC_FN_OEM_BASE + (n))
#define HVC_FN_APP(n)				(HVC_FN_APP_BASE + (n))
#define HVC_FN_OS(n)				(HVC_FN_OS_BASE + (n))

#define HVC_MAX_ARGS				6
#define HVC_MAX_RETS				3
#define HVC_MAX_EXTRA_ARGS			4

struct hvc_desc {
	u64 arg[HVC_MAX_ARGS];
	u64 ret[HVC_MAX_RETS];
};

struct hvc_extra_args {
	u64 arg[HVC_MAX_EXTRA_ARGS];
};

#ifdef CONFIG_MSM_HVC
extern int hvc(u64 func_id, struct hvc_desc *desc);
#else
static inline int hvc(u64 func_id, struct hvc_desc *desc) { return 0; }
#endif

#endif
