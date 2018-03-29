/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DDP_DPI_EXT_H__
#define __DDP_DPI_EXT_H__

#include "dpi_dvt_test.h"

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)

#include "lcm_drv.h"
#include "ddp_info.h"
/*#include "cmdq_record.h"*/
#include "dpi_dvt_test.h"
#include "ddp_dpi.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	DPI_EXT_STATUS_OK = 0,
	DPI_EXT_STATUS_ERROR,
} DPI_EXT_STATUS;


/*************************for DPI DVT***************************/
/*
typedef enum {
	acsRGB         = 0
	, acsYCbCr422    = 1
	, acsYCbCr444    = 2
	, acsFuture      = 3
} AviColorSpace_e;
*/

DPI_EXT_STATUS DPI_EnableColorBar(unsigned int pattern);
DPI_EXT_STATUS DPI_DisableColorBar(void);
DPI_EXT_STATUS ddp_dpi_EnableColorBar_0(void);
DPI_EXT_STATUS ddp_dpi_EnableColorBar_16(void);

int configInterlaceMode(unsigned int resolution);
int config3DMode(unsigned int resolution);
int config3DInterlaceMode(unsigned int resolution);
unsigned int readDPIStatus(void);
unsigned int readDPITDLRStatus(void);
unsigned int clearDPIStatus(void);
unsigned int clearDPIIntrStatus(void);
unsigned int readDPIIntrStatus(void);
unsigned int ClearDPIIntrStatus(void);
unsigned int enableRGB2YUV(enum AviColorSpace_e format);
unsigned int enableSingleEdge(void);
int enableAndGetChecksum(void);
/* int enableAndGetChecksumCmdq(cmdqRecHandle cmdq_handle); */
unsigned int configDpiRepetition(void);
unsigned int configDpiEmbsync(void);
/* unsigned int configDpiColorTransformToBT709(void); */
/* unsigned int configDpiRGB888ToLimitRange(void); */
/************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __DPI_DRV_H__ */
