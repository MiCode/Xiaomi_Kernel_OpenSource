/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_SECURE_API_H_
#define _MTK_SECURE_API_H_

#include <linux/kernel.h>

/* Error Code */
#define SIP_SVC_E_SUCCESS               0
#define SIP_SVC_E_NOT_SUPPORTED         -1
#define SIP_SVC_E_INVALID_PARAMS        -2
#define SIP_SVC_E_INVALID_Range         -3
#define SIP_SVC_E_PERMISSION_DENY       -4

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_AARCH_BIT			0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT			0x00000000
#endif

/* 0x82000200 - 0x820003FF & 0xC2000300 - 0xC20003FF */
/* Debug feature and ATF related */
#define MTK_SIP_KERNEL_TIME_SYNC \
	(0x82000202 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_ATF_DEBUG \
	(0x82000204 | MTK_SIP_SMC_AARCH_BIT)

/* Security related SMC call */
/* TRNG */
#define MTK_SIP_KERNEL_GET_RND \
	(0x82000206 | MTK_SIP_SMC_AARCH_BIT)
/* DEVAPC */
#define MTK_SIP_KERNEL_DAPC_PERM_GET \
	(0x82000207 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_CLR_SRAMROM_VIO \
	(0x82000208 | MTK_SIP_SMC_AARCH_BIT)
/* CCCI debug feature */
#define MTK_SIP_KERNEL_CCCI_GET_INFO \
	(0x82000209 | MTK_SIP_SMC_AARCH_BIT)
/* VCOREFS */
#define MTK_SIP_VCOREFS_CONTROL \
	(0x82000506 | MTK_SIP_SMC_AARCH_BIT)

#endif				/* _MTK_SECURE_API_H_ */

