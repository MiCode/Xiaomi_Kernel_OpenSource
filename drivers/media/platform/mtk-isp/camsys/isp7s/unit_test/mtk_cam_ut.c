// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Bibby Hsieh <bibby.hsieh@mediatek.com>
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/mtk_ccd_controls.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "mtk_cam_ut.h"
#include "mtk_isp_ut_ioctl.h"
#include "mtk_cam_ut-engines.h"

#define CAM_DEV_NAME "mtk_cam_ut"

static int debug_testmdl_pixmode = -1;
module_param(debug_testmdl_pixmode, int, 0644);
MODULE_PARM_DESC(debug_testmdl_pixmode, "fixed pixel mode for testmdl");

static int apply_next_req(struct mtk_cam_ut *ut)
{
	struct mtk_cam_ut_buf_entry *buf_entry;
	unsigned long flags;

	spin_lock_irqsave(&ut->enque_list.lock, flags);
	if (list_empty(&ut->enque_list.list)) {
		dev_info(ut->dev, "no req to apply\n");
		spin_unlock_irqrestore(&ut->enque_list.lock, flags);
		return 0;
	}

	buf_entry = list_first_entry(&ut->enque_list.list,
				     struct mtk_cam_ut_buf_entry, list_entry);
	if (buf_entry->cq_buf.size == 0) {
		dev_dbg(ut->dev, "Composer handler is not finished yet\n");
		spin_unlock_irqrestore(&ut->enque_list.lock, flags);
		return 0;
	}

	if (!ut->with_testmdl) {
		spin_lock_irqsave(&ut->spinlock_irq, flags);
		if (!ut->m2m_available) {
			dev_info(ut->dev, "%s: m2m not avialable\n");
			spin_unlock_irqrestore(&ut->spinlock_irq, flags);
			return 0;
		}

		ut->m2m_available = 0;
		spin_unlock_irqrestore(&ut->spinlock_irq, flags);
	}

	list_del(&buf_entry->list_entry);
	ut->enque_list.cnt--;
	spin_unlock_irqrestore(&ut->enque_list.lock, flags);

#if SUPPORT_RAWB
	if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_ON_THE_FLY_RAWB) {
		CALL_RAW_OPS(ut->raw[1], apply_cq,
			     buf_entry->cq_buf.iova,
			     buf_entry->cq_buf.size,
			     buf_entry->cq_offset,
			     buf_entry->sub_cq_size,
			     buf_entry->sub_cq_offset);
	}
#endif
	CALL_RAW_OPS(ut->raw[0], apply_cq,
		     buf_entry->cq_buf.iova,
		     buf_entry->cq_buf.size,
		     buf_entry->cq_offset,
		     buf_entry->sub_cq_size,
		     buf_entry->sub_cq_offset);

	spin_lock_irqsave(&ut->processing_list.lock, flags);
	list_add_tail(&buf_entry->list_entry, &ut->processing_list.list);
	ut->processing_list.cnt++;
	spin_unlock_irqrestore(&ut->processing_list.lock, flags);

	return 0;
}

static int handle_req_done(struct mtk_cam_ut *ut)
{
	struct mtk_cam_ut_buf_entry *buf_entry;

	spin_lock(&ut->processing_list.lock);
	if (list_empty(&ut->processing_list.list)) {
		dev_info(ut->dev, "Duplicate SW P1 DONE, should not happen\n");
		spin_unlock(&ut->processing_list.lock);
		return 0;
	}

	buf_entry = list_first_entry(&ut->processing_list.list,
				     struct mtk_cam_ut_buf_entry, list_entry);
	list_del(&buf_entry->list_entry);
	ut->processing_list.cnt--;
	spin_unlock(&ut->processing_list.lock);

	spin_lock(&ut->deque_list.lock);
	list_add_tail(&buf_entry->list_entry, &ut->deque_list.list);
	ut->deque_list.cnt++;
	spin_unlock(&ut->deque_list.lock);

	wake_up(&ut->done_wq);
	return 0;
}

static int handle_req_done_m2m(struct mtk_cam_ut *ut)
{
	unsigned long flags;

	handle_req_done(ut);

	spin_lock_irqsave(&ut->spinlock_irq, flags);
	ut->m2m_available = 1;
	spin_unlock_irqrestore(&ut->spinlock_irq, flags);

	apply_next_req(ut);
	return 0;
}

static int on_ipi_composed(struct mtk_cam_ut *ut)
{
	return  0;
}

static int apply_req_on_composed_once(struct mtk_cam_ut *ut)
{
	struct mtk_ut_raw_initial_params raw_params;

	raw_params.subsample = ut->subsample;
	raw_params.streamon_type = STREAM_FROM_TG;
	raw_params.hardware_scenario = ut->hardware_scenario;

	CALL_RAW_OPS(ut->raw[0], initialize, &raw_params);
	//CALL_RAW_OPS(ut->raw[1], initialize, &raw_params);
	//CALL_RAW_OPS(ut->raw[2], initialize, &raw_params);

	ut->hdl.on_ipi_composed = on_ipi_composed;
	return apply_next_req(ut);
}

