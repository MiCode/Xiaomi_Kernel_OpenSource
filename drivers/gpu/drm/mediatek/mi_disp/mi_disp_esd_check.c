// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
//#include <drm/drmP.h>
//#include <linux/soc/mediatek/mtk-cmdq.h>

#include "../../../../kernel/irq/internals.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_mmp.h"
//#include "mtk_drm_fbdev.h"
#include "mtk_drm_trace.h"
#include "mi_disp_feature.h"
#include "mtk_disp_recovery.h"
#include "linux/delay.h"
#include "mi_disp_esd_check.h"

#define ESD_CHECK_IRQ_PERIOD 10 /* ms */

/* pinctrl implementation */
static long set_state(struct drm_crtc *crtc, const char *name)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct pinctrl_state *pState = 0;
	long ret = 0;

	/* TODO: race condition issue for pctrl handle */
	/* SO Far _set_state() only process once */
	if (!priv->pctrl) {
		DDPPR_ERR("this pctrl is null\n");
		return -1;
	}

	pState = pinctrl_lookup_state(priv->pctrl, name);
	if (IS_ERR(pState)) {
		DDPPR_ERR("lookup state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(priv->pctrl, pState);

exit:
	return ret; /* Good! */
#else
	return 0; /* Good! */
#endif
}

extern void set_panel_dead_flag(int value);

static int mtk_drm_esd_recover(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	int ret = 0;
	unsigned int last_hrt_req= 0;

	set_panel_dead_flag(1);

	CRTC_MMP_EVENT_START(drm_crtc_index(crtc), esd_recovery, 0, 0);
	if (crtc->state && !crtc->state->active) {
		DDPMSG("%s: crtc is inactive\n", __func__);
		return 0;
	}
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 1);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s: invalid output comp\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_PANEL_DISABLE, NULL);

	mtk_drm_crtc_disable(crtc, true);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 2);

#ifdef MTK_FB_MMDVFS_SUPPORT
	if (drm_crtc_index(crtc) == 0)
		mtk_disp_set_hrt_bw(mtk_crtc,
			mtk_crtc->qos_ctx->last_hrt_req);
	last_hrt_req = mtk_crtc->qos_ctx->last_hrt_req;
#endif

	mdelay(150);

	mtk_drm_crtc_enable(crtc);
	mtk_crtc->qos_ctx->last_hrt_req = last_hrt_req;

	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 3);

	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_PANEL_ENABLE, NULL);

	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 4);

	mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_RESTORE_BACKLIGHT, NULL);

	mtk_crtc_hw_block_ready(crtc);
	if (mtk_crtc_is_frame_trigger_mode(crtc)) {
		struct cmdq_pkt *cmdq_handle;

		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);

		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 5);

done:
	set_panel_dead_flag(0);
	CRTC_MMP_EVENT_END(drm_crtc_index(crtc), esd_recovery, 0, ret);

	return 0;
}

static irqreturn_t _mi_esd_check_err_flag_irq_handler(int irq, void *data)
{
	struct mi_esd_ctx *mi_esd_ctx = (struct mi_esd_ctx *)data;

	if (((mi_esd_ctx->err_flag_irq_flags & IRQF_TRIGGER_FALLING
		|| mi_esd_ctx->err_flag_irq_flags & IRQF_TRIGGER_LOW)
		&& gpio_get_value(mi_esd_ctx->err_flag_irq_gpio))
		|| ((mi_esd_ctx->err_flag_irq_flags & IRQF_TRIGGER_RISING
		|| mi_esd_ctx->err_flag_irq_flags & IRQF_TRIGGER_HIGH)
		&& !gpio_get_value(mi_esd_ctx->err_flag_irq_gpio))) {
		pr_info("[ESD] err flag pin level is normal\n");
		return IRQ_HANDLED;
	}

	if (mi_esd_ctx->panel_init) {
		atomic_set(&mi_esd_ctx->err_flag_event, 1);
		wake_up_interruptible(&mi_esd_ctx->err_flag_wq);
		pr_info("[ESD]_esd_check_err_flag_irq_handler is comming\n");
	}
	else {
		pr_info("[ESD]_esd_check_err_flag_irq_handler is comming, but ignore\n");
	}
	return IRQ_HANDLED;
}

