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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <drm/drmP.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "../../../../kernel/irq/internals.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_fbdev.h"
#include "mtk_drm_trace.h"
#include "mi_disp/mi_disp_feature.h"

#define ESD_TRY_CNT 5
#define ESD_CHECK_PERIOD 2000 /* ms */

#ifdef CONFIG_MI_ESD_CHECK
#define ESD_CHECK_IRQ_PERIOD 10 /* ms */
#ifndef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
static irqreturn_t _mi_esd_check_ext_te_irq_handler(int irq, void *data);
#else
static irqreturn_t _mi_esd_check_err_flag_irq_handler(int irq, void *data);
#endif
#endif

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

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_PANEL_EXT, &panel_ext);
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("%s:can't find panel_ext handle\n", __func__);
		return -EINVAL;
	}

	if (mtk_drm_is_idle(crtc) && mtk_dsi_is_cmd_mode(output_comp)) {
		if (panel_ext->params->cust_esd_check)
			mtk_drm_idlemgr_kick(__func__, crtc, 0);
		else
			return 0;
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

#ifdef CONFIG_MI_ESD_CHECK
#ifdef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, MI_ESD_CHECK_READ, NULL);
#endif
#else
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, ESD_CHECK_READ,
				    (void *)mtk_crtc->gce_obj.buf.pa_base +
					    DISP_SLOT_ESD_READ_BASE);
#endif
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

#ifdef CONFIG_MI_ESD_CHECK
	//cmdq_pkt_flush(cmdq_handle);
#else
	cmdq_pkt_flush(cmdq_handle);
#endif

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

#ifdef CONFIG_MI_ESD_CHECK
#ifdef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, MI_ESD_CHECK_CMP, NULL);
#endif
#else
	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_CHECK_CMP,
				  (void *)mtk_crtc->gce_obj.buf.va_base +
					  DISP_SLOT_ESD_READ_BASE);
#endif

done:
	cmdq_pkt_destroy(cmdq_handle);
	return ret;
}

#ifndef CONFIG_MI_ESD_CHECK
static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	struct mtk_drm_esd_ctx *esd_ctx = (struct mtk_drm_esd_ctx *)data;

	atomic_set(&esd_ctx->ext_te_event, 1);
	wake_up_interruptible(&esd_ctx->ext_te_wq);

	return IRQ_HANDLED;
}
#endif

static int _mtk_esd_check_eint(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 1;

	DDPINFO("[ESD]ESD check eint\n");

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
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
#ifdef CONFIG_MI_ESD_CHECK
#ifndef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
	ret = request_irq(esd_ctx->eint_irq, _mi_esd_check_ext_te_irq_handler,
			  IRQF_TRIGGER_FALLING, "ESD_TE-eint", esd_ctx);
	if (ret) {
		DDPPR_ERR("eint irq line not available!\n");
		return ret;
	}
#endif
#else
	ret = request_irq(esd_ctx->eint_irq, _esd_check_ext_te_irq_handler,
			IRQF_TRIGGER_RISING, "ESD_TE-eint", esd_ctx);
	if (ret) {
		DDPPR_ERR("eint irq line not available!\n");
		return ret;
	}

	disable_irq(esd_ctx->eint_irq);
#endif

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

static atomic_t panel_dead;
int get_panel_dead_flag(void) {
	return atomic_read(&panel_dead);
}
EXPORT_SYMBOL(get_panel_dead_flag);

static int mtk_drm_esd_recover(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	int ret = 0;
	unsigned int last_hrt_req= 0;

	atomic_set(&panel_dead, 1);

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
#ifdef CONFIG_MI_ESD_CHECK
	mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_RESTORE_BACKLIGHT, NULL);
#endif
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
	atomic_set(&panel_dead, 0);
	CRTC_MMP_EVENT_END(drm_crtc_index(crtc), esd_recovery, 0, ret);

	return 0;
}

#ifdef CONFIG_MI_ESD_CHECK
#ifndef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
static irqreturn_t _mi_esd_check_ext_te_irq_handler(int irq, void *data)
{
	struct mtk_drm_esd_ctx *esd_ctx = (struct mtk_drm_esd_ctx *)data;

	if (esd_ctx->panel_init) {
		atomic_set(&esd_ctx->ext_te_event, 1);
		wake_up_interruptible(&esd_ctx->ext_te_wq);
		DDPINFO("[ESD]_esd_check_ext_te_irq_handler is comming\n");
	}
	else {
		DDPINFO("[ESD]_esd_check_ext_te_irq_handler is comming, but ignore\n");
	}
	return IRQ_HANDLED;
}