static int apply_req_on_composed_m2m_once(struct mtk_cam_ut *ut)
{
	struct mtk_ut_raw_initial_params raw_params;

	raw_params.subsample = ut->subsample;
	raw_params.streamon_type = STREAM_FROM_TG;

	if (!ut->with_testmdl) {
		if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER) {
			if (ut->main_rawi == MTKCAM_IPI_RAW_RAWI_5)
				raw_params.streamon_type = STREAM_FROM_RAWI_R5;
			else {
				dev_info(ut->dev, "can't find correct main rawi\n");
				return 0;
			}
		} else if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_OFFLINE_ADL) {
			raw_params.streamon_type = STREAM_FROM_ADLRD;
		} else
			raw_params.streamon_type = STREAM_FROM_RAWI_R2;
	}

	dev_info(ut->dev,
				 "streamon_type %d\n", raw_params.streamon_type);

	CALL_RAW_OPS(ut->raw[0], initialize, &raw_params);
	CALL_RAW_OPS(ut->raw[1], initialize, &raw_params);
	CALL_RAW_OPS(ut->raw[2], initialize, &raw_params);

	ut->hdl.on_ipi_composed = apply_next_req;
	return apply_next_req(ut);
}

static int streamon_on_cqdone_once(struct mtk_cam_ut *ut)
{
	int i;

	if (ut->hardware_scenario != MTKCAM_IPI_HW_PATH_DC_STAGGER)
		CALL_RAW_OPS(ut->raw[0], s_stream, 1)

	for (i = ut->is_dcif_camsv - 1; i >= 0; i--)
		CALL_CAMSV_OPS(ut->camsv[i], s_stream, 1);

	ut->hdl.on_isr_cq_done = NULL;
	return 0;
}

static int trigger_rawi(struct mtk_cam_ut *ut)
{
	CALL_RAW_OPS(ut->raw[0], s_stream, 1);
	return 0;
}

static void setup_hanlder(struct mtk_cam_ut *ut)
{
	memset(&ut->hdl, 0, sizeof(struct mtk_cam_ut_event_handler));

	dev_info(ut->dev,
				 "ut->with_testmdl %d\n", ut->with_testmdl);

	if (ut->with_testmdl) {
		if (!ut->is_dcif_camsv) {
			ut->hdl.on_ipi_composed = apply_req_on_composed_once;
			ut->hdl.on_isr_sof = apply_next_req;
			ut->hdl.on_isr_sv_sof = NULL;
			ut->hdl.on_isr_cq_done = streamon_on_cqdone_once;
			ut->hdl.on_isr_frame_done = handle_req_done;
		} else {
			ut->hdl.on_ipi_composed = apply_req_on_composed_once;
			ut->hdl.on_isr_sof = NULL;
			ut->hdl.on_isr_sv_sof = apply_next_req;
			ut->hdl.on_isr_cq_done = streamon_on_cqdone_once;
			ut->hdl.on_isr_frame_done = handle_req_done;
		}
	} else {
		if (!ut->is_dcif_camsv) {
			ut->hdl.on_ipi_composed = apply_req_on_composed_m2m_once;
			ut->hdl.on_isr_sof = NULL;
			ut->hdl.on_isr_sv_sof = NULL;
			ut->hdl.on_isr_cq_done = trigger_rawi;
			ut->hdl.on_isr_frame_done = handle_req_done_m2m;
		} else {
			dev_info(ut->dev,
				 "dcif without testmdl: not supported yet\n");
		}
	}
}

static int cam_of_rproc(struct mtk_cam_ut *ut)
{
	struct device *dev = ut->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,ccd",
				   &ut->rproc_phandle);
	if (ret) {
		dev_info(dev, "fail to get rproc_phandle:%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

static int cam_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct mtk_cam_ut *ut = (struct mtk_cam_ut *)priv;
	struct device *dev = ut->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ut_buf_entry *buf_entry;
	unsigned long flags;

	ipi_msg = (struct mtkcam_ipi_event *)data;
	if (!ipi_msg)
		return -EINVAL;

	if (len < offsetofend(struct mtkcam_ipi_event, ack_data)) {
		dev_dbg(dev, "wrong IPI len:%d\n", len);
		return -EINVAL;
	}

	if (ipi_msg->cmd_id != CAM_CMD_ACK ||
		(ipi_msg->ack_data.ack_cmd_id != CAM_CMD_FRAME &&
		ipi_msg->ack_data.ack_cmd_id != CAM_CMD_DESTROY_SESSION))
		return -EINVAL;

	if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_FRAME) {
		dev_dbg(dev, "%s, ack_cmd_id = CAM_CMD_FRAME\n", __func__);

		spin_lock_irqsave(&ut->enque_list.lock, flags);
		buf_entry = list_first_entry(&ut->enque_list.list,
				struct mtk_cam_ut_buf_entry, list_entry);
		buf_entry->cq_buf.size =
			ipi_msg->ack_data.frame_result.main.size;
		buf_entry->cq_offset =
			ipi_msg->ack_data.frame_result.main.offset;
		buf_entry->sub_cq_size =
			ipi_msg->ack_data.frame_result.sub.size;
		buf_entry->sub_cq_offset =
			ipi_msg->ack_data.frame_result.sub.offset;
		spin_unlock_irqrestore(&ut->enque_list.lock, flags);

		if (ut->hdl.on_ipi_composed)
			ut->hdl.on_ipi_composed(ut);
	}

	return 0;
}

