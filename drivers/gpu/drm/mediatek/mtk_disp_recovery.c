/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <drm/drmP.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_fbdev.h"

#include <../../../kernel/irq/internals.h>

#define ESD_TRY_CNT 5
#define ESD_CHECK_PERIOD 10 /* ms */
#define MTK_DRM_ESD_CHECK 0

/* pinctrl implementation */
long _set_state(struct drm_crtc *crtc, const char *name)
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

long disp_dts_gpio_init(struct device *dev, struct mtk_drm_private *private)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long ret = 0;
	struct pinctrl *pctrl;

	/* retrieve */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		DDPPR_ERR("Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	private->pctrl = pctrl;

exit:
	return ret;
#else
	return 0;
#endif
}

static inline int _can_switch_check_mode(struct drm_crtc *crtc,
					 struct mtk_panel_ext *panel_ext)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int ret = 0;

	if (panel_ext->params->cust_esd_check == 0 &&
	    panel_ext->params->lcm_esd_check_table[0].cmd != 0 &&
	    mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_ESD_CHECK_SWITCH))
		ret = 1;

	return ret;
}

static inline int _lcm_need_esd_check(struct mtk_panel_ext *panel_ext)
{
	int ret = 0;

	if (panel_ext->params->esd_check_enable == 1 &&
		mtk_drm_lcm_is_connect()) {
		ret = 1;
	}

	return ret;
}

static inline int need_wait_esd_eof(struct drm_crtc *crtc,
				    struct mtk_panel_ext *panel_ext)
{
	int ret = 1;

	/*
	 * 1.vdo mode
	 * 2.cmd mode te
	 */
	if (!mtk_crtc_is_frame_trigger_mode(crtc))
		ret = 0;

	if (panel_ext->params->cust_esd_check == 0)
		ret = 0;

	return ret;
}

static void esd_cmdq_timeout_cb(struct cmdq_cb_data data)
{
	struct drm_crtc *crtc = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;

	if (!crtc) {
		DDPMSG("%s find crtc fail\n", __func__);
		return;
	}

	DDPMSG("read flush fail\n");
	esd_ctx->chk_sta = 0xff;
	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);
}


int _mtk_esd_check_read(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct mtk_panel_ext *panel_ext;
	struct cmdq_pkt *cmdq_handle, *cmdq_handle2;
	struct mtk_drm_esd_ctx *esd_ctx;
	int ret = 0;

	DDPINFO("[ESD]ESD read panel\n");


	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EINVAL;
	}

	if (mtk_drm_is_idle(crtc) && mtk_dsi_is_cmd_mode(output_comp))
		return 0;

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_PANEL_EXT, &panel_ext);
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("%s:can't find panel_ext handle\n", __func__);
		return -EINVAL;
	}

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
	cmdq_handle->err_cb.cb = esd_cmdq_timeout_cb;
	cmdq_handle->err_cb.data = crtc;

	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 2, 1);

	if (mtk_dsi_is_cmd_mode(output_comp)) {
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_FIRST_PATH, 0);

		cmdq_pkt_clear_event(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, ESD_CHECK_READ,
				    (void *)mtk_crtc->gce_obj.buf.pa_base +
					    DISP_SLOT_ESD_READ_BASE);

		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
	} else { /* VDO mode */
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_SECOND_PATH, 1);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_FIRST_PATH, 1);

		CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 2, 2);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_STOP_VDO_MODE,
				    NULL);

		CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 2, 3);


		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, ESD_CHECK_READ,
				    (void *)mtk_crtc->gce_obj.buf.pa_base +
					    DISP_SLOT_ESD_READ_BASE);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle,
				    DSI_START_VDO_MODE, NULL);

		mtk_disp_mutex_trigger(mtk_crtc->mutex[0], cmdq_handle);
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, COMP_REG_START,
				    NULL);
	}
	esd_ctx = mtk_crtc->esd_ctx;
	esd_ctx->chk_sta = 0;

	cmdq_pkt_flush(cmdq_handle);

	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 2, 4);


	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_READ_EPILOG,
				    NULL);
	if (esd_ctx->chk_sta == 0xff) {
		ret = -1;
		if (need_wait_esd_eof(crtc, panel_ext)) {
			/* TODO: set ESD_EOF event through CPU is better */
			mtk_crtc_pkt_create(&cmdq_handle2, crtc,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);

			cmdq_pkt_set_event(
				cmdq_handle2,
				mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
			cmdq_pkt_flush(cmdq_handle2);
			cmdq_pkt_destroy(cmdq_handle2);
		}
		goto done;
	}

	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_CHECK_CMP,
				  (void *)mtk_crtc->gce_obj.buf.va_base +
					  DISP_SLOT_ESD_READ_BASE);