static int mi_disp_request_err_flag(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext;
	struct mi_esd_ctx *mi_esd_ctx = mtk_crtc->mi_esd_ctx;

	int ret = 0;
	if (unlikely(!mi_esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return -EINVAL;
	}

	panel_ext = mtk_crtc->panel_ext;

	mi_esd_ctx->err_flag_irq_gpio = panel_ext->params->err_flag_irq_gpio;
	mi_esd_ctx->err_flag_irq_flags = panel_ext->params->err_flag_irq_flags;

	if (!mi_esd_ctx->err_flag_irq_gpio) {
		DDPPR_ERR("%s err_flag_irq_gpio is not defined\n", __func__);
		return -EINVAL;
	}

	if (gpio_is_valid(mi_esd_ctx->err_flag_irq_gpio)) {
		mi_esd_ctx->err_flag_irq = gpio_to_irq(mi_esd_ctx->err_flag_irq_gpio);
		ret = gpio_request(mi_esd_ctx->err_flag_irq_gpio, "esd_err_irq_gpio");
		if (ret) {
			DDPPR_ERR("Failed to request esd irq gpio %d, ret=%d\n",
				mi_esd_ctx->err_flag_irq_gpio, ret);
			return ret;
		} else
			gpio_direction_input(mi_esd_ctx->err_flag_irq_gpio);
	} else {
		DDPPR_ERR("err_flag_irq_gpio is invalid\n");
		ret = -EINVAL;
		return ret;
	}

	if (mi_esd_ctx->err_flag_irq_gpio > 0) {
		ret = request_threaded_irq(mi_esd_ctx->err_flag_irq,
			NULL, _mi_esd_check_err_flag_irq_handler,
			mi_esd_ctx->err_flag_irq_flags,
			"esd_err_irq", mi_esd_ctx);
		if (ret) {
			DDPPR_ERR("display register esd irq failed\n");
			return ret;
		} else {
			DDPPR_ERR("display register esd irq success\n");
			disable_irq(mi_esd_ctx->err_flag_irq);
		}
	}
	set_state(crtc, "err_flag_init");
	return ret;
}

static int mi_esd_err_flag_irq_check_worker_kthread(void *data)
{
    struct sched_param param = { .sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mi_esd_ctx *mi_esd_ctx = mtk_crtc->mi_esd_ctx;
	int ret = 0;
	u8 panel_dead = 0;
	struct disp_event event;

	sched_setscheduler(current, SCHED_RR, &param);

	event.disp_id = MI_DISP_PRIMARY;
	event.type = MI_DISP_EVENT_PANEL_DEAD;
	event.length = sizeof(panel_dead);

	/* wait 110ms util crtc init finished then start esd irq check */
	msleep(110);
	mi_disp_err_flag_esd_check_switch(crtc, true);

	pr_info("start ESD thread\n");
	while (1) {
		msleep(ESD_CHECK_IRQ_PERIOD); /* 10ms */
		ret = wait_event_interruptible(mi_esd_ctx->err_flag_wq,
		atomic_read(&mi_esd_ctx->err_flag_event));
		if (ret < 0) {
			DDPINFO("[ESD]check thread waked up accidently\n");
			continue;
		}
		pr_info("ESD waked up\n");
		DDPINFO("[ESD]check thread waked up successfully\n");
		mutex_lock(&private->commit.lock);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		/* 1.esd recovery */
		panel_dead = 1;
		mi_disp_feature_event_notify(&event, &panel_dead);

		mtk_drm_esd_recover(crtc);

		panel_dead = 0;
		mi_disp_feature_event_notify(&event, &panel_dead);
		/* 2.clear atomic  ext_te_event */
		atomic_set(&mi_esd_ctx->err_flag_event, 0);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		pr_info("[ESD]check thread is over\n");
		/* 3.other check & recovery */
		if (kthread_should_stop())
			break;
	}
	return 0;
}

static void mi_disp_esd_err_flag_irq_init(struct mi_esd_ctx *mi_esd_ctx, struct drm_crtc *crtc)
{
	mi_esd_ctx->disp_esd_irq_chk_task = kthread_create(
		mi_esd_err_flag_irq_check_worker_kthread, crtc, "err_flag_chk");
	init_waitqueue_head(&mi_esd_ctx->err_flag_wq);
	atomic_set(&mi_esd_ctx->err_flag_event, 0);
	wake_up_process(mi_esd_ctx->disp_esd_irq_chk_task);
}

int mi_disp_esd_irq_ctrl(struct mi_esd_ctx *mi_esd_ctx,
		bool enable)
{
	struct irq_desc *desc;
	if (gpio_is_valid(mi_esd_ctx->err_flag_irq_gpio)) {
		if (mi_esd_ctx->err_flag_irq) {
			if (enable) {
				if (!mi_esd_ctx->err_flag_enabled) {
					desc = irq_to_desc(mi_esd_ctx->err_flag_irq);
					if (!irq_settings_is_level(desc)) {
						pr_info("clear pending esd irq\n");
						desc->istate &= ~IRQS_PENDING;
					}
					enable_irq_wake(mi_esd_ctx->err_flag_irq);
					enable_irq(mi_esd_ctx->err_flag_irq);
					mi_esd_ctx->err_flag_enabled = true;
					pr_info("panel esd irq is enable\n");
				}
			} else {
				if (mi_esd_ctx->err_flag_enabled) {
					disable_irq_wake(mi_esd_ctx->err_flag_irq);
					disable_irq_nosync(mi_esd_ctx->err_flag_irq);
					mi_esd_ctx->err_flag_enabled = false;
					pr_info("panel esd irq is disable\n");
				}
			}
		}
	} else {
		pr_info("panel esd irq gpio invalid\n");
	}
	return 0;
}

void mi_disp_err_flag_esd_check_switch(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mi_esd_ctx *mi_esd_ctx = mtk_crtc->mi_esd_ctx;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_ESD_CHECK_RECOVERY))
		return;

	if (unlikely(!mi_esd_ctx)) {
		DDPINFO("%s:invalid ESD context, crtc id:%d\n",
			__func__, drm_crtc_index(crtc));
		return;
	}

	if (enable) {
		mi_disp_esd_irq_ctrl(mi_esd_ctx, true);
	} else {
		mi_disp_esd_irq_ctrl(mi_esd_ctx, false);
	}
}