static int cam_composer_init(struct mtk_cam_ut *ut)
{
	struct device *dev = ut->dev;
	struct mtk_ccd *ccd;
	struct rproc_subdev *rpmsg_subdev;
	struct rpmsg_channel_info *msg = &ut->rpmsg_channel;
	int ret;

	ut->rproc_handle = rproc_get_by_phandle(ut->rproc_phandle);
	if (!ut->rproc_handle) {
		dev_dbg(dev, "fail to get rproc_handle\n");
		return -EINVAL;
	}

	ret = rproc_boot(ut->rproc_handle);
	if (ret) {
		dev_dbg(dev, "failed to rproc_boot:%d\n", ret);
		goto fail_rproc_put;
	}

	ccd = (struct mtk_ccd *)ut->rproc_handle->priv;
	rpmsg_subdev = ccd->rpmsg_subdev;
	snprintf(msg->name, RPMSG_NAME_SIZE, "mtk-isp-unit-test\0\n");
	msg->src = CCD_IPI_ISP_MAIN;
	ut->rpmsg_dev = mtk_create_client_msgdevice(rpmsg_subdev, msg);
	if (!ut->rpmsg_dev) {
		ret = -EINVAL;
		goto fail_shutdown;
	}
	ut->rpmsg_dev->rpdev.ept = rpmsg_create_ept(&ut->rpmsg_dev->rpdev,
						    cam_composer_handler,
						    ut, *msg);
	if (IS_ERR(ut->rpmsg_dev->rpdev.ept)) {
		ret = -EINVAL;
		goto fail_shutdown;
	}

	return ret;
fail_shutdown:
	rproc_shutdown(ut->rproc_handle);
fail_rproc_put:
	rproc_put(ut->rproc_handle);
	ut->rproc_handle = NULL;
	return ret;
}

static void cam_composer_uninit(struct mtk_cam_ut *ut)
{
	struct mtk_ccd *ccd = ut->rproc_handle->priv;

	mtk_destroy_client_msgdevice(ccd->rpmsg_subdev, &ut->rpmsg_channel);
	ut->rpmsg_dev = NULL;
	rproc_shutdown(ut->rproc_handle);
	rproc_put(ut->rproc_handle);
	ut->rproc_handle = NULL;
}

static void ut_event_on_notify(struct ut_event_listener *listener,
			       struct ut_event_source *src,
			       struct ut_event event)
{
	struct mtk_cam_ut *ut = container_of(listener, struct mtk_cam_ut, listener);
	int mask = event.mask;

	dev_dbg(ut->dev, "%s: event 0x%x\n", __func__, mask);

	if ((mask & EVENT_SOF) && ut->hdl.on_isr_sof)
		ut->hdl.on_isr_sof(ut);

	if ((mask & EVENT_SV_SOF) && ut->hdl.on_isr_sv_sof)
		ut->hdl.on_isr_sv_sof(ut);

	if ((mask & EVENT_CQ_DONE) && ut->hdl.on_isr_cq_done)
		ut->hdl.on_isr_cq_done(ut);

	if ((mask & EVENT_SW_P1_DONE) && ut->hdl.on_isr_frame_done)
		ut->hdl.on_isr_frame_done(ut);
}

static int dequeue_buffer(struct mtk_cam_ut *ut, unsigned int timeout_ms)
{
	struct mtk_cam_ut_buf_entry *buf_entry;
	long timeout_jiff, ret;
	unsigned long flags;

	timeout_jiff = msecs_to_jiffies(timeout_ms);
	ret = wait_event_interruptible_timeout(ut->done_wq,
					       !list_empty(&ut->deque_list.list),
					       timeout_jiff);
	if (!ret) {
		dev_info(ut->dev, "deque time=%ums out\n",
			 timeout_ms);
		return -1;
	} else if (-ERESTARTSYS == ret) {
		dev_info(ut->dev, "deque interrupted by signal\n");
		return -1;
	}

	dev_info(ut->dev, "deque success, time left %ums\n",
		 jiffies_to_msecs(ret));

	spin_lock_irqsave(&ut->deque_list.lock, flags);
	buf_entry = list_first_entry(&ut->deque_list.list,
				     struct mtk_cam_ut_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ut->deque_list.cnt--;
	spin_unlock_irqrestore(&ut->deque_list.lock, flags);
	kfree(buf_entry);

	return 0;
}

static int set_test_mdl(struct mtk_cam_ut *ut,
			struct cam_ioctl_set_testmdl *testmdl)
{
	struct device *dev = ut->dev;
	struct device *seninf = ut->seninf;
	int width, height;
	int pixel_mode;
	int pattern;

	width = testmdl->width;
	height = testmdl->height;
	pattern = testmdl->pattern;

	pixel_mode = testmdl->pixmode_lg2;
	if (debug_testmdl_pixmode >= 0) {
		dev_info(dev, "DEBUG: set testmdl pixel mode (log2) %d\n",
			 debug_testmdl_pixmode);
		pixel_mode = debug_testmdl_pixmode;
	}

	if (testmdl->mode == testmdl_disable)
		dev_info(dev, "without testmdl\n");
	else {
		ut->with_testmdl = 1;
		// reset cam mux
		CALL_SENINF_OPS(seninf, reset);
	}

	switch (testmdl->hwScenario) {
#ifdef NOT_READY
	case MTKCAM_IPI_HW_PATH_STAGGER:
		if (testmdl->mode == testmdl_stagger_3exp) {
			ut->is_dcif_camsv = 2;
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_0, camsv_tg_0);
			mdelay(1);
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_1, camsv_tg_1);
			mdelay(1);
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_2, raw_tg_0);
		} else if (testmdl->mode == testlmdl_stagger_2exp) {
			ut->is_dcif_camsv = 1;
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_0, camsv_tg_0);
			mdelay(1);
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_1, raw_tg_0);
		}
		break;

	case MTKCAM_IPI_HW_PATH_DC_STAGGER:
		// temporarily support camsv_a1 + camsv_a2 + raw_a case
		if (testmdl->mode == testmdl_stagger_2exp) {
			ut->is_dcif_camsv = 2;
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_0, camsv_tg_0);
			mdelay(1);
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_1, camsv_tg_1);
		} else if (testmdl->mode == testmdl_normal) {
			ut->is_dcif_camsv = 1;
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_0, camsv_tg_0);
		}
		break;
#endif
#if SUPPORT_RAWB
	case MTKCAM_IPI_HW_PATH_ON_THE_FLY_RAWB:
		if (ut->with_testmdl == 1) {
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_mux_raw(seninf, 0),
					seninf_cammux_raw(seninf, 1));
		}
		break;
