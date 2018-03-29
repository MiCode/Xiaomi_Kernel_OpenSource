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

#ifndef _DPI_DVT_TEST_H_
#define _DPI_DVT_TEST_H_

/* #define RDMA_DPI_PATH_SUPPORT *//* when open this option, RDMA-DPI Path can be used*/
/* #define DPI_DVT_TEST_SUPPORT	*//* when open this option, DPI DVT test case can be used*/

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)
#include "hdmi_drv.h"
#include "ddp_dpi_ext.h"
/*#include "ddp_rdma.h"*/

#ifndef DPI_I32
typedef char	      DPI_I8;
typedef unsigned char     DPI_U8;
typedef int               DPI_I32;
typedef short int         DPI_I16;
typedef unsigned int      DPI_U32;
typedef long int          DPI_I64;   /*64bit system*/
typedef unsigned long int DPI_U64;   /*64bit system*/
typedef bool              DPI_BOOL;
typedef void *pDPI;
#endif

#define COLOR_BAR_PATTERN 0x41

typedef enum {
	M4U_FOR_RDMA0,
	M4U_FOR_RDMA1,
	M4U_FOR_OVL0,
	M4U_FOR_OVL1,
	MAX_NUM_HW
} HW_MODULE_Type;

typedef enum {
	DPI_COLOR_ORDER_RGB = 0,
	DPI_COLOR_ORDER_BGR = 1
} DPI_COLOR_ORDER;

typedef struct {
	DPI_I32     hdmi_width;
	DPI_I32     hdmi_height;
	DPI_I32     bg_width;
	DPI_I32     bg_height;
	enum HDMI_VIDEO_RESOLUTION       output_video_resolution;
	DPI_I32     scaling_factor;
} DPI_DVT_CONTEXT;

#define DPI_DVT_LOG_W(fmt, args...)   pr_err("[DPI_DVT/]"fmt, ##args)

DPI_I32 dvt_init_RDMA_param(DPI_U32 mode, DPI_U32 resolution);
void dpi_dvt_parameters(DPI_U8 arg);
void dvt_dump_ext_dpi_parameters(void);
DPI_I32 dvt_copy_file_data(void *ptr, DPI_U32 resolution);
DPI_I32 dvt_allocate_buffer(DPI_U32 resolution, HW_MODULE_Type hw_type);

#endif

unsigned int dpi_dvt_ioctl(unsigned int arg);
#endif
