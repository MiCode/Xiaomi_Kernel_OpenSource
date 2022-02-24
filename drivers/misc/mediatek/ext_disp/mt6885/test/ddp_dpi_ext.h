/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DDP_DPI_EXT_H__
#define __DDP_DPI_EXT_H__


#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)

#include "lcm_drv.h"
#include "ddp_info.h"
#include "cmdq_record.h"
/*#include "dpi_dvt_test.h"*/

#ifdef __cplusplus
extern "C" {
#endif


#define DPI_PHY_ADDR 0x14015000

enum DPI_EXT_STATUS {
	DPI_EXT_STATUS_OK = 0,
	DPI_EXT_STATUS_ERROR,
};

enum DPI_POLARITY {
	DPI_POLARITY_RISING = 0,
	DPI_POLARITY_FALLING = 1
};

enum AviColorSpace_e {
	acsRGB = 0, acsYCbCr422 = 1, acsYCbCr444 = 2, acsFuture = 3
};


/*************************for DPI DVT***************************/
/*
 *
 *typedef enum {
 *	acsRGB         = 0
 *	, acsYCbCr422    = 1
 *	, acsYCbCr444    = 2
 *	, acsFuture      = 3
 *} AviColorSpace_e;
 */

enum DPI_EXT_STATUS DPI_EnableColorBar(unsigned int pattern);
enum DPI_EXT_STATUS DPI_DisableColorBar(void);
enum DPI_EXT_STATUS ddp_dpi_EnableColorBar_0(void);
enum DPI_EXT_STATUS ddp_dpi_EnableColorBar_16(void);

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
/* int enableAndGetChecksumCmdq(struct cmdqRecStruct *cmdq_handle); */
unsigned int configDpiRepetition(void);
unsigned int configDpiEmbsync(void);
/* unsigned int configDpiColorTransformToBT709(void); */
/* unsigned int configDpiRGB888ToLimitRange(void); */
/************************************************************/

#ifdef __cplusplus
}
#endif
#endif
#endif /* __DPI_DRV_H__ */
