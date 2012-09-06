/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _VIDEO_720P_RESOURCE_TRACKER_H_
#define _VIDEO_720P_RESOURCE_TRACKER_H_

#include <linux/regulator/consumer.h>
#include <linux/msm_ion.h>
#include "vcd_res_tracker_api.h"
#ifdef CONFIG_MSM_BUS_SCALING
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#endif
#include <mach/board.h>

#define RESTRK_1080P_VGA_PERF_LEVEL    VCD_MIN_PERF_LEVEL
#define RESTRK_1080P_720P_PERF_LEVEL   108000
#define RESTRK_1080P_1080P_PERF_LEVEL  244800

#define RESTRK_1080P_MIN_PERF_LEVEL RESTRK_1080P_VGA_PERF_LEVEL
#define RESTRK_1080P_MAX_PERF_LEVEL RESTRK_1080P_1080P_PERF_LEVEL
#define RESTRK_1080P_TURBO_PERF_LEVEL (RESTRK_1080P_MAX_PERF_LEVEL + 1)

struct res_trk_context {
	struct device *device;
	u32 irq_num;
	struct mutex lock;
	struct clk *vcodec_clk;
	struct clk *vcodec_pclk;
	unsigned long vcodec_clk_rate;
	unsigned int clock_enabled;
	unsigned int perf_level;
	struct regulator *footswitch;
	struct msm_vidc_platform_data *vidc_platform_data;
	int memtype;
	int fw_mem_type;
	int cmd_mem_type;
#ifdef CONFIG_MSM_BUS_SCALING
	struct msm_bus_scale_pdata *vidc_bus_client_pdata;
	uint32_t     pcl;
#endif
	u32 core_type;
	struct ddl_buf_addr firmware_addr;
	struct ion_client *res_ion_client;
	u32 disable_dmx;
	u32 disable_fullhd;
	enum ddl_mem_area res_mem_type;
	u32 mmu_clks_on;
	u32 secure_session;
	struct mutex secure_lock;
	u32 sec_clk_heap;
};

#if DEBUG

#define VCDRES_MSG_LOW(xx_fmt...)	printk(KERN_INFO "\n\t* " xx_fmt)
#define VCDRES_MSG_MED(xx_fmt...)	printk(KERN_INFO "\n  * " xx_fmt)

#else

#define VCDRES_MSG_LOW(xx_fmt...)
#define VCDRES_MSG_MED(xx_fmt...)

#endif

#define VCDRES_MSG_HIGH(xx_fmt...)	printk(KERN_WARNING "\n" xx_fmt)
#define VCDRES_MSG_ERROR(xx_fmt...)	printk(KERN_ERR "\n err: " xx_fmt)
#define VCDRES_MSG_FATAL(xx_fmt...)	printk(KERN_ERR "\n<FATAL> " xx_fmt)

#ifdef CONFIG_MSM_BUS_SCALING
int res_trk_update_bus_perf_level(struct vcd_dev_ctxt *dev_ctxt,
				u32 perf_level);
#endif
#endif
