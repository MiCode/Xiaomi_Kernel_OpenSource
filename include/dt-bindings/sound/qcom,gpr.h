/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved. */
#ifndef __DT_BINDINGS_QCOM_GPR_H
#define __DT_BINDINGS_QCOM_GPR_H

/* Domain IDs */
#define GPR_DOMAIN_SIM		0x0
#define GPR_DOMAIN_MODEM	0x1
#define GPR_DOMAIN_ADSP		0x2
#define GPR_DOMAIN_APPS		0x3
#define GPR_DOMAIN_SDSP		0x4
#define GPR_DOMAIN_CDSP		0x5
#define GPR_DOMAIN_CC_DSP	0x6
#define GPR_DOMAIN_MAX		0x7

/* ADSP service IDs */
#define GPR_SVC_ADSP_CORE	0x3
#define GPR_SVC_AFE		0x4
#define GPR_SVC_VSM		0x5
#define GPR_SVC_VPM		0x6
#define GPR_SVC_ASM		0x7
#define GPR_SVC_ADM		0x8
#define GPR_SVC_ADSP_MVM	0x09
#define GPR_SVC_ADSP_CVS	0x0A
#define GPR_SVC_ADSP_CVP	0x0B
#define GPR_SVC_USM		0x0C
#define GPR_SVC_LSM		0x0D
#define GPR_SVC_VIDC		0x16
#define GPR_SVC_MAX		0x17

#endif /* __DT_BINDINGS_QCOM_GPR_H */

