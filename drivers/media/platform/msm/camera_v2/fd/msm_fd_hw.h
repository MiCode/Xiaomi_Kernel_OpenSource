/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_FD_HW_H__
#define __MSM_FD_HW_H__

#include "msm_fd_dev.h"

int msm_fd_hw_get_face_count(struct msm_fd_device *fd);

int msm_fd_hw_get_result_x(struct msm_fd_device *fd, int idx);

int msm_fd_hw_get_result_y(struct msm_fd_device *fd, int idx);

void msm_fd_hw_get_result_conf_size(struct msm_fd_device *fd,
	int idx, u32 *conf, u32 *size);

void msm_fd_hw_get_result_angle_pose(struct msm_fd_device *fd, int idx,
	u32 *angle, u32 *pose);

int msm_fd_hw_request_irq(struct platform_device *pdev,
	struct msm_fd_device *fd, work_func_t work_func);

void msm_fd_hw_release_irq(struct msm_fd_device *fd);

int msm_fd_hw_get_revision(struct msm_fd_device *fd);

void msm_fd_hw_release_mem_resources(struct msm_fd_device *fd);

int msm_fd_hw_get_mem_resources(struct platform_device *pdev,
	struct msm_fd_device *fd);

int msm_fd_hw_get_iommu(struct msm_fd_device *fd);

void msm_fd_hw_put_iommu(struct msm_fd_device *fd);

int msm_fd_hw_get_clocks(struct msm_fd_device *fd);

int msm_fd_hw_put_clocks(struct msm_fd_device *fd);

int msm_fd_hw_get_bus(struct msm_fd_device *fd);

void msm_fd_hw_put_bus(struct msm_fd_device *fd);

int msm_fd_hw_get(struct msm_fd_device *fd, unsigned int clock_rate_idx);

void msm_fd_hw_put(struct msm_fd_device *fd);

int msm_fd_hw_map_buffer(struct msm_fd_mem_pool *pool, int fd,
	struct msm_fd_buf_handle *buf);

void msm_fd_hw_unmap_buffer(struct msm_fd_buf_handle *buf);

void msm_fd_hw_add_buffer(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer);

void msm_fd_hw_remove_buffers_from_queue(struct msm_fd_device *fd,
	struct vb2_queue *vb2_q);

int msm_fd_hw_buffer_done(struct msm_fd_device *fd,
	struct msm_fd_buffer *buffer);

struct msm_fd_buffer *msm_fd_hw_get_active_buffer(struct msm_fd_device *fd);

int msm_fd_hw_schedule_and_start(struct msm_fd_device *fd);

int msm_fd_hw_schedule_next_buffer(struct msm_fd_device *fd);

#endif /* __MSM_FD_HW_H__ */