static int mi_mtk_drm_esd_check_worker_kthread(void *data)
{
    struct sched_param param = { .sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int ret = 0;
	struct disp_event event;
	u8 panel_dead = 0;

	event.disp_id = MI_DISP_PRIMARY;
	event.type = MI_DISP_EVENT_PANEL_DEAD;
	event.length = sizeof(panel_dead);

	sched_setscheduler(current, SCHED_RR, &param);
	pr_info("start ESD thread\n");
	while (1) {
		msleep(ESD_CHECK_IRQ_PERIOD); /* 10ms */
		ret = wait_event_interruptible(esd_ctx->ext_te_wq,
		atomic_read(&esd_ctx->ext_te_event));
		if (ret < 0) {
			DDPINFO("[ESD]check thread waked up accidently\n");
			continue;
		}

		pr_info("ESD waked up\n");
		DDPINFO("[ESD]check thread waked up successfully\n");
		mutex_lock(&private->commit.lock);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		panel_dead = 1;
		mi_disp_feature_event_notify(&event, &panel_dead);

		/* 1.esd recovery */
		mtk_drm_esd_recover(crtc);

		panel_dead = 0;
		mi_disp_feature_event_notify(&event, &panel_dead);

		/* 2.clear atomic  ext_te_event */
		atomic_set(&esd_ctx->ext_te_event, 0);

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		DDPINFO("[ESD]check thread is over\n");

		/* 3.other check & recovery */
		if (kthread_should_stop())
			break;
	}
	return 0;
}
#else
static int mtk_drm_request_err_flag(struct drm_crtc *crtc)
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

	if (gpio_is_valid(mi_esd_ctx->err_flag_irq_gpio)) {
		mi_esd_ctx->err_flag_irq = gpio_to_irq(mi_esd_ctx->err_flag_irq_gpio);
		ret = gpio_request(mi_esd_ctx->err_flag_irq_gpio, "esd_err_irq_gpio");
		if (ret)
			DDPPR_ERR("Failed to request esd irq gpio %d, ret=%d\n",
				mi_esd_ctx->err_flag_irq_gpio, ret);
		else
			gpio_direction_input(mi_esd_ctx->err_flag_irq_gpio);
	} else {
		DDPPR_ERR("err_flag_irq_gpio is invalid\n");
		ret = -EINVAL;
	}

	if (mi_esd_ctx->err_flag_irq_gpio > 0) {
		ret = request_threaded_irq(mi_esd_ctx->err_flag_irq,
			NULL, _mi_esd_check_err_flag_irq_handler,
			mi_esd_ctx->err_flag_irq_flags,
			"esd_err_irq", mi_esd_ctx);
		if (ret) {
			DDPPR_ERR("display register esd irq failed\n");
		} else {
			DDPPR_ERR("display register esd irq success\n");
			disable_irq(mi_esd_ctx->err_flag_irq);
		}
	}
	_set_state(crtc, "err_flag_init");
	return ret;
}