#endif
	case MTKCAM_IPI_HW_PATH_OTF_RGBW:
		width *= 2;
	default:
		if (ut->with_testmdl == 1) {
			CALL_SENINF_OPS(seninf, set_size,
					width, height,
					pixel_mode, pattern,
					seninf_mux_raw(seninf, 0),
					seninf_cammux_raw(seninf, 0));
		}
	}

	return 0;
}

#define LOG_CMD(cmd)	pr_info("%s, %d: %s\n", __func__, __LINE__, #cmd)

static long cam_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mtk_cam_ut *ut = filp->private_data;
	struct device *dev = ut->dev;

	switch (cmd) {
	case ISP_UT_IOCTL_SET_TESTMDL: {
		struct cam_ioctl_set_testmdl testmdl;

		LOG_CMD(ISP_UT_IOCTL_SET_TESTMDL);

		ut->is_dcif_camsv = 0;
		ut->with_testmdl = 0;

		if (copy_from_user(&testmdl, (void *)arg,
				   sizeof(struct cam_ioctl_set_testmdl)) != 0) {
			dev_dbg(dev, "Fail to get testmdl parameter\n");
			return -EFAULT;
		}

		// update hardware scenario
		ut->hardware_scenario = testmdl.hwScenario;

		dev_info(dev, "testmdl.mode=%d testmdl.hwScenario id=%d\n",
			testmdl.mode, HWPATH_ID(testmdl.hwScenario));

		set_test_mdl(ut, &testmdl);

		return 0;
	}
	case ISP_UT_IOCTL_DEQUE: {
		unsigned int timeout_ms;

		LOG_CMD(ISP_UT_IOCTL_DEQUE);

		if (copy_from_user(&timeout_ms, (void *)arg,
				   sizeof(unsigned int)) != 0) {
			dev_dbg(dev, "Fail to get deque info\n");
			return -EFAULT;
		}

		dev_info(dev, "dequeue_buffer, timeout=%ums\n", timeout_ms);
		if (dequeue_buffer(ut, timeout_ms)) {
			dev_info(dev, "dequeue_buffer failed\n");
			return -ENODATA;
		}

		return 0;
	}
	case ISP_UT_IOCTL_CREATE_SESSION: {
		struct cam_ioctl_create_session session;
		struct mtkcam_ipi_event event;
		struct mem_obj smem;
		struct mem_obj smem_ipi;
		void *mem_priv;
		void *mem_ipi_priv;
		struct mtk_ccd *ccd;
		int dmabuf_ipi_fd;
		struct mtkcam_ipi_buffer *cqbuf;
		struct mtkcam_ipi_buffer *msgbuf;

		LOG_CMD(ISP_UT_IOCTL_CREATE_SESSION);
		if (copy_from_user(&session, (void *)arg,
				   sizeof(session)) != 0) {
			dev_dbg(dev, "[CREATE_SESSION] Fail to get session\n");
			return -EFAULT;
		}

		ccd = (struct mtk_ccd *)ut->rproc_handle->priv;
		event.cmd_id = CAM_CMD_CREATE_SESSION;
		event.cookie = session.cookie;
		ut->mem->size = round_up(session.cq_buffer_size, PAGE_SIZE);
		smem.len = ut->mem->size;
		mem_priv = mtk_ccd_get_buffer(ccd, &smem);
		ut->mem->fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
		ut->mem->iova = smem.iova;
		ut->mem->va = smem.va;

		cqbuf = &event.session_data.workbuf;
		cqbuf->size = session.cq_buffer_size;
		cqbuf->ccd_fd = ut->mem->fd;
		cqbuf->iova = ut->mem->iova;

		/* config ipi msg */
		ut->msg_mem->size = round_up(IPI_FRAME_BUF_TOTAL_SIZE,
					 PAGE_SIZE);
		smem_ipi.len = ut->msg_mem->size;
		mem_ipi_priv = mtk_ccd_get_buffer(ccd, &smem_ipi);
		ut->msg_mem->fd = dmabuf_ipi_fd =
			mtk_ccd_get_buffer_fd(ccd, mem_ipi_priv);
		ut->msg_mem->iova = smem_ipi.iova;
		ut->msg_mem->va = smem_ipi.va;

		msgbuf = &event.session_data.msg_buf;
		msgbuf->size = IPI_FRAME_BUF_TOTAL_SIZE;
		msgbuf->ccd_fd = dmabuf_ipi_fd;
		msgbuf->iova = ut->msg_mem->iova;
		dev_info(dev, "[CREATE_SESSION] >msg_mem->va 0x%x size %d\n",
				ut->msg_mem->va, msgbuf->size);
		/* ipi msg end */

		ut->enque_list.cnt = 0;
		ut->deque_list.cnt = 0;
		ut->processing_list.cnt = 0;
		INIT_LIST_HEAD(&ut->enque_list.list);
		INIT_LIST_HEAD(&ut->deque_list.list);
		INIT_LIST_HEAD(&ut->processing_list.list);
		rpmsg_send(ut->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		ut->m2m_available = 1;

		return 0;
	}
	case ISP_UT_IOCTL_DESTROY_SESSION: {
		struct mtkcam_ipi_session_cookie cookie;
		struct mtkcam_ipi_event event;
		struct mtk_ccd *ccd = (struct mtk_ccd *)ut->rproc_handle->priv;
		struct mem_obj smem;
		int fd, i;

		LOG_CMD(ISP_UT_IOCTL_DESTROY_SESSION);

		if (copy_from_user(&cookie, (void *)arg,
				   sizeof(cookie)) != 0) {
			dev_dbg(dev, "[DESTROY_SESSION] Fail to get cookie\n");
			return -EFAULT;
		}

		event.cmd_id = CAM_CMD_DESTROY_SESSION;
		event.cookie = cookie;

		if (ut->with_testmdl) {
			for (i = 0; i < ut->is_dcif_camsv; i++)
				CALL_CAMSV_OPS(ut->camsv[i], s_stream, 0);

			if (ut->hardware_scenario != MTKCAM_IPI_HW_PATH_DC_STAGGER) {
				CALL_RAW_OPS(ut->raw[0], s_stream, 0);
				/* stream off rawb for bc case(or any case)
				 * w/o send hardware_scenario
				 */
				CALL_RAW_OPS(ut->raw[1], s_stream, 0);
			}

			CALL_SENINF_OPS(ut->seninf, reset);
		}

		smem.va = ut->mem->va;
		smem.iova = ut->mem->iova;
		smem.len = ut->mem->size;
		fd = ut->mem->fd;
		mtk_ccd_put_buffer(ccd, &smem);

		dev_dbg(dev,
				"%s:msg buffers release, mem(%p), sz(%d)\n",
				__func__, smem.va, smem.len);

		smem.va = ut->msg_mem->va;
		smem.iova = 0;
		smem.len = ut->msg_mem->size;
		fd = ut->msg_mem->fd;
		mtk_ccd_put_buffer(ccd, &smem);
		dev_dbg(dev,
				"%s:ipi msg buffers release, mem(%p), sz(%d)\n",
				__func__, smem.va, smem.len);

		rpmsg_send(ut->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		return 0;
	}
	case ISP_UT_IOCTL_ENQUE: {
		static struct cam_ioctl_enque enque; /* large struct */
		struct mtkcam_ipi_event event;
		struct mtk_cam_ut_buf_entry *buf_entry;
		unsigned long flags;
		struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
		struct mtkcam_ipi_frame_param *frame_data;
		int i = 0;

		struct cam_ioctl_enque *pEnque;

		LOG_CMD(ISP_UT_IOCTL_ENQUE);

		if (copy_from_user(&pEnque, (void *)arg, sizeof(pEnque)) != 0) {
			dev_dbg(dev, "[ENQUE] Fail to get enque\n");
			return -EFAULT;
		}

		if (copy_from_user(&enque, (void *)pEnque, sizeof(enque)) != 0) {
			dev_dbg(dev, "[ENQUE] Fail to get enque\n");
			return -EFAULT;
		}

		buf_entry = kzalloc(sizeof(*buf_entry), GFP_KERNEL);
		event.cmd_id = CAM_CMD_FRAME;
		event.cookie = enque.cookie;
		//event.frame_data = enque.frame_param;

		spin_lock_irqsave(&ut->enque_list.lock, flags);
		list_add_tail(&buf_entry->list_entry, &ut->enque_list.list);
		ut->enque_list.cnt++;

		if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER) {
			for (i = 0 ; i < CAM_MAX_IMAGE_INPUT; i++) {

				if (enque.frame_param.img_ins[i].uid.id == MTKCAM_IPI_RAW_RAWI_5) {
					ut->main_rawi = MTKCAM_IPI_RAW_RAWI_5;
					break;
				}
			}
		}
		/* ipi msg */
		dev_info(dev, "[ENQUE] msg_mem->va 0x%x size %d cur_msgbuf_offset 0x%x cur_workbuf_offset 0x%x frame_no %d\n",
				ut->msg_mem->va, ut->msg_mem->size,
				frame_info->cur_msgbuf_offset,
				enque.frame_param.cur_workbuf_offset,
				event.cookie.frame_no);

		/* Prepare rp message */
		BUILD_BUG_ON(sizeof(struct mtkcam_ipi_frame_param) > IPI_FRAME_BUF_SIZE);
		frame_info->cur_msgbuf_offset =
				(event.cookie.frame_no % IPI_FRAME_BUF_NUM) * IPI_FRAME_BUF_SIZE;
		frame_info->cur_msgbuf_size = IPI_FRAME_BUF_SIZE;
		buf_entry->msg_buffer.va = ut->msg_mem->va +
				frame_info->cur_msgbuf_offset;
		frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;


		memcpy(frame_data, &enque.frame_param,
				sizeof(enque.frame_param));
		/* ipi msg end */

		buf_entry->frame_seq_no = ut->enque_list.cnt;
		//buf_entry->cq_buf.iova = ut->mem->iova +
		//	event.frame_data.cur_workbuf_offset;
		buf_entry->cq_buf.iova = ut->mem->iova +
			enque.frame_param.cur_workbuf_offset;
		buf_entry->cq_buf.size = buf_entry->sub_cq_size = 0;

		spin_unlock_irqrestore(&ut->enque_list.lock, flags);
		rpmsg_send(ut->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		return 0;
	}
	case ISP_UT_IOCTL_CONFIG: {
		struct cam_ioctl_config config;
		struct mtkcam_ipi_event event;
		struct mtkcam_ipi_config_param *pParam;
		int i;

		LOG_CMD(ISP_UT_IOCTL_CONFIG);

		if (copy_from_user(&config, (void *)arg,
				   sizeof(config)) != 0) {
			dev_dbg(dev, "[CONFIG] Fail to get config\n");
			return -EFAULT;
		}
		dev_info(dev, "ut->is_dcif_camsv=%d\n", ut->is_dcif_camsv);
		if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_STAGGER) {
			pParam = &config.config_param;
			for (i = 0; i < ut->is_dcif_camsv; i++)
				CALL_CAMSV_OPS(ut->camsv[i], dev_config, &pParam->input);
		} else if (ut->hardware_scenario == MTKCAM_IPI_HW_PATH_DC_STAGGER) {
			pParam = &config.config_param;
			for (i = 0; i < ut->is_dcif_camsv; i++)
				CALL_CAMSV_OPS(ut->camsv[i], dev_config, &pParam->input);
		}

		config.config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
					/* |MTK_CAM_IPI_CONFIG_TYPE_SMVR_PREVIEW */

		ut->subsample = config.config_param.input.subsample;

		event.cmd_id = CAM_CMD_CONFIG;
		event.cookie = config.cookie;
		event.config_data = config.config_param;
		rpmsg_send(ut->rpmsg_dev->rpdev.ept, &event, sizeof(event));

		setup_hanlder(ut);

		return 0;
	}
	case ISP_UT_IOCTL_ALLOC_DMABUF: {
		struct cam_ioctl_dmabuf_param workbuf;
		struct mem_obj smem;
		void *mem_priv;
		struct mtk_ccd *ccd;

		LOG_CMD(ISP_UT_IOCTL_ALLOC_DMABUF);

		if (copy_from_user(&workbuf, (void *)arg,
				   sizeof(workbuf)) != 0) {
			dev_dbg(dev, "[ALLOC_DMABUF] Fail to get sw buffer\n");
			return -EFAULT;
		}

		ccd = (struct mtk_ccd *)ut->rproc_handle->priv;
		workbuf.size = round_up(workbuf.size, PAGE_SIZE);
		smem.len = workbuf.size;
		mem_priv = mtk_ccd_get_buffer(ccd, &smem);
		workbuf.ccd_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
		workbuf.iova = smem.iova;
		workbuf.kva = smem.va;

		if (copy_to_user((void *)arg, &workbuf,
				   sizeof(workbuf)) != 0) {
			dev_dbg(dev, "[ALLOC_DMABUF] Fail to put sw buffer\n");
			return -EFAULT;
		}

		dev_info(dev, "[ALLOC_DMABUF] ccd_fd=%d, kva=0x%x, iova=0x%x, size=%d\n",
				workbuf.ccd_fd, workbuf.kva, workbuf.iova, workbuf.size);
		return 0;
	}
	case ISP_UT_IOCTL_FREE_DMABUF: {
		struct cam_ioctl_dmabuf_param workbuf;
		struct mtk_ccd *ccd = (struct mtk_ccd *)ut->rproc_handle->priv;
		struct mem_obj smem;
		int fd;

		LOG_CMD(ISP_UT_IOCTL_FREE_DMABUF);

		if (copy_from_user(&workbuf, (void *)arg,
				   sizeof(workbuf)) != 0) {
			dev_dbg(dev, "[FREE_DMABUF] Fail to get sw buffer\n");
			return -EFAULT;
		}

		dev_info(dev, "[ALLOC_DMABUF] kva=0x%x, iova=0x%x, size=%d, ccd_fd=%d\n",
				workbuf.kva, workbuf.iova, workbuf.size, workbuf.ccd_fd);

		smem.va = workbuf.kva;
		smem.iova = workbuf.iova;
		smem.len = workbuf.size;
		fd = workbuf.ccd_fd;
		mtk_ccd_put_buffer(ccd, &smem);

		dev_dbg(dev,
				"%s:sw buffers release, mem(%p), sz(%d)\n",
				__func__, smem.iova, smem.len);

		return 0;
	}
	default:
		dev_info(dev, "DO NOT support this ioctl (%d)\n", cmd);
		return -ENOIOCTLCMD;
	}
}

