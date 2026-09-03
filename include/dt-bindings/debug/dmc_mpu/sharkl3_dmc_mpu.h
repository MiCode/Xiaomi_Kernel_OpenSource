/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc sp9863 platform dmc_mpu
 *
 * Copyright (C) 2022, Unisoc Inc.
 */

#ifndef __DT_BINDINGS_DEBUG_SHARKL3_DMC_MPU_H__
#define __DT_BINDINGS_DEBUG_SHARKL3_DMC_MPU_H__

#include "../common.h"

/* list master id */

/* Sub-sys AP */
#define AP_DAP			0x80
#define AP_CPU			0x81
#define AP_CE_S			0x82
#define AP_CE_P			0x89
#define AP_CE_AES_FDE		0x85
#define AP_SDIO0		0x83
#define AP_SDIO1		0x84
#define AP_SDIO2		0x8f
#define AP_EMMC			0x86
#define AP_NANDC		0x87
#define AP_USBOTG		0x88
#define AP_PERI_APCPU		0x8e
#define AP_DMA_CHAN(n)		(0xa0 + (n))

/*DISP*/
#define DISPC			0x8a
#define GSP			0x8b

/*MM*/
#define DCAM			0x90
#define ISP			0x91

/*MM_VSP*/
#define VSP			0x8d
#define JPG			0x92
#define CPP			0x8c

/*GPU*/
#define GPU(n)			(0xf0 + (n))
#define GPU_ETC			0x93

/*WCN*/
#define WCN_M(n)		(0xc0 + (n))

/*WTLCP*/
#define WTLCP_LDSP_P		0xe0
#define WTLCP_LDSP_D		0xe1
#define WTLCP_TGDSP_P		0xe2
#define WTLCP_TGDSP_D		0xe3
#define WTLCP_LDMA		0xe4
#define WTLCP_TGDMA		0xe5
#define LTE_RXDFE		0xea
#define LTE_MEAS		0xeb
#define LTE_ICSNSC		0xec
#define LTE_TFC			0xed
#define LTE_CTP			0xee
#define LTE_CEP			0xef
#define LTE_DBUF		0xf0
#define LTE_FEC			0xf1
#define LTE_ULCH		0xf2
#define LTE_HSDL		0xf3
#define WTL_HU3GE_A		0xe8
#define WTL_HU3GE_B		0xe9

/*PUBCP*/
#define PUBCP_CR5		0xf8
#define PUBCP_CR5_PERI		0xf9
#define PUBCP_DMA		0xfa
#define PUBCP_SEC0		0xfb
#define PUBCP_TFT		0xfc
#define PUBCP_SDIO		0xfd
#define PUBCP_DMA_LINK0		0xfe
#define PUBCP_DMA_LINK_4C	0Xff

/*AON*/
#define AON_DMA(n)		(0xc8 + (n))
#define AON_WCN			0xd6
#define AON_CM4			0xd7
#define AON_ETR			0xc7

#endif/* __DT_BINDINGS_DEBUG_SHARKL3_DMC_MPU_H__ */
