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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/atomic.h>

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/wait.h>


#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>

#include "mtk_rsc.h"
#include "mtk_rsc-core.h"
#include "linux/soc/mediatek/mtk-cmdq.h"
#include "soc/mediatek/smi.h"
#include "smi_public.h"
#include "mtk-hcp.h"

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif

#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#else /* CONFIG_MTK_IOMMU_V2 */
#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */
#include "mach/pseudo_m4u.h"

#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define RSC_DEV_NAME        "mtk-rsc-v4l2"

/*
 *   IRQ signal mask
 */
#define INT_ST_MASK_RSC    (RSC_INT_ST)

#define RSC_START          (0x1)
#define RSC_ENABLE         (0x1)
#define RSC_IS_BUSY        (0x2)

/*SW Access Registers : using mapped base address from DTS*/
#define RSC_RST_REG(rsc_reg)                    (rsc_reg)
#define RSC_START_REG(rsc_reg)                  (rsc_reg + 0x04)
#define RSC_INT_STATUS_REG(rsc_reg)             (rsc_reg + 0x14)
#define RSC_STA_0_REG(rsc_reg)                  (rsc_reg + 0x100)

#ifndef M4U_PORT_L20_IPE_RSC_RDMA0_DISP
#define M4U_PORT_L20_IPE_RSC_RDMA0_DISP M4U_PORT_L20_IPE_RSC_RDMA0
#endif /* M4U_PORT_L20_IPE_RSC_RDMA0_DISP */

#ifndef M4U_PORT_L20_IPE_RSC_WDMA_DISP
#define M4U_PORT_L20_IPE_RSC_WDMA_DISP M4U_PORT_L20_IPE_RSC_WDMA
#endif /* M4U_PORT_L20_IPE_RSC_WDMA_DISP */

static irqreturn_t isp_irq_rsc(int irq, void *data);

#ifndef CONFIG_OF
const struct isr_table rsc_irq_cb_tbl[RSC_IRQ_TYPE_AMOUNT] = {
	{isp_irq_rsc, RSC_IRQ_BIT_ID, "rsc"},
};

#else
/* int number is got from kernel api */
const struct isr_table rsc_irq_cb_tbl[RSC_IRQ_TYPE_AMOUNT] = {
	{isp_irq_rsc, 0, "rsc"},
};
#endif

#ifdef CONFIG_MTK_IOMMU_V2
static int RSC_MEM_USE_VIRTUL = 1;
#endif

static int rsc_enqueue_work(void *data)
{
	struct rsc_device *rsc_hw_dev = NULL;
	struct rsc_device_ctx  *rsc_ctx = NULL;
	int ret = 0;

	rsc_hw_dev = (struct rsc_device *)data;
	rsc_ctx = &rsc_hw_dev->rsc_ctx;

	while (1) {
		ret = wait_event_interruptible
			(rsc_ctx->enqueue_thread.wq,
			 (atomic_read(&rsc_ctx->enqueue_param.queue_cnt) > 0 ||
			 kthread_should_stop()));

		if (kthread_should_stop())
			break;

		if (ret == ERESTARTSYS) {
			dev_dbg(&rsc_hw_dev->pdev->dev,
				"interrupted by a signal!\n");
			continue;
		}

		spin_lock(&rsc_ctx->enqueue_param.lock);
		atomic_dec(&rsc_ctx->enqueue_param.queue_cnt);
		spin_unlock(&rsc_ctx->enqueue_param.lock);

		spin_lock(&rsc_ctx->dequeue_param.lock);
		memcpy(&rsc_ctx->dequeue_param.frameparams,
			&rsc_ctx->enqueue_param.frameparams,
			sizeof(struct v4l2_rsc_frame_param));
		atomic_inc(&rsc_ctx->dequeue_param.queue_cnt);
		spin_unlock(&rsc_ctx->dequeue_param.lock);

		ret = mtk_hcp_send_async(rsc_hw_dev->mtk_hcp_pdev,
			HCP_RSC_FRAME_ID, &rsc_ctx->enqueue_param.frameparams,
			sizeof(struct v4l2_rsc_frame_param));

	}

	dev_dbg(&rsc_hw_dev->pdev->dev, " %s -X frame_id:%d\n",
		__func__, rsc_ctx->enqueue_param.frameparams.frame_id);

	return ret;
}

