/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
/*
 * I2C driver for Qualcomm MSM platforms.
 */

#ifndef _MACH_MSM_I2C_MSM_H
#define _MACH_MSM_I2C_MSM_H

/**
 * i2c_msm_v2_platform_data: i2c-msm-v2 driver configuration data
 *
 * @clk_freq_in core clock frequency in Hz
 * @clk_freq_out bus clock frequency in Hz
 * @bam_pipe_idx_cons index of BAM's consumer pipe
 * @bam_pipe_idx_prod index of BAM's producer pipe
 * @bam_disable disables DMA transfers.
 * @gpio_scl clock GPIO pin number
 * @gpio_sda data GPIO pin number
 * @noise_rjct_scl number of low samples on clock line to consider it low.
 * @noise_rjct_sda number of low samples on data  line to consider it low.
 * @active_only when set, votes when system active and removes the vote when
 *       system goes idle (optimises for performance). When unset, voting using
 *       runtime pm (optimizes for power).
 * @master_id master id number of the i2c core or its wrapper (BLSP/GSBI).
 *       When zero, clock path voting is disabled.
 */
struct i2c_msm_v2_platform_data {
	int  clk_freq_in;
	int  clk_freq_out;
	u32  bam_pipe_idx_cons;
	u32  bam_pipe_idx_prod;
	bool bam_disable;
	int  gpio_scl;
	int  gpio_sda;
	int  noise_rjct_scl;
	int  noise_rjct_sda;
	bool active_only;
	u32  master_id;
};

#endif