static int cam_open(struct inode *inode, struct file *filp)
{
	struct mtk_cam_ut *ut =	container_of(inode->i_cdev,
					     struct mtk_cam_ut, cdev);
	int i;

	get_device(ut->dev);

	pm_runtime_get_sync(ut->dev);

	for (i = 0; i < ut->num_raw; i++) {
		pr_info("get_sync raw %d\n", i);
		pm_runtime_get_sync(ut->raw[i]);
	}

	for (i = 0; i < ut->num_camsv; i++) {
		pr_info("get_sync camsv %d\n", i);
		pm_runtime_get_sync(ut->camsv[i]);
	}

	/* Note: seninf's dts have no power-domains now, so do it after raw's */
	pm_runtime_get_sync(ut->seninf);

	filp->private_data = ut;

	cam_composer_init(ut);

	return 0;
}

static int cam_release(struct inode *inode, struct file *filp)
{
	struct mtk_cam_ut *ut = filp->private_data;
	int i;

	cam_composer_uninit(ut);

	pm_runtime_put(ut->seninf);

	for (i = 0; i < ut->num_camsv; i++)
		pm_runtime_put(ut->camsv[i]);

	for (i = 0; i < ut->num_raw; i++)
		pm_runtime_put(ut->raw[i]);

	pm_runtime_put(ut->dev);

	put_device(ut->dev);
	return 0;
}