done:
	cmdq_pkt_destroy(cmdq_handle);
	return ret;
}

static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	struct mtk_drm_esd_ctx *esd_ctx = (struct mtk_drm_esd_ctx *)data;
	if (esd_ctx->panel_init) {
		atomic_set(&esd_ctx->ext_te_event, 1);
		wake_up_interruptible(&esd_ctx->ext_te_wq);
		DDPDSIINFO("[ESD]_esd_check_ext_te_irq_handler is comming\n");
	}
	else {
		DDPDSIINFO("[ESD]_esd_check_ext_te_irq_handler is comming, but ignore\n");
	}
	return IRQ_HANDLED;
}

void mkt_disp_esd_irq_ctrl(struct mtk_drm_esd_ctx *esd_ctx,
                               bool enable)
{
	struct irq_desc *desc;

	DDPDSIINFO("[ESD]_esd_irq_ctrl, enable = %d\n",enable);
	if (esd_ctx->eint_irq) {
		if (enable) {
			desc = irq_to_desc(esd_ctx->eint_irq);
			DDPDSIINFO("[ESD]_esd_irq_ctrl, esd_ctx->eint_irq = %d, level = %d\n",esd_ctx->eint_irq, irq_settings_is_level(desc));
			if (!irq_settings_is_level(desc))
				desc->istate &= ~IRQS_PENDING;
			enable_irq(esd_ctx->eint_irq);
		} else {
			disable_irq_nosync(esd_ctx->eint_irq);
		}
	}
}

static int _mtk_esd_check_eint(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 1;

	DDPDSIINFO("[ESD]ESD check eint\n");

	if (unlikely(!esd_ctx)) {
		DDPDSIINFO("%s:invalid ESD context\n", __func__);
		return -EINVAL;
	}

	enable_irq(esd_ctx->eint_irq);

	/* check if there is TE in the last 2s, if so ESD check is pass */
	if (wait_event_interruptible_timeout(
		    esd_ctx->ext_te_wq,
		    atomic_read(&esd_ctx->ext_te_event),
		    HZ / 2) > 0)
		ret = 0;

	disable_irq(esd_ctx->eint_irq);
	atomic_set(&esd_ctx->ext_te_event, 0);

	return ret;
}

static int mtk_drm_request_eint(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	struct mtk_ddp_comp *output_comp;
	struct device_node *node;
	u32 ints[2] = {0, 0};
	char *compat_str;
	int ret = 0;

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return -EINVAL;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EINVAL;
	}

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_ESD_EINT_COMPAT,
			    &compat_str);
	if (unlikely(!compat_str)) {
		DDPPR_ERR("%s: invalid compat string\n", __func__);
		return -EINVAL;
	}
	node = of_find_compatible_node(NULL, NULL, compat_str);
	if (unlikely(!node)) {
		DDPPR_ERR("can't find ESD TE eint compatible node\n");
		return -EINVAL;
	}

	of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
	esd_ctx->eint_irq = irq_of_parse_and_map(node, 0);

	ret = request_irq(esd_ctx->eint_irq, _esd_check_ext_te_irq_handler,
			  IRQF_TRIGGER_FALLING, "ESD_TE-eint", esd_ctx);
	if (ret) {
		DDPPR_ERR("eint irq line not available!\n");
		return ret;
	}

	//disable_irq(esd_ctx->eint_irq);

	_set_state(crtc, "mode_te_te");

	return ret;
}

