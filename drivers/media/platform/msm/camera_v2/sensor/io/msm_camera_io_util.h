/* Copyright (c) 2011-2014, The Linux Foundataion. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_CAMERA_IO_UTIL_H
#define __MSM_CAMERA_IO_UTIL_H

#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <soc/qcom/camera2.h>
#include <media/msm_cam_sensor.h>
#include <media/v4l2-ioctl.h>

#define NO_SET_RATE -1
#define INIT_RATE -2

struct msm_gpio_set_tbl {
	unsigned gpio;
	unsigned long flags;
	uint32_t delay;
};

void msm_camera_io_w(u32 data, void __iomem *addr);
void msm_camera_io_w_mb(u32 data, void __iomem *addr);
u32 msm_camera_io_r(void __iomem *addr);
u32 msm_camera_io_r_mb(void __iomem *addr);
void msm_camera_io_dump(void __iomem *addr, int size);
void msm_camera_io_memcpy(void __iomem *dest_addr,
		void __iomem *src_addr, u32 len);
void msm_camera_io_memcpy_mb(void __iomem *dest_addr,
	void __iomem *src_addr, u32 len);
int msm_cam_clk_sel_src(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct msm_cam_clk_info *clk_src_info, int num_clk);
int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable);

int msm_camera_config_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int config);
int msm_camera_enable_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int enable);

void msm_camera_bus_scale_cfg(uint32_t bus_perf_client,
		enum msm_bus_perf_setting perf_setting);

int msm_camera_set_gpio_table(struct msm_gpio_set_tbl *gpio_tbl,
	uint8_t gpio_tbl_size, int gpio_en);

void msm_camera_config_single_gpio(uint16_t gpio, unsigned long flags,
	int gpio_en);

int msm_camera_config_single_vreg(struct device *dev,
	struct camera_vreg_t *cam_vreg, struct regulator **reg_ptr, int config);

int msm_camera_request_gpio_table(struct gpio *gpio_tbl, uint8_t size,
	int gpio_en);

#endif
