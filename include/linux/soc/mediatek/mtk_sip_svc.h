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


/* Security related SMC call */
/* TRNG */
#define MTK_SIP_KERNEL_GET_RND \
	(0x82000206 | MTK_SIP_SMC_AARCH_BIT)


#endif				/* _MTK_SECURE_API_H_ */