void mi_disp_esd_chk_deinit(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mi_esd_ctx *mi_esd_ctx = mtk_crtc->mi_esd_ctx;

	if (unlikely(!mi_esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return;
	}

	/* Stop MI ESD kthread */
	kthread_stop(mi_esd_ctx->disp_esd_irq_chk_task);

	kfree(mi_esd_ctx);

	mtk_crtc->mi_esd_ctx = NULL;
}

void mi_disp_esd_chk_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mi_esd_ctx *mi_esd_ctx;

	if (drm_crtc_index(&mtk_crtc->base) != 0)
		return;

	mi_esd_ctx = kzalloc(sizeof(*mi_esd_ctx), GFP_KERNEL);
	if (!mi_esd_ctx) {
		DDPPR_ERR("allocate MI ESD context failed!\n");
		return;
	}
	mtk_crtc->mi_esd_ctx = mi_esd_ctx;

	if (!mi_disp_request_err_flag(crtc)) {
		mi_disp_esd_err_flag_irq_init(mi_esd_ctx, crtc);
		mi_esd_ctx->panel_init = true;
	} else {
		DDPPR_ERR("mi esd err flag gpio request failed\n");
		kfree(mtk_crtc->mi_esd_ctx);
		mtk_crtc->mi_esd_ctx = NULL;
	}

	return;
}
