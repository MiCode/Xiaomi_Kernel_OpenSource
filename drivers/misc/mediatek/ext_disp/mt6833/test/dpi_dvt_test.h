/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DPI_DVT_TEST_H_
#define _DPI_DVT_TEST_H_

/* when open this option, RDMA-DPI Path can be used*/
#define RDMA_DPI_PATH_SUPPORT
/* when open this option, DPI DVT test case can be used*/
#define DPI_DVT_TEST_SUPPORT

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)
#include "hdmi_drv.h"
#include "disp_drv_log.h"
/*#include "ddp_dpi_ext.h"*/
/*#include "ddp_rdma.h"*/

#define COLOR_BAR_PATTERN 0x41

enum HW_MODULE_Type {
	M4U_FOR_RDMA0,
	M4U_FOR_RDMA1,
	M4U_FOR_RDMA2,
	M4U_FOR_OVL0,
	M4U_FOR_OVL1,
	M4U_FOR_OVL0_2L,
	M4U_FOR_OVL1_2L,
	M4U_FOR_WDMA1,
	MAX_NUM_HW
};

enum DPI_COLOR_ORDER {
	DPI_COLOR_ORDER_RGB = 0,
	DPI_COLOR_ORDER_BGR = 1
};

struct DPI_DVT_CONTEXT {
	int     hdmi_width;
	int     hdmi_height;
	int     bg_width;
	int     bg_height;
	enum HDMI_VIDEO_RESOLUTION       output_video_resolution;
	int     scaling_factor;
};

#define DPI_DVT_LOG_W(fmt, args...)   DISPINFO("[DPI_DVT/]"fmt, ##args)

int dvt_init_RDMA_param(unsigned int mode, unsigned int resolution);
void dpi_dvt_parameters(unsigned char arg);
void dvt_dump_ext_dpi_parameters(void);
int dvt_copy_file_data(void *ptr, unsigned int resolution);
int dvt_allocate_buffer(unsigned int resolution, enum HW_MODULE_Type hw_type);
int ldvt_allocate_buffer(unsigned int resolution, enum HW_MODULE_Type hw_type);
unsigned int dpi_ldvt_testcase(void);



#endif

unsigned int dpi_dvt_ioctl(unsigned int arg);
#endif
