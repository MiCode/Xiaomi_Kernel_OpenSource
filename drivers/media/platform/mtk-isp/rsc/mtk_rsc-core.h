/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_RSC_CORE_H__
#define __MTK_RSC_CORE_H__

#define SIG_ERESTARTSYS 512

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#endif

#include <linux/io.h>

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

#define RSC_PMQOS_EN
#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#endif

#include "mtk_rsc-dev.h"
#include "mtk_rsc-ctx.h"

#define RSC_WR32(v, a) \
do { \
	__raw_writel((v), (void __force __iomem *)((a))); \
	mb(); /* ensure written */ \
} while (0)

#define RSC_RD32(addr)    ioread32((void *)addr)

#define SUPPORT_MAX_RSC_FRAME_REQUEST      6
#define SUPPORT_MAX_RSC_REQUEST_RING_SIZE  4
#define RSC_INT_ST                         (1<<0)

enum  rsc_irq_type {
	RSC_IRQ_TYPE_INT_RSC_ST,	/* RSC */
	RSC_IRQ_TYPE_AMOUNT
};

enum rsc_stream_state {
	RSC_STREAM_STATE_INIT	= 0,
	RSC_STREAM_STATE_STREAMON,
	RSC_STREAM_STATE_STREAMOFF
};

enum rsc_process_id {
	RSC_PROCESS_ID_NONE,
	RSC_PROCESS_ID_RSC,
	RSC_PROCESS_ID_AMOUNT
};

struct rsc_clk_struct {
	struct clk *CG_IPESYS_RSC;
};

struct RSC_Request {
	unsigned int                   req_num;
	unsigned int                   frame_id;
	struct v4l2_rsc_frame_param    *rsc_config;
};

struct isr_table {
	irq_handler_t isr_fp;
	unsigned int int_number;
	char device_name[16];
};

struct  rsc_user_info {
	pid_t p_id;
	pid_t t_id;
};

struct rsc_queue {
	struct v4l2_rsc_frame_param frameparams;
	atomic_t queue_cnt;
	spinlock_t lock; /* queue attributes protection */
};

struct rsc_thread {
	struct task_struct *thread;
	wait_queue_head_t wq;
};

struct rsc_device_ctx {
	struct rsc_queue enqueue_param;
	struct rsc_thread enqueue_thread;
	struct rsc_queue dequeue_param;
	struct rsc_thread dequeue_thread;
	spinlock_t spin_lock_irq[RSC_IRQ_TYPE_AMOUNT];
};

struct rsc_device {
	struct platform_device *pdev;
	struct device *larb_dev;
	struct rsc_clk_struct  rsc_clk;
	struct rsc_device_ctx  rsc_ctx;
	enum rsc_stream_state  streaming;

	/* for V4L2 common driver  */
	void (*v4l2cb)(void *data, struct platform_device *pdev);
	struct platform_device *v4l2_pdev;
	/* for mtk_hcp  driver  */
	struct platform_device *mtk_hcp_pdev;
    /* for pm qos */
#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	struct pm_qos_request rsc_pm_qos_request;
#endif
	/* for gce */
	struct cmdq_base       *cmdq_base;
	struct cmdq_client     *cmdq_clt;
	struct cmdq_pkt        *pkt;
	s32                    cmdq_event_id;
	/* for others */
	void __iomem           *regs;
	atomic_t               user_count;	/* User Count */
	u32                    debug_mask;	/* Debug Mask */
	u32                    irq_num;
	u32                    irq;
	bool                   is_needed_reset_hw;
	bool                   is_hw_enable;
};

struct mtk_isp_rsc_drv_data {
	struct mtk_rsc_dev rsc_v4l2_dev;
	struct rsc_device  rsc_hw_dev;
} __packed;

static inline struct rsc_device *get_rsc_hw_device(struct device *dev)
{
	struct mtk_isp_rsc_drv_data *drv_data =
		dev_get_drvdata(dev);
	if (drv_data)
		return &drv_data->rsc_hw_dev;
	else
		return NULL;
}

#define mtk_rsc_us_to_jiffies(us) \
	((((unsigned long)(us) / 1000) * HZ + 512) >> 10)

#define mtk_rsc_hw_dev_to_drv(__rsc_hw_dev) \
	container_of(__rsc_hw_dev, \
	struct mtk_isp_rsc_drv_data, rsc_hw_dev)

#define mtk_rsc_ctx_to_drv(__rsc_ctx) \
	container_of(__rsc_ctx, \
	struct mtk_isp_rsc_drv_data, rsc_hw_dev.rsc_ctx)

#endif/*__MTK_RSC_CORE_H__*/