static const struct file_operations cam_file_oper = {
	.owner = THIS_MODULE,
	.open = cam_open,
	.release = cam_release,
	.unlocked_ioctl = cam_ioctl,
};

static inline void cam_unreg_char_dev(struct mtk_cam_ut *ut)
{
	device_destroy(ut->class, ut->devno);
	class_destroy(ut->class);
	cdev_del(&ut->cdev);
	unregister_chrdev_region(ut->devno, 1);
}

static inline int cam_reg_char_dev(struct mtk_cam_ut *ut)
{
	struct device *dev;
	int ret;

	ret = alloc_chrdev_region(&ut->devno, 0, 1, CAM_DEV_NAME);
	if (ret < 0) {
		dev_dbg(ut->dev, "Fail to alloc chrdev region, %d\n", ret);
		return ret;
	}

	cdev_init(&ut->cdev, &cam_file_oper);
	//ut->cdev.owner = THIS_MODULE;
	ret = cdev_add(&ut->cdev, ut->devno, 1);
	if (ret < 0) {
		dev_dbg(ut->dev, "Attach file operation failed, %d\n", ret);
		goto EXIT;
	}

	ut->class = class_create(THIS_MODULE, CAM_DEV_NAME);
	if (IS_ERR(ut->class)) {
		ret = PTR_ERR(ut->class);
		dev_dbg(ut->dev, "Fail to create class, %d\n", ret);
		goto CLASS_CREATE_FAIL;
	}

	dev = device_create(ut->class, ut->dev, ut->devno, NULL, CAM_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		dev_dbg(ut->dev, "Fail to create /dev/%s, %d\n",
			CAM_DEV_NAME, ret);
		goto DEV_CREATE_FAIL;
	}
	return ret;

DEV_CREATE_FAIL:
	class_destroy(ut->class);
CLASS_CREATE_FAIL:
	cdev_del(&ut->cdev);
EXIT:
	cam_unreg_char_dev(ut);
	return ret;
}

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void mtk_cam_match_remove(struct device *dev)
{
	(void) dev;
}