static int mtk_drm_esd_check(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext;
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 0;

	CRTC_MMP_EVENT_START(drm_crtc_index(crtc), esd_check, 0, 0);

	if (mtk_crtc->enabled == 0) {
		CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 0, 99);
		DDPINFO("[ESD] CRTC %d disable. skip esd check\n",
			drm_crtc_index(crtc));
		goto done;
	}

	panel_ext = mtk_crtc->panel_ext;
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("can't find panel_ext handle\n");
		ret = -EINVAL;
		goto done;
	}

	/* Check panel EINT */
	if (panel_ext->params->cust_esd_check == 0 &&
	    esd_ctx->chk_mode == READ_EINT) {
		CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 1, 0);
		ret = _mtk_esd_check_eint(crtc);
	} else { /* READ LCM CMD  */
		CRTC_MMP_MARK(drm_crtc_index(crtc), esd_check, 2, 0);
		ret = _mtk_esd_check_read(crtc);
	}

	/* switch ESD check mode */
	if (_can_switch_check_mode(crtc, panel_ext) &&
	    !mtk_crtc_is_frame_trigger_mode(crtc))
		esd_ctx->chk_mode =
			(esd_ctx->chk_mode == READ_EINT) ? READ_LCM : READ_EINT;

done:
	CRTC_MMP_EVENT_END(drm_crtc_index(crtc), esd_check, 0, ret);
	return ret;
}

static int mtk_drm_esd_recover(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc_state);

	int ret = 0;
	DDPDSIINFO("%s: esd recovery is comming\n", __func__);
	CRTC_MMP_EVENT_START(drm_crtc_index(crtc), esd_recovery, 0, 0);
	if (crtc->state && !crtc->state->active) {
		DDPDSIINFO("%s: crtc is inactive\n", __func__);
		return 0;
	}
	if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] == 1) {
		DDPDSIINFO("%s: crtc is in doze active, skip\n", __func__);
		return 0;
	}
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 1);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (unlikely(!output_comp)) {
		DDPDSIINFO("%s: invalid output comp\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_PANEL_DISABLE, NULL);

	mtk_drm_crtc_disable(crtc, true);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 2);

	mtk_drm_crtc_enable(crtc);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 3);

	mtk_dsi_esd_recovery_flag(output_comp, crtc);
	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_PANEL_ENABLE, NULL);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 4);

	mtk_ddp_comp_io_cmd_dispparam(output_comp, NULL, DISPPARAM_RESTORE_BACKLIGHT, NULL);
	CRTC_MMP_MARK(drm_crtc_index(crtc), esd_recovery, 0, 5);

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
	DDPDSIINFO("%s: esd recovery is successfully\n", __func__);
done:

	DDPDSIINFO("%s: esd recovery is over\n", __func__);
	CRTC_MMP_EVENT_END(drm_crtc_index(crtc), esd_recovery, 0, ret);

	return 0;
}

