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
 */
#ifndef __MSM_ISP_UTIL_H__
#define __MSM_ISP_UTIL_H__

#include "msm_isp.h"

/* #define CONFIG_MSM_ISP_DBG 1 */

#ifdef CONFIG_MSM_ISP_DBG
#define ISP_DBG(fmt, args...) printk(fmt, ##args)
#else
#define ISP_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

void msm_isp_gettimeofday(struct timeval *tv);

int msm_isp_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub);

int msm_isp_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub);

int msm_isp_proc_cmd(struct vfe_device *vfe_dev, void *arg);
int msm_isp_send_event(struct vfe_device *vfe_dev,
	uint32_t type, struct msm_isp_event_data *event_data);
int msm_isp_cal_word_per_line(uint32_t output_format,
	uint32_t pixel_per_line);
irqreturn_t msm_isp_process_irq(int irq_num, void *data);
void msm_isp_set_src_state(struct vfe_device *vfe_dev, void *arg);
void msm_isp_do_tasklet(unsigned long data);

int msm_isp_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
int msm_isp_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
long msm_isp_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);
#endif /* __MSM_ISP_UTIL_H__ */
