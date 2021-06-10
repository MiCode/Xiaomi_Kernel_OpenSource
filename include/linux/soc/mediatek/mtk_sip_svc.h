/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_SECURE_API_H_
#define _MTK_SECURE_API_H_

#include <linux/kernel.h>

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_AARCH_BIT			0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT			0x00000000
#endif

/* CCCI debug feature */
#define MTK_SIP_KERNEL_CCCI_GET_INFO \
	(0x82000206 | MTK_SIP_SMC_AARCH_BIT)

/* eMMC related SMC call */
#define MTK_SIP_KERNEL_HW_FDE_AES_INIT \
		(0x82000271 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_HW_FDE_KEY   \
		(0x82000272 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL \
		(0x82000273 | MTK_SIP_SMC_AARCH_BIT)

/* M4U kernel SMC call */
#define MTK_M4U_DEBUG_DUMP \
	(0x820002B0 | MTK_SIP_SMC_AARCH_BIT)

/* Debug related SMC call */
#define MTK_SIP_KERNEL_CCCI_CONTROL \
	(0x82000505 | MTK_SIP_SMC_AARCH_BIT)

/* EMI MPU */
#define MTK_SIP_EMIMPU_CONTROL \
	(0x8200050B | MTK_SIP_SMC_AARCH_BIT)

/* Clock Buffer */
#define MTK_SIP_CLKBUF_CONTROL \
	(0x82000520 | MTK_SIP_SMC_AARCH_BIT)

/* MCUSYS CPU */
#define MTK_SIP_MCUSYS_CPU_CONTROL \
	(0x82000521 | MTK_SIP_SMC_AARCH_BIT)

/* display port control related SMC call */
#define MTK_SIP_DP_CONTROL \
	(0x82000523 | MTK_SIP_SMC_AARCH_BIT)

#endif
/* _MTK_SECURE_API_H_ */