static int mtk_drm_esd_check_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 0;
	int i = 0;
	int recovery_flg = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	if (!crtc) {
		DDPPR_ERR("%s invalid CRTC context, stop thread\n", __func__);

		return -EINVAL;
	}

	while (1) {
		msleep(ESD_CHECK_PERIOD);
		ret = wait_event_interruptible(
			esd_ctx->check_task_wq,
			atomic_read(&esd_ctx->check_wakeup));
		if (ret < 0) {
			DDPDSIINFO("[ESD]check thread waked up accidently\n");
			continue;
		}

		mutex_lock(&private->commit.lock);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		/* 1. esd check & recovery */
		if (!esd_ctx->chk_active) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mutex_unlock(&private->commit.lock);
			continue;
		}

		i = 0; /* repeat */
		do {
			ret = mtk_drm_esd_check(crtc);

			if (!ret) /* success */
				break;

			DDPDSIINFO(
				"[ESD]esd check fail, will do esd recovery. try=%d\n",
				i);
			mtk_drm_esd_recover(crtc);
			recovery_flg = 1;
		} while (++i < ESD_TRY_CNT);

		if (ret != 0) {
			DDPDSIINFO(
				"[ESD]after esd recovery %d times, still fail, disable esd check\n",
				ESD_TRY_CNT);
			mtk_disp_esd_check_switch(crtc, false);
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mutex_unlock(&private->commit.lock);
			break;
		} else if (recovery_flg) {
			DDPDSIINFO("[ESD] esd recovery success\n");
			recovery_flg = 0;
		}

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);

		/* 2. other check & recovery */
		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mtk_drm_esd_recovery_check_worker_kthread(void *data)
{
	struct sched_param param = { .sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		msleep(ESD_CHECK_PERIOD); /* 10ms */
		ret = wait_event_interruptible(esd_ctx->ext_te_wq,
					atomic_read(&esd_ctx->ext_te_event));
		if (ret < 0) {
			DDPDSIINFO("[ESD]check thread waked up accidently\n");
			continue;
		}

		DDPDSIINFO("[ESD]check thread waked up successfully\n");
		mutex_lock(&private->commit.lock);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		/* 1.esd recovery */
		mtk_drm_esd_recover(crtc);

		/* 2.clear atomic  ext_te_event */
		atomic_set(&esd_ctx->ext_te_event, 0);

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		DDPDSIINFO("[ESD]check thread is over\n");

		/* 3.other check & recovery */
		if (kthread_should_stop())
			break;
	}
	return 0;
}
void mtk_disp_esd_check_switch(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_ESD_CHECK_RECOVERY))
		return;

	if (unlikely(!esd_ctx)) {
		DDPINFO("%s:invalid ESD context, crtc id:%d\n",
			__func__, drm_crtc_index(crtc));
		return;
	}
	esd_ctx->chk_active = enable;
	atomic_set(&esd_ctx->check_wakeup, enable);
	if (enable)
		wake_up_interruptible(&esd_ctx->check_task_wq);
}

static void mtk_disp_esd_chk_deinit(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return;
	}

	/* Stop ESD task */
	mtk_disp_esd_check_switch(crtc, false);

	/* Stop ESD kthread */
	kthread_stop(esd_ctx->disp_esd_chk_task);

	kfree(esd_ctx);
}

static void mtk_disp_esd_chk_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext;
	struct mtk_drm_esd_ctx *esd_ctx;

	panel_ext = mtk_crtc->panel_ext;
	if (!(panel_ext && panel_ext->params)) {
		DDPMSG("can't find panel_ext handle\n");
		return;
	}

	if (_lcm_need_esd_check(panel_ext) == 0)
		return;

	DDPDSIINFO("create ESD thread\n");
	/* primary display check thread init */
	esd_ctx = kzalloc(sizeof(*esd_ctx), GFP_KERNEL);
	if (!esd_ctx) {
		DDPPR_ERR("allocate ESD context failed!\n");
		return;
	}
	mtk_crtc->esd_ctx = esd_ctx;

	if (MTK_DRM_ESD_CHECK) {
		esd_ctx->disp_esd_chk_task = kthread_create(
			mtk_drm_esd_check_worker_kthread, crtc, "disp_echk");
	}
	else {
		esd_ctx->disp_esd_chk_task = kthread_create(
			mtk_drm_esd_recovery_check_worker_kthread, crtc, "disp_esd_check");
	}

	init_waitqueue_head(&esd_ctx->check_task_wq);
	init_waitqueue_head(&esd_ctx->ext_te_wq);
	atomic_set(&esd_ctx->check_wakeup, 0);
	atomic_set(&esd_ctx->ext_te_event, 0);
	esd_ctx->chk_mode = READ_EINT;
	mtk_drm_request_eint(crtc);

	wake_up_process(esd_ctx->disp_esd_chk_task);
}

void mtk_disp_chk_recover_deinit(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	/* TODO : check function work in other CRTC & other connector */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_ESD_CHECK_RECOVERY) &&
	    drm_crtc_index(&mtk_crtc->base) == 0)
		mtk_disp_esd_chk_deinit(crtc);
}

void mtk_disp_chk_recover_init(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	/* TODO : check function work in other CRTC & other connector */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_ESD_CHECK_RECOVERY) &&
	    drm_crtc_index(&mtk_crtc->base) == 0)
		mtk_disp_esd_chk_init(crtc);
}