static int add_match_by_driver(struct device *dev,
			       struct component_match **match,
			       struct platform_driver *drv)
{
	struct device *p = NULL, *d;
	int num = 0;

	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(dev, match, compare_dev, d);
		num++;
	} while (true);

	return num;
}

static struct component_match *mtk_cam_match_add(struct device *dev)
{
	struct mtk_cam_ut *ut = dev_get_drvdata(dev);
	struct component_match *match = NULL;
	int seninf_num;

	seninf_num = add_match_by_driver(dev, &match, &mtk_ut_seninf_driver);
	dev_info(dev, "# of seninf_num: %d\n", seninf_num);

	ut->num_raw = add_match_by_driver(dev, &match, &mtk_ut_raw_driver);
	dev_info(dev, "# of raw: %d\n", ut->num_raw);

	ut->num_yuv = add_match_by_driver(dev, &match, &mtk_ut_yuv_driver);
	dev_info(dev, "# of yuv: %d\n", ut->num_yuv);
#if WITH_CAMSV_DRIVER
	ut->num_camsv = add_match_by_driver(dev, &match, &mtk_ut_camsv_driver);
	dev_info(dev, "# of camsv: %d\n", ut->num_camsv);
#endif
	if (IS_ERR(match))
		mtk_cam_match_remove(dev);

	return match ? match : ERR_PTR(-ENODEV);
}

static int bind_cam_sub_pipes(struct mtk_cam_ut *ut)
{
	struct device *consumer, *supplier;
	struct device_link *link;
	struct mtk_ut_raw_device *raw;
	struct mtk_ut_yuv_device *yuv;
	int i;

	//dev_info(ut->dev, "%s\n", __func__);

	for (i = 0; i < ut->num_raw; i++) {
		consumer = ut->raw[i];
		supplier = ut->yuv[i];
		if (!consumer || !supplier) {
			dev_info(ut->dev,
				 "failed to get raw/yuv dev for id %d\n", i);
			continue;
		}

		raw = dev_get_drvdata(consumer);
		yuv = dev_get_drvdata(supplier);

		raw->yuv_base = yuv->base;

		link = device_link_add(consumer, supplier,
				       DL_FLAG_AUTOREMOVE_CONSUMER |
				       DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_info(ut->dev,
				 "Unable to create link between %s and %s\n",
				 dev_name(consumer), dev_name(supplier));
			return -ENODEV;
		}
	}
	return 0;
}

static int mtk_cam_ut_master_bind(struct device *dev)
{
	struct mtk_cam_ut *ut = dev_get_drvdata(dev);
	int ret;

	if (ut->num_raw) {
		ut->raw = devm_kcalloc(dev, ut->num_raw, sizeof(*ut->raw),
				       GFP_KERNEL);
		if (!ut->raw)
			return -ENOMEM;
	}

	if (ut->num_yuv) {
		ut->yuv = devm_kcalloc(dev, ut->num_yuv, sizeof(*ut->yuv),
				       GFP_KERNEL);
		if (!ut->yuv)
			return -ENOMEM;
	}

	if (ut->num_raw != ut->num_yuv) {
		dev_info(dev, "wrong num: raw %d, yuv %d\n",
			 ut->num_raw, ut->num_yuv);
		return -ENODEV;
	}
#if WITH_CAMSV_DRIVER
	if (ut->num_camsv) {
		ut->camsv = devm_kcalloc(dev, ut->num_camsv, sizeof(*ut->camsv),
				       GFP_KERNEL);
		if (!ut->camsv)
			return -ENOMEM;
	}
#endif
#if WITH_LARB_DRIVER
	if (ut->num_larb) {
		ut->larb = devm_kcalloc(dev, ut->num_larb, sizeof(*ut->larb),
					GFP_KERNEL);
		if (!ut->larb)
			return -ENOMEM;
	}
#endif

	dev_info(dev, "component_bind_all with data = 0x%llx\n", dev_get_drvdata(dev));
	ret = component_bind_all(dev, dev_get_drvdata(dev));
	if (ret) {
		dev_info(dev, "Failed to bind all component: %d\n", ret);
		return ret;
	}

	ret = bind_cam_sub_pipes(ut);
	if (ret) {
		dev_info(dev, "Failed to update_yuv_base: %d\n", ret);
		return ret;
	}

	dev_info(dev, "%s success\n", __func__);
	return 0;
}

static void mtk_cam_ut_master_unbind(struct device *dev)
{
	struct mtk_cam_ut *ut = dev_get_drvdata(dev);

	component_unbind_all(dev, ut);
}

