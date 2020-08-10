/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef __MTK_SIP_SVC_H
#define __MTK_SIP_SVC_H

/* Error Code */
#define SIP_SVC_E_SUCCESS               0
#define SIP_SVC_E_NOT_SUPPORTED         -1
#define SIP_SVC_E_INVALID_PARAMS        -2
#define SIP_SVC_E_INVALID_RANGE         -3
#define SIP_SVC_E_PERMISSION_DENIED     -4

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_CONVENTION          ARM_SMCCC_SMC_64
#else
#define MTK_SIP_SMC_CONVENTION          ARM_SMCCC_SMC_32
#endif

#define MTK_SIP_SMC_CMD(fn_id) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, MTK_SIP_SMC_CONVENTION, \
			   ARM_SMCCC_OWNER_SIP, fn_id)

/* VCOREFS */
#define MTK_SIP_VCOREFS_CONTROL \
	MTK_SIP_SMC_CMD(0x506)

/* EMI MPU */
#define MTK_SIP_EMIMPU_CONTROL \
	MTK_SIP_SMC_CMD(0x50B)

/* Debug feature and ATF related */
#define MTK_SIP_KERNEL_WDT \
	MTK_SIP_SMC_CMD(0x200)

/* CCCI debug feature */
#define MTK_SIP_KERNEL_CCCI_GET_INFO \
	MTK_SIP_SMC_CMD(0x206)

/* Security related SMC call */
/* DEVMPU */
#define MTK_SIP_KERNEL_DEVMPU_VIO_GET \
	MTK_SIP_SMC_CMD(0x264)
#define MTK_SIP_KERNEL_DEVMPU_PERM_GET \
	MTK_SIP_SMC_CMD(0x265)
#define MTK_SIP_KERNEL_DEVMPU_VIO_CLR \
	MTK_SIP_SMC_CMD(0x268)
/* DEVAPC */
#define MTK_SIP_KERNEL_DAPC_PERM_GET \
	MTK_SIP_SMC_CMD(0x26B)
#define MTK_SIP_KERNEL_CLR_SRAMROM_VIO \
	MTK_SIP_SMC_CMD(0x26C)

/* Debug related SMC call */
#define MTK_SIP_KERNEL_CCCI_CONTROL \
	MTK_SIP_SMC_CMD(0x505)

/* AUDIO related SMC call */
#define MTK_SIP_AUDIO_CONTROL \
	MTK_SIP_SMC_CMD(0x517)

/* MTK LPM */
#define MTK_SIP_MTK_LPM_CONTROL \
	MTK_SIP_SMC_CMD(0x507)
/* for APUSYS SMC call */
#define MTK_SIP_APUSYS_CONTROL \
	MTK_SIP_SMC_CMD(0x51E)

#endif