static int rsc_dequeue_work(void *data)
{
	struct mtk_rsc_ctx_finish_param fram_param;
	struct rsc_device *rsc_hw_dev = NULL;
	struct rsc_device_ctx  *rsc_ctx = NULL;
	struct mtk_rsc_ctx *dev_ctx;
	struct mtk_isp_rsc_drv_data *drv_data;
	int ret = 0;

	rsc_hw_dev = (struct rsc_device *)data;
	rsc_ctx = &rsc_hw_dev->rsc_ctx;

	drv_data = mtk_rsc_hw_dev_to_drv(rsc_hw_dev);
	dev_ctx = &drv_data->rsc_v4l2_dev.ctx;

	while (1) {
		ret = wait_event_interruptible
			(rsc_ctx->dequeue_thread.wq,
			 (atomic_read(&rsc_ctx->dequeue_param.queue_cnt) > 0 ||
			 kthread_should_stop()));

		if (kthread_should_stop())
			break;

		if (ret == ERESTARTSYS) {
			dev_dbg(&rsc_hw_dev->pdev->dev,
				"interrupted by a signal!\n");
			continue;
		}

		spin_lock(&rsc_ctx->dequeue_param.lock);
		atomic_dec(&rsc_ctx->dequeue_param.queue_cnt);
		spin_unlock(&rsc_ctx->dequeue_param.lock);

		fram_param.state = MTK_RSC_CTX_FRAME_DATA_DONE;
		fram_param.frame_id =
			rsc_ctx->dequeue_param.frameparams.frame_id;
		dev_dbg(&rsc_hw_dev->pdev->dev, "%s frame_id:%d done\n",
			__func__, rsc_ctx->dequeue_param.frameparams.frame_id);
		mtk_rsc_ctx_core_job_finish(dev_ctx, &fram_param);
	}
	return ret;
}

