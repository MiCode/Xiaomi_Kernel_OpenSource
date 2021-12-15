/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MT_EMI_H__
#define __MT_EMI_H__

/* submodule control */
#define ENABLE_BWL	1
#define ENABLE_MPU	1
#define ENABLE_ELM	1
#define ENABLE_MBW	0
#define DECS_ON_SSPM
#define MPU_BYPASS
#define ISU_VER_CTRL
/* #define ENABLE_MPU_SLVERR */
#define DBG_INFO_READY	1
#define ENABLE_EMI_DEBUG_API

/* IRQ from device tree */
#define MPU_IRQ_INDEX	0
#define ELM_IRQ_INDEX	1 /* fcBianco uses CGM IRQ as ELM IRQ */

/* macro for MPU */
#if defined(CONFIG_MACH_MT6768)
#define ENABLE_AP_REGION	1
#else
#define ENABLE_AP_REGION	0
#endif

#define AP_REGION_ID		31

#define EMI_MPUD0_ST		(CEN_EMI_BASE + 0x160)
#define EMI_MPUD_ST(domain)	(EMI_MPUD0_ST + (4*domain))
#define EMI_MPUD0_ST2		(CEN_EMI_BASE + 0x200)
#define EMI_MPUD_ST2(domain)	(EMI_MPUD0_ST2 + (4*domain))
#define EMI_MPUS		(CEN_EMI_BASE + 0x01F0)
#define EMI_MPUT		(CEN_EMI_BASE + 0x01F8)
#define EMI_MPUT_2ND		(CEN_EMI_BASE + 0x01FC)

#define EMI_MPU_SA0		(0x100)
#define EMI_MPU_EA0		(0x200)
#define EMI_MPU_SA(region)	(EMI_MPU_SA0 + (region*4))
#define EMI_MPU_EA(region)	(EMI_MPU_EA0 + (region*4))
#define EMI_MPU_APC0		(0x300)
#define EMI_MPU_APC(region, dgroup) \
	(EMI_MPU_APC0 + (region*4) + ((dgroup)*0x100))

#define EMI_MPU_CTRL_D0		(0x800)
#define EMI_MPU_CTRL_D(domain)	(EMI_MPU_CTRL_D0 + (domain*4))

/* macro for ELM */
#define MBW_BUF_LEN		0x800000

#define LAST_EMI_DECS_CTRL	(LAST_EMI_BASE + 0x04)
#define LAST_EMI_MBW_BUF_L	(LAST_EMI_BASE + 0x10)
#define LAST_EMI_MBW_BUF_H	(LAST_EMI_BASE + 0x14)
#define LAST_EMI_ISU_VER	(LAST_EMI_BASE + 0x18)

#include <mt_emi_api.h>
#include <bwl_v1.h>
#include <elm_v1.h>

#endif /* __MT_EMI_H__ */

