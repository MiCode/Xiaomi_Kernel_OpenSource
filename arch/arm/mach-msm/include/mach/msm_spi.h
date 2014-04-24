/* Copyright (c) 2008-2009, 2012-2013 The Linux Foundation. All rights reserved.
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
 * SPI driver for Qualcomm MSM platforms.
 */

/**
 * msm_spi_platform_data: msm spi-controller's configuration data
 *
 * @active_only when set, votes when system active and removes the vote when
 *       system goes idle (optimises for performance). When unset, voting using
 *       runtime pm (optimizes for power).
 * @master_id master id number of the controller's wrapper (BLSP or GSBI).
 *       When zero, clock path voting is disabled.
 * @rt when set, spi will pump transaction messages with high (realtime)
 *	priority to reduce the transfer latency on the bus by minimising
 *	the delay between a transfer request.
 */
struct msm_spi_platform_data {
	u32 max_clock_speed;
	bool active_only;
	u32  master_id;
	int (*gpio_config)(void);
	void (*gpio_release)(void);
	int (*dma_config)(void);
	const char *rsl_id;
	u32  pm_lat;
	u32  infinite_mode;
	bool ver_reg_exists;
	bool use_bam;
	u32  bam_consumer_pipe_index;
	u32  bam_producer_pipe_index;
	bool rt_priority;
};