static irqreturn_t isp_irq_rsc(int irq, void *data)
{
	struct rsc_device *rsc_hw_dev = (struct rsc_device *)data;
	struct rsc_device_ctx  *rsc_ctx = NULL;
	unsigned int rsc_sta_0;
	unsigned int RscStatus;

	rsc_ctx = &rsc_hw_dev->rsc_ctx;

	/* RSC Status */
	RscStatus = readl(RSC_INT_STATUS_REG(rsc_hw_dev->regs));

	dev_dbg(&rsc_hw_dev->pdev->dev, "%s RscStatus = %d\n",
		__func__, RscStatus);

	if (RSC_INT_ST == (RSC_INT_ST & RscStatus)) {
		/* Update the frame status. */
#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("rsc_irq");
#endif
		rsc_sta_0 = readl(RSC_STA_0_REG(rsc_hw_dev->regs));

		dev_info(&rsc_hw_dev->pdev->dev,
			"%s [frame_id=%d] rsc_sta_0 = %d\n",
			__func__, rsc_ctx->dequeue_param.frameparams.frame_id,
			rsc_sta_0);

		memcpy((void *)rsc_ctx->dequeue_param.frameparams.meta_out.va,
			&rsc_sta_0, sizeof(unsigned int));

#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}

	wake_up_interruptible(&rsc_ctx->dequeue_thread.wq);

	return IRQ_HANDLED;
}

static int config_hw_handler(void *data, unsigned int len, void *priv)
{
	struct rsc_device *rsc_hw_dev = (struct rsc_device *)priv;
	uint32_t regData[30][2];
	int i = 0;

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	unsigned int w_imgi, h_imgi, w_mvio, h_mvio, w_bvo, h_bvo;
	unsigned int dma_bandwidth, trig_num;
#endif

	if (data == NULL || len == 0)
		return -1;

	memcpy(regData, data, len);

	rsc_hw_dev->pkt = cmdq_pkt_create(rsc_hw_dev->cmdq_clt);

	for (i = 0; i < 29; i++) {
		cmdq_pkt_write(rsc_hw_dev->pkt,
			rsc_hw_dev->cmdq_base, regData[i][0],
			regData[i][1], ~0);
	}

	cmdq_pkt_wfe(rsc_hw_dev->pkt, rsc_hw_dev->cmdq_event_id);
	cmdq_pkt_write(rsc_hw_dev->pkt, rsc_hw_dev->cmdq_base, regData[29][0],
		regData[29][1], ~0);

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	trig_num = (regData[1][1] & 0x00000F00) >> 8;
	w_imgi = regData[2][1] & 0x000001FF;
	h_imgi = (regData[2][1] & 0x01FF0000) >> 16;

	w_mvio = ((w_imgi + 1) >> 1) - 1;
	w_mvio = ((w_mvio / 7) << 4) + (((((w_mvio % 7) + 1) * 18) + 7) >> 3);
	h_mvio = (h_imgi + 1) >> 1;

	w_bvo =  (w_imgi + 1) >> 1;
	h_bvo =  (h_imgi + 1) >> 1;

	dma_bandwidth = ((w_imgi * h_imgi) * 2 + (w_mvio * h_mvio) * 2 * 16 +
			(w_bvo * h_bvo)) * trig_num * 30 / 1000000;

	pm_qos_update_request(&rsc_hw_dev->rsc_pm_qos_request, dma_bandwidth);
#endif
	cmdq_pkt_flush(rsc_hw_dev->pkt);
	cmdq_pkt_destroy(rsc_hw_dev->pkt);

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_update_request(&rsc_hw_dev->rsc_pm_qos_request, 0);
#endif
	dev_dbg(&rsc_hw_dev->pdev->dev, "%s configure hw done", __func__);
	return 0;
}

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(struct rsc_device *rsc_hw_dev)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;

	/* LARB19 */
	int count_of_ports = 0;
	int i = 0;

	count_of_ports = M4U_PORT_L20_IPE_RSC_WDMA_DISP -
		M4U_PORT_L20_IPE_RSC_RDMA0_DISP + 1;

	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L20_IPE_RSC_RDMA0_DISP+i;
		sPort.Virtuality = RSC_MEM_USE_VIRTUL;
		dev_dbg(&rsc_hw_dev->pdev->dev,
			"config M4U Port ePortID=%d\n", sPort.ePortID);
#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);
		if (ret == 0) {
			dev_dbg(&rsc_hw_dev->pdev->dev,
				"config M4U Port %s to %s SUCCESS\n",
				iommu_get_port_name(sPort.ePortID),
				RSC_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			dev_dbg(&rsc_hw_dev->pdev->dev,
				"config M4U Port %s to %s FAIL(ret=%d)\n",
				iommu_get_port_name(sPort.ePortID),
				RSC_MEM_USE_VIRTUL ? "virtual" : "physical",
				ret);
			ret = -1;
		}
#endif
	}
	return ret;
}
#endif

static inline void rsc_enable_ccf_clock(struct rsc_device *rsc_hw_dev)
{
	int ret = 0;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s.\n", __func__);

	if (rsc_hw_dev->is_hw_enable)
		return;

	smi_bus_prepare_enable(SMI_LARB20, RSC_DEV_NAME);
	ret = clk_prepare_enable(rsc_hw_dev->rsc_clk.CG_IPESYS_RSC);
	if (ret)
		dev_dbg(&rsc_hw_dev->pdev->dev,
			"cannot prepare and enable CG_IPESYS_RSC clock\n");

	if (rsc_hw_dev->is_needed_reset_hw == MTRUE) {
		/* Reset RSC flow */
		RSC_WR32(0x1, RSC_RST_REG(rsc_hw_dev->regs));
		dev_dbg(&rsc_hw_dev->pdev->dev, "RSC start reset...\n");
		while ((readl(RSC_RST_REG(rsc_hw_dev->regs)) & 0x02) != 0x2)
			dev_dbg(&rsc_hw_dev->pdev->dev, "RSC resetting...\n");
		RSC_WR32(0x11, RSC_RST_REG(rsc_hw_dev->regs));
		RSC_WR32(0x10, RSC_RST_REG(rsc_hw_dev->regs));
		RSC_WR32(0x0, RSC_RST_REG(rsc_hw_dev->regs));
		RSC_WR32(0, RSC_START_REG(rsc_hw_dev->regs));
	}

#ifdef CONFIG_MTK_IOMMU_V2
	/* set iommu port */
	ret = m4u_control_iommu_port(rsc_hw_dev);
	if (ret)
		dev_dbg(&rsc_hw_dev->pdev->dev,
			"cannot config M4U IOMMU PORTS\n");
#endif
	rsc_hw_dev->is_hw_enable = MTRUE;

}