static irqreturn_t _mi_esd_check_err_flag_irq_handler(int irq, void *data)
{
	struct mi_esd_ctx *mi_esd_ctx = (struct mi_esd_ctx *)data;
	if (gpio_get_value(mi_esd_ctx->err_flag_irq_gpio)) {
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

static void mtk_drm_esd_err_flag_irq_init(struct mi_esd_ctx *mi_esd_ctx, struct drm_crtc *crtc)
{
	mi_esd_ctx->disp_esd_irq_chk_task = kthread_create(
		mi_esd_err_flag_irq_check_worker_kthread, crtc, "err_flag_chk");
	init_waitqueue_head(&mi_esd_ctx->err_flag_wq);
	atomic_set(&mi_esd_ctx->err_flag_event, 0);
	wake_up_process(mi_esd_ctx->disp_esd_irq_chk_task);
}

int mtk_drm_esd_irq_ctrl(struct mi_esd_ctx *mi_esd_ctx,
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
		mtk_drm_esd_irq_ctrl(mi_esd_ctx, true);
	} else {
		mtk_drm_esd_irq_ctrl(mi_esd_ctx, false);
	}
}
#endif
#endif

static int mtk_drm_esd_check_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	struct disp_event event;
	int ret = 0;
	u8 panel_dead = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	if (!crtc) {
		DDPPR_ERR("%s invalid CRTC context, stop thread\n", __func__);

		return -EINVAL;
	}

	event.disp_id = MI_DISP_PRIMARY;
	event.type = MI_DISP_EVENT_PANEL_DEAD;
	event.length = sizeof(panel_dead);

	while (1) {
		msleep(ESD_CHECK_PERIOD);
		ret = wait_event_interruptible(
			esd_ctx->check_task_wq,
			atomic_read(&esd_ctx->check_wakeup) &&
			(atomic_read(&esd_ctx->target_time) ||
				esd_ctx->chk_mode == READ_EINT));
		if (ret < 0) {
			DDPINFO("[ESD]check thread waked up accidently\n");
			continue;
		}

		mutex_lock(&private->commit.lock);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		mtk_drm_trace_begin("esd");
		if (!mtk_drm_is_idle(crtc))
			atomic_set(&esd_ctx->target_time, 0);

		/* 1. esd check & recovery */
		if (!esd_ctx->chk_active) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mutex_unlock(&private->commit.lock);
			continue;
		}

		ret = mtk_drm_esd_check(crtc);
		if (ret) {
			DDPPR_ERR("[ESD]esd check fail, will do esd recovery.\n");
			panel_dead = 1;
			mi_disp_feature_event_notify(&event, &panel_dead);

			mtk_drm_esd_recover(crtc);

			panel_dead = 0;
			mi_disp_feature_event_notify(&event, &panel_dead);
		}

		mtk_drm_trace_end();
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);

		/* 2. other check & recovery */
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

	DDPMSG("%s %d\n", __func__, enable);
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
#ifdef CONFIG_MI_ESD_CHECK
#ifdef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
	struct mi_esd_ctx *mi_esd_ctx;
#endif
#endif
	panel_ext = mtk_crtc->panel_ext;
	if (!(panel_ext && panel_ext->params)) {
		DDPMSG("can't find panel_ext handle\n");
		return;
	}

#ifdef CONFIG_MI_ESD_CHECK
#ifdef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
	mi_esd_ctx = kzalloc(sizeof(*mi_esd_ctx), GFP_KERNEL);
	if (!mi_esd_ctx) {
		DDPPR_ERR("allocate MI ESD context failed!\n");
		return;
	}
	mtk_crtc->mi_esd_ctx = mi_esd_ctx;

	if(!mtk_drm_request_err_flag(crtc)) {
		mtk_drm_esd_err_flag_irq_init(mi_esd_ctx, crtc);
	} else {
		DDPPR_ERR("mi esd err flag gpio request failed\n");
		kfree(mtk_crtc->mi_esd_ctx);
	}
#endif
#endif

	if (_lcm_need_esd_check(panel_ext) == 0) {
		return;
	}

	DDPINFO("create ESD thread\n");
	/* primary display check thread init */
	esd_ctx = kzalloc(sizeof(*esd_ctx), GFP_KERNEL);
	if (!esd_ctx) {
		DDPPR_ERR("allocate ESD context failed!\n");
		return;
	}
	mtk_crtc->esd_ctx = esd_ctx;

	esd_ctx->disp_esd_chk_task = kthread_create(
		mtk_drm_esd_check_worker_kthread, crtc, "disp_echk");

	init_waitqueue_head(&esd_ctx->check_task_wq);
	init_waitqueue_head(&esd_ctx->ext_te_wq);
	atomic_set(&esd_ctx->check_wakeup, 0);
	atomic_set(&esd_ctx->ext_te_event, 0);
	atomic_set(&esd_ctx->target_time, 0);
	if (panel_ext->params->cust_esd_check == 1)
		esd_ctx->chk_mode = READ_LCM;
	else
		esd_ctx->chk_mode = READ_EINT;
	mtk_drm_request_eint(crtc);

#ifdef CONFIG_MI_ESD_CHECK
#ifdef CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD
	wake_up_process(esd_ctx->disp_esd_chk_task);
#else
	esd_ctx->mi_disp_esd_chk_task = kthread_create(
		mi_mtk_drm_esd_check_worker_kthread, crtc, "mi_disp_echk");
	wake_up_process(esd_ctx->mi_disp_esd_chk_task);
#endif
#endif
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