static const struct component_master_ops mtk_cam_master_ops = {
	.bind = mtk_cam_ut_master_bind,
	.unbind = mtk_cam_ut_master_unbind,
};

static int register_sub_drivers(struct device *dev)
{
	struct component_match *match = NULL;
	int ret;

#if WITH_LARB_DRIVER
	ret = platform_driver_register(&mtk_ut_larb_driver);
	if (ret) {
		dev_info(dev, "%s register larb driver fail\n", __func__);
		goto REGISTER_LARB_FAIL;
	}
#endif

	ret = platform_driver_register(&mtk_ut_raw_driver);
	if (ret) {
		dev_info(dev, "%s register raw driver fail\n", __func__);
		goto REGISTER_RAW_FAIL;
	}

	ret = platform_driver_register(&mtk_ut_yuv_driver);
	if (ret) {
		dev_info(dev, "%s register yuv driver fail\n", __func__);
		goto REGISTER_YUV_FAIL;
	}
#if WITH_CAMSV_DRIVER
	ret = platform_driver_register(&mtk_ut_camsv_driver);
	if (ret) {
		dev_info(dev, "%s register camsv driver fail\n", __func__);
		goto REGISTER_CAMSV_FAIL;
	}
#endif
	ret = platform_driver_register(&mtk_ut_seninf_driver);
	if (ret) {
		dev_info(dev, "%s register seninf driver fail\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	match = mtk_cam_match_add(dev);
	if (IS_ERR(match)) {
		dev_info(dev, "%s mtk_cam_match_add failed\n", __func__);
		ret = PTR_ERR(match);
		goto ADD_MATCH_FAIL;
	}

	ret = component_master_add_with_match(dev, &mtk_cam_master_ops, match);
	if (ret < 0)
		goto MASTER_ADD_MATCH_FAIL;

	return 0;

MASTER_ADD_MATCH_FAIL:
	mtk_cam_match_remove(dev);

ADD_MATCH_FAIL:
	platform_driver_unregister(&mtk_ut_seninf_driver);

REGISTER_SENINF_FAIL:
	platform_driver_unregister(&mtk_ut_camsv_driver);
#if WITH_CAMSV_DRIVER
REGISTER_CAMSV_FAIL:
	platform_driver_unregister(&mtk_ut_yuv_driver);
#endif
REGISTER_YUV_FAIL:
	platform_driver_unregister(&mtk_ut_raw_driver);

REGISTER_RAW_FAIL:
#if WITH_LARB_DRIVER
	platform_driver_unregister(&mtk_ut_larb_driver);

REGISTER_LARB_FAIL:
#endif
	return ret;
}

static int mtk_cam_ut_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_ut *ut;
	int ret;

	ut = devm_kzalloc(dev, sizeof(*ut), GFP_KERNEL);
	if (!ut)
		return -ENOMEM;
	ut->dev = dev;
	platform_set_drvdata(pdev, ut);

#ifdef CONFIG_MTK_IOMMU_PGTABLE_EXT
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);
#endif
#endif

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}


	ret = cam_of_rproc(ut);
	if (ret)
		return ret;

	ret = cam_reg_char_dev(ut);
	if (ret) {
		dev_info(dev, "fail to register char dev\n");
		return ret;
	}

	ut->mem = devm_kzalloc(dev, sizeof(*ut->mem), GFP_KERNEL);
	ut->msg_mem = devm_kzalloc(dev, sizeof(*ut->msg_mem), GFP_KERNEL);
	spin_lock_init(&ut->spinlock_irq);
	spin_lock_init(&ut->enque_list.lock);
	spin_lock_init(&ut->deque_list.lock);
	spin_lock_init(&ut->processing_list.lock);
	init_waitqueue_head(&ut->done_wq);

	ut->listener.on_notify = ut_event_on_notify;

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_info(dev, "fail to register_sub_drivers\n");
		return ret;
	}

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static int mtk_cam_ut_remove(struct platform_device *pdev)
{
	struct mtk_cam_ut *ut =
		(struct mtk_cam_ut *)platform_get_drvdata(pdev);

	pm_runtime_disable(ut->dev);

	cam_unreg_char_dev(ut);

	return 0;
}

static int mtk_cam_ut_pm_suspend(struct device *dev)
{
	int ret = 0;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		dev_dbg(dev, "failed to force suspend:%d\n", ret);

	return ret;
}

static int mtk_cam_ut_pm_resume(struct device *dev)
{
	int ret = 0;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_resume(dev);
	return ret;
}

static int mtk_cam_ut_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_cam_ut_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_cam_ut_pm_suspend, mtk_cam_ut_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_cam_ut_runtime_suspend,
			   mtk_cam_ut_runtime_resume,
			   NULL)
};

static const struct of_device_id cam_ut_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6985-camisp", },
	{ .compatible = "mediatek,mt6886-camisp", },
	{}
};
MODULE_DEVICE_TABLE(of, cam_ut_driver_dt_match);

static struct platform_driver mtk_cam_ut_driver = {
	.probe		= mtk_cam_ut_probe,
	.remove		= mtk_cam_ut_remove,
	.driver		= {
		.name	= "mtk-cam ut",
		.of_match_table = of_match_ptr(cam_ut_driver_dt_match),
	}
};

static int __init mtk_cam_ut_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_cam_ut_driver);
	return ret;
}

static void __exit mtk_cam_ut_exit(void)
{
	platform_driver_unregister(&mtk_cam_ut_driver);
}

module_init(mtk_cam_ut_init);
module_exit(mtk_cam_ut_exit);

MODULE_DESCRIPTION("Mediatek ISP unit test driver");
MODULE_LICENSE("GPL v2");