static inline void rsc_disable_ccf_clock(struct rsc_device *rsc_hw_dev)
{
	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s.\n", __func__);
	if (!rsc_hw_dev->is_hw_enable)
		return;

	clk_disable_unprepare(rsc_hw_dev->rsc_clk.CG_IPESYS_RSC);
	smi_bus_disable_unprepare(SMI_LARB20, RSC_DEV_NAME);

	rsc_hw_dev->is_hw_enable = MFALSE;
}

static void rsc_set_clock(struct rsc_device *rsc_hw_dev, bool En)
{
	if (En) {
		/* Enable clock. */
		rsc_enable_ccf_clock(rsc_hw_dev);
	} else {
		/* Disable clock. */
		rsc_disable_ccf_clock(rsc_hw_dev);
	}
}


int mtk_rsc_open(struct platform_device *pdev)
{
	struct mtk_isp_rsc_drv_data *rsc_drv;
	struct rsc_device *rsc_hw_dev;
	struct resource *rsc_resource = &pdev->resource[0];
	int ret = 0;
	s32 usercount = 0;

	dev_dbg(&pdev->dev, "- E. %s.\n", __func__);

	rsc_drv = dev_get_drvdata(&pdev->dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;
	usercount = atomic_inc_return(&rsc_hw_dev->user_count);

	if (usercount == 1) {
		struct v4l2_rsc_init_param init_info;

		dev_dbg(&rsc_hw_dev->pdev->dev,
			"first user and open device.\n");

		memset(&init_info, 0, sizeof(struct v4l2_rsc_init_param));
		init_info.reg_base = rsc_resource->start;
		init_info.reg_range = resource_size(rsc_resource);

		/* init enqeueu thread */
		dev_dbg(&rsc_hw_dev->pdev->dev, "init enqueue work thread\n");
		if (!rsc_hw_dev->rsc_ctx.enqueue_thread.thread) {
			init_waitqueue_head(
				&rsc_hw_dev->rsc_ctx.enqueue_thread.wq);
			rsc_hw_dev->rsc_ctx.enqueue_thread.thread =
				kthread_run(rsc_enqueue_work,
					(void *)rsc_hw_dev,
					"rsc_enqueue_work");
			if (IS_ERR(rsc_hw_dev->rsc_ctx.enqueue_thread.thread)) {
				dev_info(&rsc_hw_dev->pdev->dev,
					"unable to alloc kthread\n");
				rsc_hw_dev->rsc_ctx.enqueue_thread.thread =
					NULL;
				return -ENOMEM;
			}
		}

		atomic_set(&rsc_hw_dev->rsc_ctx.enqueue_param.queue_cnt, 0);
		spin_lock_init(&rsc_hw_dev->rsc_ctx.enqueue_param.lock);

		/* init deqeueu thread */
		dev_dbg(&rsc_hw_dev->pdev->dev, "init dequeue work thread\n");
		if (!rsc_hw_dev->rsc_ctx.dequeue_thread.thread) {
			init_waitqueue_head(
				&rsc_hw_dev->rsc_ctx.dequeue_thread.wq);
			rsc_hw_dev->rsc_ctx.dequeue_thread.thread =
				kthread_run(rsc_dequeue_work,
					(void *)rsc_hw_dev,
					"rsc_dequeue_work");
			if (IS_ERR(rsc_hw_dev->rsc_ctx.dequeue_thread.thread)) {
				dev_info(&rsc_hw_dev->pdev->dev,
					"unable to alloc kthread\n");
				rsc_hw_dev->rsc_ctx.dequeue_thread.thread =
					NULL;
				return -ENOMEM;
			}
		}

		atomic_set(&rsc_hw_dev->rsc_ctx.dequeue_param.queue_cnt, 0);
		spin_lock_init(&rsc_hw_dev->rsc_ctx.dequeue_param.lock);

		/* get hcp platform device for later register event handler */
		rsc_hw_dev->mtk_hcp_pdev =
			mtk_hcp_get_plat_device(rsc_hw_dev->pdev);
		if (!rsc_hw_dev->mtk_hcp_pdev) {
			dev_info(&rsc_hw_dev->pdev->dev,
				"Failed to get HCP device\n");
			return -EINVAL;
		}

		/* register frame handle callback function */
		mtk_hcp_register(rsc_hw_dev->mtk_hcp_pdev,
			     HCP_RSC_FRAME_ID,
			     (hcp_handler_t)config_hw_handler,
			     "config_hw_handler", rsc_hw_dev);

		mtk_hcp_send_async(rsc_hw_dev->mtk_hcp_pdev,
			HCP_RSC_INIT_ID, &init_info,
			sizeof(struct v4l2_rsc_init_param));

		rsc_hw_dev->is_needed_reset_hw = MTRUE;

		/* notify pm */
		pm_runtime_get_sync(&pdev->dev);
	}

	dev_dbg(&rsc_hw_dev->pdev->dev,
		"- X. %s ret:%d user_count:%d\n",
		__func__, ret, atomic_read(&rsc_hw_dev->user_count));

	return ret;
}
EXPORT_SYMBOL(mtk_rsc_open);

int mtk_rsc_streamon(struct platform_device *pdev, u16 id)
{
	int ret = 0;
	struct rsc_device *rsc_hw_dev;
	struct mtk_isp_rsc_drv_data *rsc_drv;

	rsc_drv = dev_get_drvdata(&pdev->dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s\n", __func__);
	rsc_hw_dev->streaming = RSC_STREAM_STATE_STREAMON;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- X. %s\n", __func__);
	return ret;
}
EXPORT_SYMBOL(mtk_rsc_streamon);

int mtk_rsc_enqueue(struct platform_device *pdev,
		   struct v4l2_rsc_frame_param *frameparams)
{
	struct rsc_device *rsc_hw_dev;
	struct rsc_device_ctx *rsc_ctx;
	struct mtk_isp_rsc_drv_data *rsc_drv;

	rsc_drv = dev_get_drvdata(&pdev->dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;
	rsc_ctx = &rsc_hw_dev->rsc_ctx;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s\n", __func__);

	spin_lock(&rsc_ctx->enqueue_param.lock);
	memcpy(&rsc_ctx->enqueue_param.frameparams, frameparams,
		sizeof(struct v4l2_rsc_frame_param));
	atomic_inc(&rsc_ctx->enqueue_param.queue_cnt);
	spin_unlock(&rsc_ctx->enqueue_param.lock);

	wake_up_interruptible(&rsc_ctx->enqueue_thread.wq);

	dev_dbg(&rsc_hw_dev->pdev->dev, "- X. %s\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_rsc_enqueue);

int mtk_rsc_release(struct platform_device *pdev)
{
	int ret = 0;
	struct rsc_device *rsc_hw_dev;
	struct mtk_isp_rsc_drv_data *rsc_drv;

	rsc_drv = dev_get_drvdata(&pdev->dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s\n", __func__);

	if (atomic_dec_and_test(&rsc_hw_dev->user_count)) {
		/* Disable clock. */
		pm_runtime_put_sync(&rsc_hw_dev->pdev->dev);

		/* stop enqueue thread. */
		if (!IS_ERR(rsc_hw_dev->rsc_ctx.enqueue_thread.thread)) {
			kthread_stop(
				rsc_hw_dev->rsc_ctx.enqueue_thread.thread);
			wake_up_interruptible(
				&rsc_hw_dev->rsc_ctx.enqueue_thread.wq);
			rsc_hw_dev->rsc_ctx.enqueue_thread.thread = NULL;
		}

		/* stop dequeue thread. */
		if (!IS_ERR(rsc_hw_dev->rsc_ctx.dequeue_thread.thread)) {
			kthread_stop(
				rsc_hw_dev->rsc_ctx.dequeue_thread.thread);
			wake_up_interruptible(
				&rsc_hw_dev->rsc_ctx.dequeue_thread.wq);
			rsc_hw_dev->rsc_ctx.dequeue_thread.thread = NULL;
		}

		mtk_hcp_unregister(rsc_hw_dev->mtk_hcp_pdev, HCP_RSC_FRAME_ID);

		rsc_hw_dev->is_needed_reset_hw = MFALSE;
	}

	dev_dbg(&rsc_hw_dev->pdev->dev,
		"%s Curr user_count(%d), (process, pid, tgid)=(%s, %d, %d)\n",
		__func__, atomic_read(&rsc_hw_dev->user_count), current->comm,
		current->pid, current->tgid);

	return ret;
}
EXPORT_SYMBOL(mtk_rsc_release);

int mtk_rsc_streamoff(struct platform_device *pdev, u16 id)
{
	int ret = 0;
	struct rsc_device *rsc_hw_dev;
	struct mtk_isp_rsc_drv_data *rsc_drv;

	rsc_drv = dev_get_drvdata(&pdev->dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. %s\n", __func__);

	rsc_hw_dev->streaming = RSC_STREAM_STATE_STREAMOFF;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- X. %s\n", __func__);
	return ret;
}
EXPORT_SYMBOL(mtk_rsc_streamoff);

static struct mtk_rsc_ctx_desc mtk_isp_ctx_desc_rsc = {
	"proc_device_rsc", mtk_isp_ctx_rsc_init,};

static int mtk_rsc_probe(struct platform_device *pdev)
{
	struct mtk_isp_rsc_drv_data *rsc_drv;
	struct rsc_device *rsc_hw_dev;
	struct device_node *node;
	struct platform_device *larb_pdev;

	signed int ret = 0;
	signed int i = 0;
	unsigned int irq_info[3];

	dev_info(&pdev->dev, "- E. RSC driver probe.\n");

	rsc_drv = devm_kzalloc(&pdev->dev, sizeof(struct mtk_isp_rsc_drv_data),
		GFP_KERNEL);

	if (!rsc_drv) {
		dev_dbg(&pdev->dev, "Unable to allocate RSC_devs\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, rsc_drv);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;
	rsc_hw_dev->pdev = pdev;

	rsc_hw_dev->is_needed_reset_hw = MFALSE;
	rsc_hw_dev->is_hw_enable = MFALSE;

	/* iomap registers */
	rsc_hw_dev->regs = of_iomap(pdev->dev.of_node, 0);

	if (!rsc_hw_dev->regs) {
		dev_info(&pdev->dev,
			"Unable to ioremap registers, devnode(%s).\n",
			pdev->dev.of_node->name);
		return -ENOMEM;
	}

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	*(rsc_hw_dev->pdev->dev.dma_mask) =
		(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
	rsc_hw_dev->pdev->dev.coherent_dma_mask =
		(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif

	/* get IRQ ID and request IRQ */
	rsc_hw_dev->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (rsc_hw_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
			(pdev->dev.of_node,
			"interrupts",
			irq_info,
			ARRAY_SIZE(irq_info))) {
			dev_info(&pdev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < RSC_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pdev->dev.of_node->name,
				rsc_irq_cb_tbl[i].device_name) == 0) {
				ret = request_irq(rsc_hw_dev->irq,
						rsc_irq_cb_tbl[i].isr_fp,
						irq_info[2],
						rsc_irq_cb_tbl[i].device_name,
						rsc_hw_dev);
				if (ret) {
					dev_dbg(&pdev->dev,
						"request IRQ fail devnode(%s)",
						pdev->dev.of_node->name);
					dev_dbg(&pdev->dev,
						"irq=%d isr:%s\n",
						rsc_hw_dev->irq,
						rsc_irq_cb_tbl[i].device_name);

					return ret;
				}

				dev_info(&pdev->dev,
					"devnode(%s), irq=%d, ISR: %s\n",
					pdev->dev.of_node->name,
					rsc_hw_dev->irq,
					rsc_irq_cb_tbl[i].device_name);
				break;
			}
		}
	} else {
		dev_info(&pdev->dev, "No IRQ!!: devnode(%s), irq=%d\n",
			pdev->dev.of_node->name, rsc_hw_dev->irq);
	}

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!node) {
		dev_info(&pdev->dev, "no mediatek,larb found");
		return -EINVAL;
	}
	larb_pdev = of_find_device_by_node(node);
	if (!larb_pdev) {
		dev_info(&pdev->dev, "no mediatek,larb device found");
		return -EINVAL;
	}
	rsc_hw_dev->larb_dev = &larb_pdev->dev;

	rsc_hw_dev->rsc_clk.CG_IPESYS_RSC =
		devm_clk_get(&pdev->dev, "RSC_CLK_IPE_RSC");

	if (IS_ERR(rsc_hw_dev->rsc_clk.CG_IPESYS_RSC)) {
		dev_info(&pdev->dev, "cannot get CG_IPESYS_RSC clock\n");
		return PTR_ERR(rsc_hw_dev->rsc_clk.CG_IPESYS_RSC);
	}

	/* init rsc user count */
	atomic_set(&rsc_hw_dev->user_count, 0);

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_add_request(&rsc_hw_dev->rsc_pm_qos_request,
			PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
#endif
	/* init cmdq */
	rsc_hw_dev->cmdq_base = cmdq_register_device(&pdev->dev);
	rsc_hw_dev->cmdq_clt = cmdq_mbox_create(&pdev->dev, 0);
	rsc_hw_dev->cmdq_event_id = cmdq_dev_get_event(&pdev->dev, "rsc_eof");

	rsc_hw_dev->streaming = RSC_STREAM_STATE_STREAMOFF;

	/* initialize the v4l2 common part */
	ret = mtk_rsc_dev_core_init(pdev, &rsc_drv->rsc_v4l2_dev,
		&mtk_isp_ctx_desc_rsc);

	if (ret)
		dev_info(&pdev->dev, "v4l2 init failed: %d\n", ret);

	pm_runtime_enable(&pdev->dev);

	dev_info(&pdev->dev, "- X. RSC driver probe.");

	return 0;
}

static int mtk_rsc_remove(struct platform_device *pdev)
{
	struct mtk_isp_rsc_drv_data *drv_data = dev_get_drvdata(&pdev->dev);
	struct rsc_device *rsc_hw_dev = NULL;
	signed int irq_num;

	rsc_hw_dev = &drv_data->rsc_hw_dev;

	dev_dbg(&rsc_hw_dev->pdev->dev, "- E. RSC driver remove.");

	mtk_rsc_dev_core_release(pdev, &drv_data->rsc_v4l2_dev);

	/* Release IRQ */
	disable_irq(rsc_hw_dev->irq_num);
	irq_num = platform_get_irq(pdev, 0);
	free_irq(irq_num, NULL);

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_remove_request(&rsc_hw_dev->rsc_pm_qos_request);
#endif
	pm_runtime_disable(&pdev->dev);

	kfree(rsc_hw_dev);
	return 0;
}

static int mtk_rsc_suspend(struct device *dev)
{
	struct mtk_isp_rsc_drv_data *rsc_drv;
	struct platform_device *pdev;
	struct rsc_device *rsc_hw_dev;

	if (pm_runtime_suspended(dev))
		return 0;

	rsc_drv = dev_get_drvdata(dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;
	pdev = rsc_hw_dev->pdev;

	rsc_set_clock(rsc_hw_dev, MFALSE);

	dev_dbg(&pdev->dev, "X.%s\n", __func__);
	return 0;
}

static int mtk_rsc_resume(struct device *dev)
{
	struct mtk_isp_rsc_drv_data *rsc_drv;
	struct platform_device *pdev;
	struct rsc_device *rsc_hw_dev;

	if (pm_runtime_suspended(dev))
		return 0;

	rsc_drv = dev_get_drvdata(dev);
	rsc_hw_dev = &rsc_drv->rsc_hw_dev;
	pdev = rsc_hw_dev->pdev;

	if (atomic_read(&rsc_hw_dev->user_count) > 0)
		rsc_set_clock(rsc_hw_dev, MTRUE);

	dev_dbg(&pdev->dev, "X.%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops mtk_rsc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_rsc_suspend, mtk_rsc_resume)
	SET_RUNTIME_PM_OPS(mtk_rsc_suspend, mtk_rsc_resume, NULL)
};

static const struct of_device_id mtk_rsc_of_ids[] = {
	{.compatible = "mediatek,rsc",},
	{}
};

static struct platform_driver mtk_rsc_driver = {
	.probe   = mtk_rsc_probe,
	.remove  = mtk_rsc_remove,
	.driver  = {
		.name  = RSC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mtk_rsc_of_ids,
		.pm = &mtk_rsc_pm_ops,
	}
};
module_platform_driver(mtk_rsc_driver);

MODULE_DESCRIPTION("Mediatek RSC driver");
MODULE_LICENSE("GPL");
