/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc UMS9621 platform clocks
 *
 * Copyright (C) 2022, Unisoc Inc.
 */


#ifndef __DT_BINDINGS_DEBUG_QOGIRL6_DMC_MPU_H__
#define __DT_BINDINGS_DEBUG_QOGIRL6_DMC_MPU_H__

#include "../common.h"

/* list master id */

/* Sub-sys AP */
#define AP_DAP			0x80
#define AP_CPU			0x81
#define ETR			0x82
#define AP_DPU			0x90
#define AP_GSP			0x91
#define AP_VDSP_MSTI		0xC4
#define AP_VDSP_MSTD		0xC9
#define AP_VDMA			0x92
#define AP_CE_SEC		0x87
#define AP_CE_PUB		0x88
#define AP_CE_FDE		0x89
#define AP_SDIO0		0x8a
#define AP_SDIO1		0x8b
#define AP_SDIO2		0x8c
#define AP_EMMC			0x8d
#define AP_VSP			0x8E
#define AP_VDSP_IDMA		0x8F

/*MM*/
#define DCAM_IF			0xc0
#define MM_FD			0xc1
#define ISP_YUV			0xc2
#define VSP_DSP			0xc3
#define VSP_IDMA		0xc4
#define JPG			0xc5
#define CPP			0xc6

/*PUBCP*/
#define PUBCP_CR5		0xc8
#define PUBCP_DMA		0xca
#define PUBCP_SEC0		0xcb
#define PUBCP_TFT		0xcc
#define PUBCP_DMA_LINK0		0xcd
#define PUBCP_DMA_LINK_4C	0Xce
#define PUBCP_SDIO		0xcf

/*WTLCP*/
#define HU3GE_A_VDEC		0xe0
#define HU3GE_A_TEDC		0xe1
#define HU3GE_A_I_Q_0		0xe2
#define HU3GE_A_I_Q_1		0xe3
#define HU3GE_A_HARQ_E2I	0xe4
#define HU3GE_A_HARQ_I2E	0xe5
#define HU3GE_A_FMRAM		0xe6
#define HU3GE_B_VDEC		0xe8
#define HU3GE_B_TEDC		0xe9
#define HU3GE_B_I_Q_0		0xea
#define HU3GE_B_I_Q_1		0xeb
#define HU3GE_B_HARQ_E2I	0xec
#define HU3GE_B_HARQ_I2E	0xed
#define HU3GE_B_FMRAM		0xee

#define CDMA_2X_DFE0		0x84
#define CDMA_2X_DFE1		0x85
#define CDMA_2X_DFE2		0x86
#define CDMA_2X_AHB_MASTER	0xc3

#define LDSP_P			0xd2
#define LDSP_D			0xd3
#define TGDPS_P			0xdf
#define TGDPS_D			0xde
#define LDSP_DMA		0xef
#define TGDSP_DMA		0xe7
#define LTE_CSDFE_RXDFE		0xd4
#define LTE_CE_MEAS		0xd5
#define LTE_CE_ICSNCS		0xd6
#define LTE_CE_TFC		0xd7
#define LTE_CE_CTP		0xd8
#define LTE_CE_CEP		0xd9
#define LTE_CE_DBUF		0xda
#define LTE_DPFEC		0xdb
#define LTE_ULCH		0xdc
#define LTE_HSDL		0xdd

/*AUDCP*/
#define AUDDSP_I		0xf4
#define AUDDSP_D		0xf5
#define DMA_AP			0xf6
#define DMA_CP			0xf7

/*GPU*/
#define GPU			0xd1

/*SP*/
#define USBOTG			0xf1
#define TMC			0xf2
#define SP_CM4			0xf3

#endif /* __DT_BINDINGS_DEBUG_QOGIRL6_DMC_MPU_H__ */
