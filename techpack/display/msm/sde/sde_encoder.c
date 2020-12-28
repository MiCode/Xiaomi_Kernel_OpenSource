/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/input.h>
#include <linux/seq_file.h>
#include <linux/sde_rsc.h>

#include "msm_drv.h"
#include "sde_kms.h"
#include <drm/drm_crtc.h>
#include <drm/drm_probe_helper.h>
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_ctl.h"
#include "sde_formats.h"
#include "sde_encoder.h"
#include "sde_encoder_phys.h"
#include "sde_hw_dsc.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_core_irq.h"
#include "sde_hw_top.h"
#include "sde_hw_qdss.h"
#include "sde_encoder_dce.h"

#define SDE_DEBUG_ENC(e, fmt, ...) SDE_DEBUG("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_ENC(e, fmt, ...) SDE_ERROR("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_DEBUG_PHYS(p, fmt, ...) SDE_DEBUG("enc%d intf%d pp%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
		(p) ? ((p)->hw_pp ? (p)->hw_pp->idx - PINGPONG_0 : -1) : -1, \
		##__VA_ARGS__)

#define SDE_ERROR_PHYS(p, fmt, ...) SDE_ERROR("enc%d intf%d pp%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
		(p) ? ((p)->hw_pp ? (p)->hw_pp->idx - PINGPONG_0 : -1) : -1, \
		##__VA_ARGS__)



#define MISR_BUFF_SIZE			256

#define IDLE_SHORT_TIMEOUT	1

#define EVT_TIME_OUT_SPLIT 2

/* Maximum number of VSYNC wait attempts for RSC state transition */
#define MAX_RSC_WAIT	5

/**
 * enum sde_enc_rc_events - events for resource control state machine
 * @SDE_ENC_RC_EVENT_KICKOFF:
 *	This event happens at NORMAL priority.
 *	Event that signals the start of the transfer. When this event is
 *	received, enable MDP/DSI core clocks and request RSC with CMD state.
 *	Regardless of the previous state, the resource should be in ON state
 *	at the end of this event. At the end of this event, a delayed work is
 *	scheduled to go to IDLE_PC state after IDLE_POWERCOLLAPSE_DURATION
 *	ktime.
 * @SDE_ENC_RC_EVENT_PRE_STOP:
 *	This event happens at NORMAL priority.
 *	This event, when received during the ON state, set RSC to IDLE, and
 *	and leave the RC STATE in the PRE_OFF state.
 *	It should be followed by the STOP event as part of encoder disable.
 *	If received during IDLE or OFF states, it will do nothing.
 * @SDE_ENC_RC_EVENT_STOP:
 *	This event happens at NORMAL priority.
 *	When this event is received, disable all the MDP/DSI core clocks, and
 *	disable IRQs. It should be called from the PRE_OFF or IDLE states.
 *	IDLE is expected when IDLE_PC has run, and PRE_OFF did nothing.
 *	PRE_OFF is expected when PRE_STOP was executed during the ON state.
 *	Resource state should be in OFF at the end of the event.
 * @SDE_ENC_RC_EVENT_PRE_MODESET:
 *	This event happens at NORMAL priority from a work item.
 *	Event signals that there is a seamless mode switch is in prgoress. A
 *	client needs to turn of only irq - leave clocks ON to reduce the mode
 *	switch latency.
 * @SDE_ENC_RC_EVENT_POST_MODESET:
 *	This event happens at NORMAL priority from a work item.
 *	Event signals that seamless mode switch is complete and resources are
 *	acquired. Clients wants to turn on the irq again and update the rsc
 *	with new vtotal.
 * @SDE_ENC_RC_EVENT_ENTER_IDLE:
 *	This event happens at NORMAL priority from a work item.
 *	Event signals that there were no frame updates for
 *	IDLE_POWERCOLLAPSE_DURATION time. This would disable MDP/DSI core clocks
 *      and request RSC with IDLE state and change the resource state to IDLE.
 * @SDE_ENC_RC_EVENT_EARLY_WAKEUP:
 *	This event is triggered from the input event thread when touch event is
 *	received from the input device. On receiving this event,
 *      - If the device is in SDE_ENC_RC_STATE_IDLE state, it turns ON the
	  clocks and enable RSC.
 *      - If the device is in SDE_ENC_RC_STATE_ON state, it resets the delayed
 *        off work since a new commit is imminent.
 */
enum sde_enc_rc_events {
	SDE_ENC_RC_EVENT_KICKOFF = 1,
	SDE_ENC_RC_EVENT_PRE_STOP,
	SDE_ENC_RC_EVENT_STOP,
	SDE_ENC_RC_EVENT_PRE_MODESET,
	SDE_ENC_RC_EVENT_POST_MODESET,
	SDE_ENC_RC_EVENT_ENTER_IDLE,
	SDE_ENC_RC_EVENT_EARLY_WAKEUP,
};

void sde_encoder_uidle_enable(struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	sde_enc = to_sde_encoder_virt(drm_enc);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->hw_ctl && phys->hw_ctl->ops.uidle_enable) {
			SDE_EVT32(DRMID(drm_enc), enable);
			phys->hw_ctl->ops.uidle_enable(phys->hw_ctl, enable);
		}
	}
}

static void _sde_encoder_pm_qos_add_request(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct device *cpu_dev;
	struct cpumask *cpu_mask = NULL;
	int cpu = 0;
	u32 cpu_dma_latency;

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	if (!sde_kms->catalog || !sde_kms->catalog->perf.cpu_mask)
		return;

	cpu_dma_latency = sde_kms->catalog->perf.cpu_dma_latency;
	cpumask_clear(&sde_enc->valid_cpu_mask);

	if (sde_enc->mode_info.frame_rate > DEFAULT_FPS)
		cpu_mask = to_cpumask(&sde_kms->catalog->perf.cpu_mask_perf);
	if (!cpu_mask &&
			sde_encoder_check_curr_mode(drm_enc,
				MSM_DISPLAY_CMD_MODE))
		cpu_mask = to_cpumask(&sde_kms->catalog->perf.cpu_mask);

	if (!cpu_mask)
		return;

	for_each_cpu(cpu, cpu_mask) {
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			SDE_ERROR("%s: failed to get cpu%d device\n", __func__,
					cpu);
			return;
		}
		cpumask_set_cpu(cpu, &sde_enc->valid_cpu_mask);
		dev_pm_qos_add_request(cpu_dev,
				&sde_enc->pm_qos_cpu_req[cpu],
				DEV_PM_QOS_RESUME_LATENCY, cpu_dma_latency);
		SDE_EVT32_VERBOSE(DRMID(drm_enc), cpu_dma_latency, cpu);
	}
}

static void _sde_encoder_pm_qos_remove_request(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct device *cpu_dev;
	int cpu = 0;

	for_each_cpu(cpu, &sde_enc->valid_cpu_mask) {
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			SDE_ERROR("%s: failed to get cpu%d device\n", __func__,
					cpu);
			continue;
		}
		dev_pm_qos_remove_request(&sde_enc->pm_qos_cpu_req[cpu]);
		SDE_EVT32_VERBOSE(DRMID(drm_enc), cpu);
	}
	cpumask_clear(&sde_enc->valid_cpu_mask);
}

static bool _sde_encoder_is_autorefresh_enabled(
		struct sde_encoder_virt *sde_enc)
{
	struct drm_connector *drm_conn;

	if (!sde_enc->cur_master ||
		!(sde_enc->disp_info.capabilities & MSM_DISPLAY_CAP_CMD_MODE))
		return false;

	drm_conn = sde_enc->cur_master->connector;

	if (!drm_conn || !drm_conn->state)
		return false;

	return sde_connector_get_property(drm_conn->state,
			CONNECTOR_PROP_AUTOREFRESH) ? true : false;
}

static void sde_configure_qdss(struct sde_encoder_virt *sde_enc,
				struct sde_hw_qdss *hw_qdss,
				struct sde_encoder_phys *phys, bool enable)
{
	if (sde_enc->qdss_status == enable)
		return;

	sde_enc->qdss_status = enable;

	phys->hw_mdptop->ops.set_mdp_hw_events(phys->hw_mdptop,
						sde_enc->qdss_status);
	hw_qdss->ops.enable_qdss_events(hw_qdss, sde_enc->qdss_status);
}

static int _sde_encoder_wait_timeout(int32_t drm_id, int32_t hw_id,
	s64 timeout_ms, struct sde_encoder_wait_info *info)
{
	int rc = 0;
	s64 wait_time_jiffies = msecs_to_jiffies(timeout_ms);
	ktime_t cur_ktime;
	ktime_t exp_ktime = ktime_add_ms(ktime_get(), timeout_ms);

	do {
		rc = wait_event_timeout(*(info->wq),
			atomic_read(info->atomic_cnt) == info->count_check,
			wait_time_jiffies);
		cur_ktime = ktime_get();

		SDE_EVT32(drm_id, hw_id, rc, ktime_to_ms(cur_ktime),
			timeout_ms, atomic_read(info->atomic_cnt),
			info->count_check);
	/* If we timed out, counter is valid and time is less, wait again */
	} while ((atomic_read(info->atomic_cnt) != info->count_check) &&
			(rc == 0) &&
			(ktime_compare_safe(exp_ktime, cur_ktime) > 0));

	return rc;
}

bool sde_encoder_is_primary_display(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc &&
		(sde_enc->disp_info.display_type ==
		SDE_CONNECTOR_PRIMARY);
}

bool sde_encoder_is_dsi_display(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc &&
		(sde_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DSI);
}

int sde_encoder_in_cont_splash(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc && sde_enc->cur_master &&
		sde_enc->cur_master->cont_splash_enabled;
}

void sde_encoder_helper_report_irq_timeout(struct sde_encoder_phys *phys_enc,
		enum sde_intr_idx intr_idx)
{
	SDE_EVT32(DRMID(phys_enc->parent),
			phys_enc->intf_idx - INTF_0,
			phys_enc->hw_pp->idx - PINGPONG_0,
			intr_idx);
	SDE_ERROR_PHYS(phys_enc, "irq %d timeout\n", intr_idx);

	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc,
				SDE_ENCODER_FRAME_EVENT_ERROR);
}

int sde_encoder_helper_wait_for_irq(struct sde_encoder_phys *phys_enc,
		enum sde_intr_idx intr_idx,
		struct sde_encoder_wait_info *wait_info)
{
	struct sde_encoder_irq *irq;
	u32 irq_status;
	int ret, i;

	if (!phys_enc || !wait_info || intr_idx >= INTR_IDX_MAX) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	/* note: do master / slave checking outside */

	/* return EWOULDBLOCK since we know the wait isn't necessary */
	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR_PHYS(phys_enc, "encoder is disabled\n");
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx, intr_idx, SDE_EVTLOG_ERROR);
		return -EWOULDBLOCK;
	}

	if (irq->irq_idx < 0) {
		SDE_DEBUG_PHYS(phys_enc, "irq %s hw %d disabled, skip wait\n",
				irq->name, irq->hw_idx);
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx);
		return 0;
	}

	SDE_DEBUG_PHYS(phys_enc, "pending_cnt %d\n",
			atomic_read(wait_info->atomic_cnt));
	SDE_EVT32_VERBOSE(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
		irq->irq_idx, phys_enc->hw_pp->idx - PINGPONG_0,
		atomic_read(wait_info->atomic_cnt), SDE_EVTLOG_FUNC_ENTRY);

	/*
	 * Some module X may disable interrupt for longer duration
	 * and it may trigger all interrupts including timer interrupt
	 * when module X again enable the interrupt.
	 * That may cause interrupt wait timeout API in this API.
	 * It is handled by split the wait timer in two halves.
	 */

	for (i = 0; i < EVT_TIME_OUT_SPLIT; i++) {
		ret = _sde_encoder_wait_timeout(DRMID(phys_enc->parent),
				irq->hw_idx,
				(wait_info->timeout_ms/EVT_TIME_OUT_SPLIT),
				wait_info);
		if (ret)
			break;
	}

	if (ret <= 0) {
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				irq->irq_idx, true);
		if (irq_status) {
			unsigned long flags;

			SDE_EVT32(DRMID(phys_enc->parent), intr_idx,
				irq->hw_idx, irq->irq_idx,
				phys_enc->hw_pp->idx - PINGPONG_0,
				atomic_read(wait_info->atomic_cnt));
			SDE_DEBUG_PHYS(phys_enc,
					"done but irq %d not triggered\n",
					irq->irq_idx);
			local_irq_save(flags);
			irq->cb.func(phys_enc, irq->irq_idx);
			local_irq_restore(flags);
			ret = 0;
		} else {
			ret = -ETIMEDOUT;
			SDE_EVT32(DRMID(phys_enc->parent), intr_idx,
				irq->hw_idx, irq->irq_idx,
				phys_enc->hw_pp->idx - PINGPONG_0,
				atomic_read(wait_info->atomic_cnt), irq_status,
				SDE_EVTLOG_ERROR);
		}
	} else {
		ret = 0;
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			irq->irq_idx, phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(wait_info->atomic_cnt));
	}

	SDE_EVT32_VERBOSE(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
		irq->irq_idx, ret, phys_enc->hw_pp->idx - PINGPONG_0,
		atomic_read(wait_info->atomic_cnt), SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

int sde_encoder_helper_register_irq(struct sde_encoder_phys *phys_enc,
		enum sde_intr_idx intr_idx)
{
	struct sde_encoder_irq *irq;
	int ret = 0;

	if (!phys_enc || intr_idx >= INTR_IDX_MAX) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	if (irq->irq_idx >= 0) {
		SDE_DEBUG_PHYS(phys_enc,
				"skipping already registered irq %s type %d\n",
				irq->name, irq->intr_type);
		return 0;
	}

	irq->irq_idx = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			irq->intr_type, irq->hw_idx);
	if (irq->irq_idx < 0) {
		SDE_ERROR_PHYS(phys_enc,
			"failed to lookup IRQ index for %s type:%d\n",
			irq->name, irq->intr_type);
		return -EINVAL;
	}

	ret = sde_core_irq_register_callback(phys_enc->sde_kms, irq->irq_idx,
			&irq->cb);
	if (ret) {
		SDE_ERROR_PHYS(phys_enc,
			"failed to register IRQ callback for %s\n",
			irq->name);
		irq->irq_idx = -EINVAL;
		return ret;
	}

	ret = sde_core_irq_enable(phys_enc->sde_kms, &irq->irq_idx, 1);
	if (ret) {
		SDE_ERROR_PHYS(phys_enc,
			"enable IRQ for intr:%s failed, irq_idx %d\n",
			irq->name, irq->irq_idx);

		sde_core_irq_unregister_callback(phys_enc->sde_kms,
				irq->irq_idx, &irq->cb);

		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx, SDE_EVTLOG_ERROR);
		irq->irq_idx = -EINVAL;
		return ret;
	}

	SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx, irq->irq_idx);
	SDE_DEBUG_PHYS(phys_enc, "registered irq %s idx: %d\n",
			irq->name, irq->irq_idx);

	return ret;
}

int sde_encoder_helper_unregister_irq(struct sde_encoder_phys *phys_enc,
		enum sde_intr_idx intr_idx)
{
	struct sde_encoder_irq *irq;
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	/* silently skip irqs that weren't registered */
	if (irq->irq_idx < 0) {
		SDE_ERROR(
			"extra unregister irq, enc%d intr_idx:0x%x hw_idx:0x%x irq_idx:0x%x\n",
				DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx);
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx, SDE_EVTLOG_ERROR);
		return 0;
	}

	ret = sde_core_irq_disable(phys_enc->sde_kms, &irq->irq_idx, 1);
	if (ret)
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx, ret, SDE_EVTLOG_ERROR);

	ret = sde_core_irq_unregister_callback(phys_enc->sde_kms, irq->irq_idx,
			&irq->cb);
	if (ret)
		SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
				irq->irq_idx, ret, SDE_EVTLOG_ERROR);

	SDE_EVT32(DRMID(phys_enc->parent), intr_idx, irq->hw_idx, irq->irq_idx);
	SDE_DEBUG_PHYS(phys_enc, "unregistered %d\n", irq->irq_idx);

	irq->irq_idx = -EINVAL;

	return 0;
}

void sde_encoder_get_hw_resources(struct drm_encoder *drm_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int ret, i = 0;

	if (!hw_res || !drm_enc || !conn_state || !hw_res->comp_info) {
		SDE_ERROR("rc %d, drm_enc %d, res %d, state %d, comp-info %d\n",
				-EINVAL, !drm_enc, !hw_res, !conn_state,
				hw_res ? !hw_res->comp_info : 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	hw_res->display_num_of_h_tiles = sde_enc->display_num_of_h_tiles;
	hw_res->display_type = sde_enc->disp_info.display_type;

	/* Query resources used by phys encs, expected to be without overlap */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.get_hw_resources)
			phys->ops.get_hw_resources(phys, hw_res, conn_state);
	}

	/*
	 * NOTE: Do not use sde_encoder_get_mode_info here as this function is
	 * called from atomic_check phase. Use the below API to get mode
	 * information of the temporary conn_state passed
	 */
	ret = sde_connector_state_get_topology(conn_state, &hw_res->topology);
	if (ret)
		SDE_ERROR("failed to get topology ret %d\n", ret);

	ret = sde_connector_state_get_compression_info(conn_state,
			hw_res->comp_info);
	if (ret)
		SDE_ERROR("failed to get compression info ret %d\n", ret);
}

void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;
	unsigned int num_encs;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");
	num_encs = sde_enc->num_phys_encs;

	mutex_lock(&sde_enc->enc_lock);
	sde_rsc_client_destroy(sde_enc->rsc_client);

	for (i = 0; i < num_encs; i++) {
		struct sde_encoder_phys *phys;

		phys = sde_enc->phys_vid_encs[i];
		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_vid_encs[i] = NULL;
		}

		phys = sde_enc->phys_cmd_encs[i];
		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_cmd_encs[i] = NULL;
		}

		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_encs[i] = NULL;
		}
	}

	if (sde_enc->num_phys_encs)
		SDE_ERROR_ENC(sde_enc, "expected 0 num_phys_encs not %d\n",
				sde_enc->num_phys_encs);
	sde_enc->num_phys_encs = 0;
	mutex_unlock(&sde_enc->enc_lock);

	drm_encoder_cleanup(drm_enc);
	mutex_destroy(&sde_enc->enc_lock);

	kfree(sde_enc->input_handler);
	sde_enc->input_handler = NULL;

	kfree(sde_enc);
}

void sde_encoder_helper_update_intf_cfg(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_hw_intf_cfg_v1 *intf_cfg;
	enum sde_3d_blend_mode mode_3d;

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid args, encoder %d\n", !phys_enc);
		return;
	}

	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	intf_cfg = &sde_enc->cur_master->intf_cfg_v1;

	SDE_DEBUG_ENC(sde_enc,
			"intf_cfg updated for %d at idx %d\n",
			phys_enc->intf_idx,
			intf_cfg->intf_count);


	/* setup interface configuration */
	if (intf_cfg->intf_count >= MAX_INTF_PER_CTL_V1) {
		pr_err("invalid inf_count %d\n", intf_cfg->intf_count);
		return;
	}
	intf_cfg->intf[intf_cfg->intf_count++] = phys_enc->intf_idx;
	if (phys_enc == sde_enc->cur_master) {
		if (sde_enc->cur_master->intf_mode == INTF_MODE_CMD)
			intf_cfg->intf_mode_sel = SDE_CTL_MODE_SEL_CMD;
		else
			intf_cfg->intf_mode_sel = SDE_CTL_MODE_SEL_VID;
	}

	/* configure this interface as master for split display */
	if (phys_enc->split_role == ENC_ROLE_MASTER)
		intf_cfg->intf_master = phys_enc->hw_intf->idx;

	/* setup which pp blk will connect to this intf */
	if (phys_enc->hw_intf->ops.bind_pingpong_blk)
		phys_enc->hw_intf->ops.bind_pingpong_blk(
				phys_enc->hw_intf,
				true,
				phys_enc->hw_pp->idx);


	/*setup merge_3d configuration */
	mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);

	if (mode_3d && phys_enc->hw_pp->merge_3d &&
			intf_cfg->merge_3d_count < MAX_MERGE_3D_PER_CTL_V1)
		intf_cfg->merge_3d[intf_cfg->merge_3d_count++] =
			phys_enc->hw_pp->merge_3d->idx;

	if (phys_enc->hw_pp->ops.setup_3d_mode)
		phys_enc->hw_pp->ops.setup_3d_mode(phys_enc->hw_pp,
				mode_3d);
}

void sde_encoder_helper_split_config(
		struct sde_encoder_phys *phys_enc,
		enum sde_intf interface)
{
	struct sde_encoder_virt *sde_enc;
	struct split_pipe_cfg *cfg;
	struct sde_hw_mdp *hw_mdptop;
	enum sde_rm_topology_name topology;
	struct msm_display_info *disp_info;

	if (!phys_enc || !phys_enc->hw_mdptop || !phys_enc->parent) {
		SDE_ERROR("invalid arg(s), encoder %d\n", !phys_enc);
		return;
	}

	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	hw_mdptop = phys_enc->hw_mdptop;
	disp_info = &sde_enc->disp_info;
	cfg = &phys_enc->hw_intf->cfg;
	memset(cfg, 0, sizeof(*cfg));

	if (disp_info->intf_type != DRM_MODE_CONNECTOR_DSI)
		return;

	if (disp_info->capabilities & MSM_DISPLAY_SPLIT_LINK)
		cfg->split_link_en = true;

	/**
	 * disable split modes since encoder will be operating in as the only
	 * encoder, either for the entire use case in the case of, for example,
	 * single DSI, or for this frame in the case of left/right only partial
	 * update.
	 */
	if (phys_enc->split_role == ENC_ROLE_SOLO) {
		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, cfg);
		if (hw_mdptop->ops.setup_pp_split)
			hw_mdptop->ops.setup_pp_split(hw_mdptop, cfg);
		return;
	}

	cfg->en = true;
	cfg->mode = phys_enc->intf_mode;
	cfg->intf = interface;

	if (cfg->en && phys_enc->ops.needs_single_flush &&
			phys_enc->ops.needs_single_flush(phys_enc))
		cfg->split_flush_en = true;

	topology = sde_connector_get_topology_name(phys_enc->connector);
	if (topology == SDE_RM_TOPOLOGY_PPSPLIT)
		cfg->pp_split_slave = cfg->intf;
	else
		cfg->pp_split_slave = INTF_MAX;

	if (phys_enc->split_role == ENC_ROLE_MASTER) {
		SDE_DEBUG_ENC(sde_enc, "enable %d\n", cfg->en);

		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, cfg);
	} else if (sde_enc->hw_pp[0]) {
		/*
		 * slave encoder
		 * - determine split index from master index,
		 *   assume master is first pp
		 */
		cfg->pp_split_index = sde_enc->hw_pp[0]->idx - PINGPONG_0;
		SDE_DEBUG_ENC(sde_enc, "master using pp%d\n",
				cfg->pp_split_index);

		if (hw_mdptop->ops.setup_pp_split)
			hw_mdptop->ops.setup_pp_split(hw_mdptop, cfg);
	}
}

bool sde_encoder_in_clone_mode(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	int i = 0;

	if (!drm_enc)
		return false;

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc)
		return false;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->in_clone_mode)
			return true;
	}

	return false;
}

bool sde_encoder_is_cwb_disabling(struct drm_encoder *drm_enc,
	struct drm_crtc *crtc)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	if (!drm_enc)
		return false;

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (sde_enc->disp_info.intf_type != DRM_MODE_CONNECTOR_VIRTUAL)
		return false;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (sde_encoder_phys_is_cwb_disabling(phys, crtc))
			return true;
	}

	return false;
}

static int _sde_encoder_atomic_check_phys_enc(struct sde_encoder_virt *sde_enc,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	const struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	int i = 0;
	int ret = 0;

	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;

	/* perform atomic check on the first physical encoder (master) */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.atomic_check)
			ret = phys->ops.atomic_check(phys, crtc_state,
					conn_state);
		else if (phys && phys->ops.mode_fixup)
			if (!phys->ops.mode_fixup(phys, mode, adj_mode))
				ret = -EINVAL;

		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"mode unsupported, phys idx %d\n", i);
			break;
		}
	}

	return ret;
}

static int _sde_encoder_atomic_check_pu_roi(struct sde_encoder_virt *sde_enc,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state,
	struct sde_connector_state *sde_conn_state,
	struct sde_crtc_state *sde_crtc_state)
{
	int ret = 0;

	if (crtc_state->mode_changed || crtc_state->active_changed) {
		struct sde_rect mode_roi, roi;

		mode_roi.x = 0;
		mode_roi.y = 0;
		mode_roi.w = crtc_state->adjusted_mode.hdisplay;
		mode_roi.h = crtc_state->adjusted_mode.vdisplay;

		if (sde_conn_state->rois.num_rects) {
			sde_kms_rect_merge_rectangles(
					&sde_conn_state->rois, &roi);
			if (!sde_kms_rect_is_equal(&mode_roi, &roi)) {
				SDE_ERROR_ENC(sde_enc,
					"roi (%d,%d,%d,%d) on connector invalid during modeset\n",
					roi.x, roi.y, roi.w, roi.h);
				ret = -EINVAL;
			}
		}

		if (sde_crtc_state->user_roi_list.num_rects) {
			sde_kms_rect_merge_rectangles(
					&sde_crtc_state->user_roi_list, &roi);
			if (!sde_kms_rect_is_equal(&mode_roi, &roi)) {
				SDE_ERROR_ENC(sde_enc,
					"roi (%d,%d,%d,%d) on crtc invalid during modeset\n",
					roi.x, roi.y, roi.w, roi.h);
				ret = -EINVAL;
			}
		}
	}

	return ret;
}

static int _sde_encoder_atomic_check_reserve(struct drm_encoder *drm_enc,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state,
	struct sde_encoder_virt *sde_enc, struct sde_kms *sde_kms,
	struct sde_connector *sde_conn,
	struct sde_connector_state *sde_conn_state)
{
	int ret = 0;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;

	if (sde_conn && drm_atomic_crtc_needs_modeset(crtc_state)) {
		struct msm_display_topology *topology = NULL;

		ret = sde_connector_get_mode_info(&sde_conn->base,
				adj_mode, &sde_conn_state->mode_info);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"failed to get mode info, rc = %d\n", ret);
			return ret;
		}

		if (sde_conn_state->mode_info.comp_info.comp_type &&
			sde_conn_state->mode_info.comp_info.comp_ratio >=
					MSM_DISPLAY_COMPRESSION_RATIO_MAX) {
			SDE_ERROR_ENC(sde_enc,
				"invalid compression ratio: %d\n",
				sde_conn_state->mode_info.comp_info.comp_ratio);
			ret = -EINVAL;
			return ret;
		}

		/* Reserve dynamic resources, indicating atomic_check phase */
		ret = sde_rm_reserve(&sde_kms->rm, drm_enc, crtc_state,
			conn_state, true);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"RM failed to reserve resources, rc = %d\n",
				ret);
			return ret;
		}

		/**
		 * Update connector state with the topology selected for the
		 * resource set validated. Reset the topology if we are
		 * de-activating crtc.
		 */
		if (crtc_state->active)
			topology = &sde_conn_state->mode_info.topology;

		ret = sde_rm_update_topology(&sde_kms->rm,
				conn_state, topology);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"RM failed to update topology, rc: %d\n", ret);
			return ret;
		}

		ret = sde_connector_set_blob_data(conn_state->connector,
				conn_state,
				CONNECTOR_PROP_SDE_INFO);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"connector failed to update info, rc: %d\n",
				ret);
			return ret;
		}
	}

	return ret;
}

static int sde_encoder_virt_atomic_check(
	struct drm_encoder *drm_enc, struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_kms *sde_kms;
	const struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	struct sde_connector *sde_conn = NULL;
	struct sde_connector_state *sde_conn_state = NULL;
	struct sde_crtc_state *sde_crtc_state = NULL;
	enum sde_rm_topology_name old_top;
	int ret = 0;

	if (!drm_enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arg(s), drm_enc %d, crtc/conn state %d/%d\n",
				!drm_enc, !crtc_state, !conn_state);
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms)
		return -EINVAL;

	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;
	sde_conn = to_sde_connector(conn_state->connector);
	sde_conn_state = to_sde_connector_state(conn_state);
	sde_crtc_state = to_sde_crtc_state(crtc_state);

	SDE_EVT32(DRMID(drm_enc), crtc_state->mode_changed,
		crtc_state->active_changed, crtc_state->connectors_changed);

	ret = _sde_encoder_atomic_check_phys_enc(sde_enc, crtc_state,
			conn_state);
	if (ret)
		return ret;

	ret = _sde_encoder_atomic_check_pu_roi(sde_enc, crtc_state,
			conn_state, sde_conn_state, sde_crtc_state);
	if (ret)
		return ret;

	/**
	 * record topology in previous atomic state to be able to handle
	 * topology transitions correctly.
	 */
	old_top  = sde_connector_get_property(conn_state,
				CONNECTOR_PROP_TOPOLOGY_NAME);
	ret = sde_connector_set_old_topology_name(conn_state, old_top);
	if (ret)
		return ret;

	ret = _sde_encoder_atomic_check_reserve(drm_enc, crtc_state,
			conn_state, sde_enc, sde_kms, sde_conn, sde_conn_state);
	if (ret)
		return ret;

	ret = sde_connector_roi_v1_check_roi(conn_state);
	if (ret) {
		SDE_ERROR_ENC(sde_enc, "connector roi check failed, rc: %d",
				ret);
		return ret;
	}

	drm_mode_set_crtcinfo(adj_mode, 0);
	SDE_EVT32(DRMID(drm_enc), adj_mode->flags, adj_mode->private_flags);

	return ret;
}

static void _sde_encoder_get_connector_roi(
		struct sde_encoder_virt *sde_enc,
		struct sde_rect *merged_conn_roi)
{
	struct drm_connector *drm_conn;
	struct sde_connector_state *c_state;

	if (!sde_enc || !merged_conn_roi)
		return;

	drm_conn = sde_enc->phys_encs[0]->connector;

	if (!drm_conn || !drm_conn->state)
		return;

	c_state = to_sde_connector_state(drm_conn->state);
	sde_kms_rect_merge_rectangles(&c_state->rois, merged_conn_roi);
}

static int _sde_encoder_update_roi(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct drm_connector *drm_conn;
	struct drm_display_mode *adj_mode;
	struct sde_rect roi;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder parameter\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->crtc || !sde_enc->crtc->state) {
		SDE_ERROR("invalid crtc parameter\n");
		return -EINVAL;
	}

	if (!sde_enc->cur_master) {
		SDE_ERROR("invalid cur_master parameter\n");
		return -EINVAL;
	}

	adj_mode = &sde_enc->cur_master->cached_mode;
	drm_conn = sde_enc->cur_master->connector;

	_sde_encoder_get_connector_roi(sde_enc, &roi);
	if (sde_kms_rect_is_null(&roi)) {
		roi.w = adj_mode->hdisplay;
		roi.h = adj_mode->vdisplay;
	}

	memcpy(&sde_enc->prv_conn_roi, &sde_enc->cur_conn_roi,
			sizeof(sde_enc->prv_conn_roi));
	memcpy(&sde_enc->cur_conn_roi, &roi, sizeof(sde_enc->cur_conn_roi));

	return 0;
}

void sde_encoder_helper_vsync_config(struct sde_encoder_phys *phys_enc,
			u32 vsync_source, bool is_dummy)
{
	struct sde_vsync_source_cfg vsync_cfg = { 0 };
	struct sde_kms *sde_kms;
	struct sde_hw_mdp *hw_mdptop;
	struct sde_encoder_virt *sde_enc;
	int i;

	sde_enc = to_sde_encoder_virt(phys_enc->parent);

	if (!sde_enc) {
		SDE_ERROR("invalid param sde_enc:%d\n", sde_enc != NULL);
		return;
	} else if (sde_enc->num_phys_encs > ARRAY_SIZE(sde_enc->hw_pp)) {
		SDE_ERROR("invalid num phys enc %d/%d\n",
				sde_enc->num_phys_encs,
				(int) ARRAY_SIZE(sde_enc->hw_pp));
		return;
	}

	sde_kms = sde_encoder_get_kms(&sde_enc->base);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	}

	hw_mdptop = sde_kms->hw_mdp;
	if (!hw_mdptop) {
		SDE_ERROR("invalid mdptop\n");
		return;
	}

	if (hw_mdptop->ops.setup_vsync_source) {
		for (i = 0; i < sde_enc->num_phys_encs; i++)
			vsync_cfg.ppnumber[i] = sde_enc->hw_pp[i]->idx;

		vsync_cfg.pp_count = sde_enc->num_phys_encs;
		vsync_cfg.frame_rate = sde_enc->mode_info.frame_rate;
		vsync_cfg.vsync_source = vsync_source;
		vsync_cfg.is_dummy = is_dummy;

		hw_mdptop->ops.setup_vsync_source(hw_mdptop, &vsync_cfg);
	}
}

static void _sde_encoder_update_vsync_source(struct sde_encoder_virt *sde_enc,
			struct msm_display_info *disp_info, bool is_dummy)
{
	struct sde_encoder_phys *phys;
	int i;
	u32 vsync_source;

	if (!sde_enc || !disp_info) {
		SDE_ERROR("invalid param sde_enc:%d or disp_info:%d\n",
					sde_enc != NULL, disp_info != NULL);
		return;
	} else if (sde_enc->num_phys_encs > ARRAY_SIZE(sde_enc->hw_pp)) {
		SDE_ERROR("invalid num phys enc %d/%d\n",
				sde_enc->num_phys_encs,
				(int) ARRAY_SIZE(sde_enc->hw_pp));
		return;
	}

	if (sde_encoder_check_curr_mode(&sde_enc->base, MSM_DISPLAY_CMD_MODE)) {
		if (is_dummy)
			vsync_source = SDE_VSYNC_SOURCE_WD_TIMER_0 -
					sde_enc->te_source;
		else if (disp_info->is_te_using_watchdog_timer)
			vsync_source = SDE_VSYNC_SOURCE_WD_TIMER_4 +
					sde_enc->te_source;
		else
			vsync_source = sde_enc->te_source;

		SDE_EVT32(DRMID(&sde_enc->base), vsync_source, is_dummy,
				disp_info->is_te_using_watchdog_timer);

		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			phys = sde_enc->phys_encs[i];

			if (phys && phys->ops.setup_vsync_source)
				phys->ops.setup_vsync_source(phys,
					vsync_source, is_dummy);
		}
	}
}



int sde_encoder_helper_switch_vsync(struct drm_encoder *drm_enc,
	 bool watchdog_te)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_display_info disp_info;

	if (!drm_enc) {
		pr_err("invalid drm encoder\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	sde_encoder_control_te(drm_enc, false);

	memcpy(&disp_info, &sde_enc->disp_info, sizeof(disp_info));
	disp_info.is_te_using_watchdog_timer = watchdog_te;
	_sde_encoder_update_vsync_source(sde_enc, &disp_info, false);

	sde_encoder_control_te(drm_enc, true);

	return 0;
}

static int _sde_encoder_rsc_client_update_vsync_wait(
	struct drm_encoder *drm_enc, struct sde_encoder_virt *sde_enc,
	int wait_vblank_crtc_id)
{
	int wait_refcount = 0, ret = 0;
	int pipe = -1;
	int wait_count = 0;
	struct drm_crtc *primary_crtc;
	struct drm_crtc *crtc;

	crtc = sde_enc->crtc;

	if (wait_vblank_crtc_id)
		wait_refcount =
			sde_rsc_client_get_vsync_refcount(sde_enc->rsc_client);
	SDE_EVT32_VERBOSE(DRMID(drm_enc), wait_vblank_crtc_id, wait_refcount,
			SDE_EVTLOG_FUNC_ENTRY);

	if (crtc->base.id != wait_vblank_crtc_id) {
		primary_crtc = drm_crtc_find(drm_enc->dev,
				NULL, wait_vblank_crtc_id);
		if (!primary_crtc) {
			SDE_ERROR_ENC(sde_enc,
					"failed to find primary crtc id %d\n",
					wait_vblank_crtc_id);
			return -EINVAL;
		}
		pipe = drm_crtc_index(primary_crtc);
	}

	/**
	 * note: VBLANK is expected to be enabled at this point in
	 * resource control state machine if on primary CRTC
	 */
	for (wait_count = 0; wait_count < MAX_RSC_WAIT; wait_count++) {
		if (sde_rsc_client_is_state_update_complete(
				sde_enc->rsc_client))
			break;

		if (crtc->base.id == wait_vblank_crtc_id)
			ret = sde_encoder_wait_for_event(drm_enc,
					MSM_ENC_VBLANK);
		else
			drm_wait_one_vblank(drm_enc->dev, pipe);

		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"wait for vblank failed ret:%d\n", ret);
			/**
			 * rsc hardware may hang without vsync. avoid rsc hang
			 * by generating the vsync from watchdog timer.
			 */
			if (crtc->base.id == wait_vblank_crtc_id)
				sde_encoder_helper_switch_vsync(drm_enc, true);
		}
	}

	if (wait_count >= MAX_RSC_WAIT)
		SDE_EVT32(DRMID(drm_enc), wait_vblank_crtc_id, wait_count,
				SDE_EVTLOG_ERROR);

	if (wait_refcount)
		sde_rsc_client_reset_vsync_refcount(sde_enc->rsc_client);
	SDE_EVT32_VERBOSE(DRMID(drm_enc), wait_vblank_crtc_id, wait_refcount,
			SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

static int _sde_encoder_update_rsc_client(
		struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	struct drm_crtc *crtc;
	enum sde_rsc_state rsc_state = SDE_RSC_IDLE_STATE;
	struct sde_rsc_cmd_config *rsc_config;
	int ret;
	struct msm_display_info *disp_info;
	struct msm_mode_info *mode_info;
	int wait_vblank_crtc_id = SDE_RSC_INVALID_CRTC_ID;
	u32 qsync_mode = 0, v_front_porch;
	struct drm_display_mode *mode;
	bool is_vid_mode;
	struct drm_encoder *enc;

	if (!drm_enc || !drm_enc->dev) {
		SDE_ERROR("invalid encoder arguments\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	mode_info = &sde_enc->mode_info;

	crtc = sde_enc->crtc;

	if (!sde_enc->crtc) {
		SDE_ERROR("invalid crtc parameter\n");
		return -EINVAL;
	}
	disp_info = &sde_enc->disp_info;
	rsc_config = &sde_enc->rsc_config;

	if (!sde_enc->rsc_client) {
		SDE_DEBUG_ENC(sde_enc, "rsc client not created\n");
		return 0;
	}

	/**
	 * only primary command mode panel without Qsync can request CMD state.
	 * all other panels/displays can request for VID state including
	 * secondary command mode panel.
	 * Clone mode encoder can request CLK STATE only.
	 */
	if (sde_enc->cur_master)
		qsync_mode = sde_connector_get_qsync_mode(
				sde_enc->cur_master->connector);

	/* left primary encoder keep vote */
	if (sde_encoder_in_clone_mode(drm_enc)) {
		SDE_EVT32(rsc_state, SDE_EVTLOG_FUNC_CASE1);
		return 0;
	}

	if ((disp_info->display_type != SDE_CONNECTOR_PRIMARY) ||
			(disp_info->display_type && qsync_mode))
		rsc_state = enable ? SDE_RSC_CLK_STATE : SDE_RSC_IDLE_STATE;
	else if (sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE))
		rsc_state = enable ? SDE_RSC_CMD_STATE : SDE_RSC_IDLE_STATE;
	else if (sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_VIDEO_MODE))
		rsc_state = enable ? SDE_RSC_VID_STATE : SDE_RSC_IDLE_STATE;

	drm_for_each_encoder(enc, drm_enc->dev) {
		if (enc->base.id != drm_enc->base.id &&
			 sde_encoder_in_cont_splash(enc))
			rsc_state = SDE_RSC_CLK_STATE;
	}

	SDE_EVT32(rsc_state, qsync_mode);

	is_vid_mode = sde_encoder_check_curr_mode(&sde_enc->base,
				MSM_DISPLAY_VIDEO_MODE);
	mode = &sde_enc->crtc->state->mode;
	v_front_porch = mode->vsync_start - mode->vdisplay;

	/* compare specific items and reconfigure the rsc */
	if ((rsc_config->fps != mode_info->frame_rate) ||
	    (rsc_config->vtotal != mode_info->vtotal) ||
	    (rsc_config->prefill_lines != mode_info->prefill_lines) ||
	    (rsc_config->jitter_numer != mode_info->jitter_numer) ||
	    (rsc_config->jitter_denom != mode_info->jitter_denom)) {

		rsc_config->fps = mode_info->frame_rate;
		rsc_config->vtotal = mode_info->vtotal;
		/*
		 * for video mode, prefill lines should not go beyond vertical
		 * front porch for RSCC configuration. This will ensure bw
		 * downvotes are not sent within the active region. Additional
		 * -1 is to give one line time for rscc mode min_threshold.
		 */
		if (is_vid_mode && (mode_info->prefill_lines >= v_front_porch))
			rsc_config->prefill_lines = v_front_porch - 1;
		else
			rsc_config->prefill_lines = mode_info->prefill_lines;

		rsc_config->jitter_numer = mode_info->jitter_numer;
		rsc_config->jitter_denom = mode_info->jitter_denom;
		sde_enc->rsc_state_init = false;
	}

	if (rsc_state != SDE_RSC_IDLE_STATE && !sde_enc->rsc_state_init
			&& (disp_info->display_type == SDE_CONNECTOR_PRIMARY)) {
		/* update it only once */
		sde_enc->rsc_state_init = true;

		ret = sde_rsc_client_state_update(sde_enc->rsc_client,
			rsc_state, rsc_config, crtc->base.id,
			&wait_vblank_crtc_id);
	} else {
		ret = sde_rsc_client_state_update(sde_enc->rsc_client,
			rsc_state, NULL, crtc->base.id,
			&wait_vblank_crtc_id);
	}

	/**
	 * if RSC performed a state change that requires a VBLANK wait, it will
	 * set wait_vblank_crtc_id to the CRTC whose VBLANK we must wait on.
	 *
	 * if we are the primary display, we will need to enable and wait
	 * locally since we hold the commit thread
	 *
	 * if we are an external display, we must send a signal to the primary
	 * to enable its VBLANK and wait one, since the RSC hardware is driven
	 * by the primary panel's VBLANK signals
	 */
	SDE_EVT32_VERBOSE(DRMID(drm_enc), wait_vblank_crtc_id);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
				"sde rsc client update failed ret:%d\n", ret);
		return ret;
	} else if (wait_vblank_crtc_id == SDE_RSC_INVALID_CRTC_ID) {
		return ret;
	}

	ret = _sde_encoder_rsc_client_update_vsync_wait(drm_enc,
			sde_enc, wait_vblank_crtc_id);

	return ret;
}

void sde_encoder_irq_control(struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	SDE_DEBUG_ENC(sde_enc, "enable:%d\n", enable);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.irq_control)
			phys->ops.irq_control(phys, enable);
	}
	sde_kms_cpu_vote_for_irq(sde_encoder_get_kms(drm_enc), enable);

}

/* keep track of the userspace vblank during modeset */
static void _sde_encoder_modeset_helper_locked(struct drm_encoder *drm_enc,
		u32 sw_event)
{
	struct sde_encoder_virt *sde_enc;
	bool enable;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "sw_event:%d, vblank_enabled:%d\n",
			sw_event, sde_enc->vblank_enabled);

	/* nothing to do if vblank not enabled by userspace */
	if (!sde_enc->vblank_enabled)
		return;

	/* disable vblank on pre_modeset */
	if (sw_event == SDE_ENC_RC_EVENT_PRE_MODESET)
		enable = false;
	/* enable vblank on post_modeset */
	else if (sw_event == SDE_ENC_RC_EVENT_POST_MODESET)
		enable = true;
	else
		return;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.control_vblank_irq)
			phys->ops.control_vblank_irq(phys, enable);
	}
}

struct sde_rsc_client *sde_encoder_get_rsc_client(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc)
		return NULL;
	sde_enc = to_sde_encoder_virt(drm_enc);
	return sde_enc->rsc_client;
}

static int _sde_encoder_resource_control_helper(struct drm_encoder *drm_enc,
		bool enable)
{
	struct sde_kms *sde_kms;
	struct sde_encoder_virt *sde_enc;
	int rc;

	sde_enc = to_sde_encoder_virt(drm_enc);
	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms)
		return -EINVAL;

	SDE_DEBUG_ENC(sde_enc, "enable:%d\n", enable);
	SDE_EVT32(DRMID(drm_enc), enable);

	if (!sde_enc->cur_master) {
		SDE_ERROR("encoder master not set\n");
		return -EINVAL;
	}

	if (enable) {
		/* enable SDE core clks */
		rc = pm_runtime_get_sync(drm_enc->dev->dev);
		if (rc < 0) {
			SDE_ERROR("failed to enable power resource %d\n", rc);
			SDE_EVT32(rc, SDE_EVTLOG_ERROR);
			return rc;
		}

		sde_enc->elevated_ahb_vote = true;
		/* enable DSI clks */
		rc = sde_connector_clk_ctrl(sde_enc->cur_master->connector,
				true);
		if (rc) {
			SDE_ERROR("failed to enable clk control %d\n", rc);
			pm_runtime_put_sync(drm_enc->dev->dev);
			return rc;
		}

		/* enable all the irq */
		sde_encoder_irq_control(drm_enc, true);

		_sde_encoder_pm_qos_add_request(drm_enc);

	} else {
		_sde_encoder_pm_qos_remove_request(drm_enc);

		/* disable all the irq */
		sde_encoder_irq_control(drm_enc, false);

		/* disable DSI clks */
		sde_connector_clk_ctrl(sde_enc->cur_master->connector, false);

		/* disable SDE core clks */
		pm_runtime_put_sync(drm_enc->dev->dev);
	}

	return 0;
}

static void sde_encoder_misr_configure(struct drm_encoder *drm_enc,
		bool enable, u32 frame_count)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	if (!sde_enc->misr_reconfigure)
		return;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || !phys->ops.setup_misr)
			continue;

		phys->ops.setup_misr(phys, enable, frame_count);
	}
	sde_enc->misr_reconfigure = false;
}

static void sde_encoder_input_event_handler(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_thread *disp_thread = NULL;
	struct msm_drm_private *priv = NULL;

	if (!handle || !handle->handler || !handle->handler->private) {
		SDE_ERROR("invalid encoder for the input event\n");
		return;
	}

	drm_enc = (struct drm_encoder *)handle->handler->private;
	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	priv = drm_enc->dev->dev_private;
	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->crtc || (sde_enc->crtc->index
			>= ARRAY_SIZE(priv->disp_thread))) {
		SDE_DEBUG_ENC(sde_enc,
			"invalid cached CRTC: %d or crtc index: %d\n",
			sde_enc->crtc == NULL,
			sde_enc->crtc ? sde_enc->crtc->index : -EINVAL);
		return;
	}

	SDE_EVT32_VERBOSE(DRMID(drm_enc));

	disp_thread = &priv->disp_thread[sde_enc->crtc->index];

	kthread_queue_work(&disp_thread->worker,
				&sde_enc->input_event_work);
}

void sde_encoder_control_idle_pc(struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	/* return early if there is no state change */
	if (sde_enc->idle_pc_enabled == enable)
		return;

	sde_enc->idle_pc_enabled = enable;

	SDE_DEBUG("idle-pc state:%d\n", sde_enc->idle_pc_enabled);
	SDE_EVT32(sde_enc->idle_pc_enabled);
}

static void _sde_encoder_rc_restart_delayed(struct sde_encoder_virt *sde_enc,
	u32 sw_event)
{
	struct drm_encoder *drm_enc = &sde_enc->base;
	struct msm_drm_private *priv;
	unsigned int lp, idle_pc_duration;
	struct msm_drm_thread *disp_thread;

	/* set idle timeout based on master connector's lp value */
	if (sde_enc->cur_master)
		lp = sde_connector_get_lp(
				sde_enc->cur_master->connector);
	else
		lp = SDE_MODE_DPMS_ON;

	if (lp == SDE_MODE_DPMS_LP2)
		idle_pc_duration = IDLE_SHORT_TIMEOUT;
	else
		idle_pc_duration = IDLE_POWERCOLLAPSE_DURATION;

	priv = drm_enc->dev->dev_private;
	disp_thread = &priv->disp_thread[sde_enc->crtc->index];

	kthread_mod_delayed_work(
			&disp_thread->worker,
			&sde_enc->delayed_off_work,
			msecs_to_jiffies(idle_pc_duration));
	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			idle_pc_duration, SDE_EVTLOG_FUNC_CASE2);
	SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work scheduled\n",
			sw_event);
}

static void _sde_encoder_rc_cancel_delayed(struct sde_encoder_virt *sde_enc,
	u32 sw_event)
{
	if (kthread_cancel_delayed_work_sync(&sde_enc->delayed_off_work))
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work cancelled\n",
				sw_event);
}

static void _sde_encoder_rc_kickoff_delayed(struct sde_encoder_virt *sde_enc,
	u32 sw_event)
{
	if (_sde_encoder_is_autorefresh_enabled(sde_enc))
		_sde_encoder_rc_cancel_delayed(sde_enc, sw_event);
	else
		_sde_encoder_rc_restart_delayed(sde_enc, sw_event);
}

static int _sde_encoder_rc_kickoff(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc, bool is_vid_mode)
{
	int ret = 0;

	mutex_lock(&sde_enc->rc_lock);

	/* return if the resource control is already in ON state */
	if (sde_enc->rc_state == SDE_ENC_RC_STATE_ON) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in ON state\n",
				sw_event);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_EVTLOG_FUNC_CASE1);
		goto end;
	} else if (sde_enc->rc_state != SDE_ENC_RC_STATE_OFF &&
			sde_enc->rc_state != SDE_ENC_RC_STATE_IDLE) {
		SDE_ERROR_ENC(sde_enc, "sw_event:%d, rc in state %d\n",
				sw_event, sde_enc->rc_state);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_ERROR);
		goto end;
	}

	if (is_vid_mode && sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
		sde_encoder_irq_control(drm_enc, true);
	} else {
		/* enable all the clks and resources */
		ret = _sde_encoder_resource_control_helper(drm_enc,
				true);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"sw_event:%d, rc in state %d\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event,
					sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			goto end;
		}
		_sde_encoder_update_rsc_client(drm_enc, true);
	}
	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE1);
	sde_enc->rc_state = SDE_ENC_RC_STATE_ON;

end:
	_sde_encoder_rc_kickoff_delayed(sde_enc, sw_event);

	mutex_unlock(&sde_enc->rc_lock);
	return ret;
}

static int _sde_encoder_rc_pre_stop(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc, bool is_vid_mode)
{
	/* cancel delayed off work, if any */
	_sde_encoder_rc_cancel_delayed(sde_enc, sw_event);

	mutex_lock(&sde_enc->rc_lock);

	if (is_vid_mode &&
		  sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
		sde_encoder_irq_control(drm_enc, true);
	}
	/* skip if is already OFF or IDLE, resources are off already */
	else if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF ||
			sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in %d state\n",
				sw_event, sde_enc->rc_state);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_EVTLOG_FUNC_CASE3);
		goto end;
	}

	/**
	 * IRQs are still enabled currently, which allows wait for
	 * VBLANK which RSC may require to correctly transition to OFF
	 */
	_sde_encoder_update_rsc_client(drm_enc, false);

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_PRE_OFF,
			SDE_EVTLOG_FUNC_CASE3);

	sde_enc->rc_state = SDE_ENC_RC_STATE_PRE_OFF;

end:
	mutex_unlock(&sde_enc->rc_lock);
	return 0;
}

static int _sde_encoder_rc_stop(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc)
{
	int ret = 0;

	mutex_lock(&sde_enc->rc_lock);
	/* return if the resource control is already in OFF state */
	if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in OFF state\n",
				sw_event);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_EVTLOG_FUNC_CASE4);
		goto end;
	} else if (sde_enc->rc_state == SDE_ENC_RC_STATE_ON ||
		   sde_enc->rc_state == SDE_ENC_RC_STATE_MODESET) {
		SDE_ERROR_ENC(sde_enc, "sw_event:%d, rc in state %d\n",
				sw_event, sde_enc->rc_state);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_ERROR);
		ret = -EINVAL;
		goto end;
	}

	/**
	 * expect to arrive here only if in either idle state or pre-off
	 * and in IDLE state the resources are already disabled
	 */
	if (sde_enc->rc_state == SDE_ENC_RC_STATE_PRE_OFF)
		_sde_encoder_resource_control_helper(drm_enc, false);

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_OFF, SDE_EVTLOG_FUNC_CASE4);

	sde_enc->rc_state = SDE_ENC_RC_STATE_OFF;

end:
	mutex_unlock(&sde_enc->rc_lock);
	return ret;
}

static int _sde_encoder_rc_pre_modeset(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc)
{
	int ret = 0;

	/* cancel delayed off work, if any */
	_sde_encoder_rc_cancel_delayed(sde_enc, sw_event);

	mutex_lock(&sde_enc->rc_lock);

	if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in OFF state\n",
				sw_event);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_EVTLOG_FUNC_CASE5);
		goto end;
	} else if (sde_enc->rc_state != SDE_ENC_RC_STATE_ON) {
		/* enable all the clks and resources */
		ret = _sde_encoder_resource_control_helper(drm_enc,
				true);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"sw_event:%d, rc in state %d\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event,
					sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			goto end;
		}

		_sde_encoder_update_rsc_client(drm_enc, true);

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE5);
		sde_enc->rc_state = SDE_ENC_RC_STATE_ON;
	}

	ret = sde_encoder_wait_for_event(drm_enc, MSM_ENC_TX_COMPLETE);
	if (ret && ret != -EWOULDBLOCK) {
		SDE_ERROR_ENC(sde_enc,
				"wait for commit done returned %d\n",
				ret);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				ret, SDE_EVTLOG_ERROR);
		ret = -EINVAL;
		goto end;
	}

	sde_encoder_irq_control(drm_enc, false);

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
		SDE_ENC_RC_STATE_MODESET, SDE_EVTLOG_FUNC_CASE5);

	sde_enc->rc_state = SDE_ENC_RC_STATE_MODESET;
	_sde_encoder_pm_qos_remove_request(drm_enc);

end:
	mutex_unlock(&sde_enc->rc_lock);
	return ret;
}

static int _sde_encoder_rc_post_modeset(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc)
{
	int ret = 0;

	mutex_lock(&sde_enc->rc_lock);

	if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in OFF state\n",
				sw_event);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_EVTLOG_FUNC_CASE5);
		goto end;
	} else if (sde_enc->rc_state != SDE_ENC_RC_STATE_MODESET) {
		SDE_ERROR_ENC(sde_enc,
				"sw_event:%d, rc:%d !MODESET state\n",
				sw_event, sde_enc->rc_state);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_ERROR);
		ret = -EINVAL;
		goto end;
	}

	sde_encoder_irq_control(drm_enc, true);

	_sde_encoder_update_rsc_client(drm_enc, true);

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE6);

	sde_enc->rc_state = SDE_ENC_RC_STATE_ON;
	_sde_encoder_pm_qos_add_request(drm_enc);

end:
	mutex_unlock(&sde_enc->rc_lock);
	return ret;
}

static int _sde_encoder_rc_idle(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc, bool is_vid_mode)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_crtc *crtc = drm_enc->crtc;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	mutex_lock(&sde_enc->rc_lock);

	if (sde_enc->rc_state != SDE_ENC_RC_STATE_ON) {
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc:%d !ON state\n",
				sw_event, sde_enc->rc_state);
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_ERROR);
		goto end;
	} else if (sde_crtc_frame_pending(sde_enc->crtc)) {
		SDE_DEBUG_ENC(sde_enc, "skip idle entry");
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			sde_crtc_frame_pending(sde_enc->crtc),
			SDE_EVTLOG_ERROR);
		_sde_encoder_rc_kickoff_delayed(sde_enc, sw_event);
		goto end;
	}

	if (is_vid_mode) {
		sde_encoder_irq_control(drm_enc, false);
	} else {
		/* disable all the clks and resources */
		_sde_encoder_update_rsc_client(drm_enc, false);
		_sde_encoder_resource_control_helper(drm_enc, false);

		if (!sde_kms->perf.bw_vote_mode)
			memset(&sde_crtc->cur_perf, 0,
				sizeof(struct sde_core_perf_params));
	}

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_IDLE, SDE_EVTLOG_FUNC_CASE7);
	sde_enc->rc_state = SDE_ENC_RC_STATE_IDLE;

end:
	mutex_unlock(&sde_enc->rc_lock);
	return 0;
}

static int _sde_encoder_rc_early_wakeup(struct drm_encoder *drm_enc,
	u32 sw_event, struct sde_encoder_virt *sde_enc,
	struct msm_drm_private *priv, bool is_vid_mode)
{
	bool autorefresh_enabled = false;
	struct msm_drm_thread *disp_thread;
	int ret = 0;

	if (!sde_enc->crtc ||
		sde_enc->crtc->index >= ARRAY_SIZE(priv->disp_thread)) {
		SDE_DEBUG_ENC(sde_enc,
			"invalid crtc:%d or crtc index:%d , sw_event:%u\n",
			sde_enc->crtc == NULL,
			sde_enc->crtc ? sde_enc->crtc->index : -EINVAL,
			sw_event);
		return -EINVAL;
	}

	disp_thread = &priv->disp_thread[sde_enc->crtc->index];

	mutex_lock(&sde_enc->rc_lock);

	if (sde_enc->rc_state == SDE_ENC_RC_STATE_ON) {
		if (sde_enc->cur_master &&
			sde_enc->cur_master->ops.is_autorefresh_enabled)
			autorefresh_enabled =
			sde_enc->cur_master->ops.is_autorefresh_enabled(
						sde_enc->cur_master);
		if (autorefresh_enabled) {
			SDE_DEBUG_ENC(sde_enc,
				"not handling early wakeup since auto refresh is enabled\n");
			goto end;
		}

		if (!sde_crtc_frame_pending(sde_enc->crtc))
			kthread_mod_delayed_work(&disp_thread->worker,
					&sde_enc->delayed_off_work,
					msecs_to_jiffies(
					IDLE_POWERCOLLAPSE_DURATION));
	} else if (sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
		/* enable all the clks and resources */
		ret = _sde_encoder_resource_control_helper(drm_enc,
				true);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"sw_event:%d, rc in state %d\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event,
					sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			goto end;
		}

		_sde_encoder_update_rsc_client(drm_enc, true);

		/*
		 * In some cases, commit comes with slight delay
		 * (> 80 ms)after early wake up, prevent clock switch
		 * off to avoid jank in next update. So, increase the
		 * command mode idle timeout sufficiently to prevent
		 * such case.
		 */
		kthread_mod_delayed_work(&disp_thread->worker,
				&sde_enc->delayed_off_work,
				msecs_to_jiffies(
				IDLE_POWERCOLLAPSE_IN_EARLY_WAKEUP));

		sde_enc->rc_state = SDE_ENC_RC_STATE_ON;
	}

	SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE8);

end:
	mutex_unlock(&sde_enc->rc_lock);
	return ret;
}

static int sde_encoder_resource_control(struct drm_encoder *drm_enc,
		u32 sw_event)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	int ret = 0;
	bool is_vid_mode = false;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid encoder parameters, sw_event:%u\n",
				sw_event);
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	if (sde_encoder_check_curr_mode(&sde_enc->base, MSM_DISPLAY_VIDEO_MODE))
		is_vid_mode = true;
	/*
	 * when idle_pc is not supported, process only KICKOFF, STOP and MODESET
	 * events and return early for other events (ie wb display).
	 */
	if (!sde_enc->idle_pc_enabled &&
			(sw_event != SDE_ENC_RC_EVENT_KICKOFF &&
			sw_event != SDE_ENC_RC_EVENT_PRE_MODESET &&
			sw_event != SDE_ENC_RC_EVENT_POST_MODESET &&
			sw_event != SDE_ENC_RC_EVENT_STOP &&
			sw_event != SDE_ENC_RC_EVENT_PRE_STOP))
		return 0;

	SDE_DEBUG_ENC(sde_enc, "sw_event:%d, idle_pc:%d\n",
			sw_event, sde_enc->idle_pc_enabled);
	SDE_EVT32_VERBOSE(DRMID(drm_enc), sw_event, sde_enc->idle_pc_enabled,
			sde_enc->rc_state, SDE_EVTLOG_FUNC_ENTRY);

	switch (sw_event) {
	case SDE_ENC_RC_EVENT_KICKOFF:
		ret = _sde_encoder_rc_kickoff(drm_enc, sw_event, sde_enc,
				is_vid_mode);
		break;
	case SDE_ENC_RC_EVENT_PRE_STOP:
		ret = _sde_encoder_rc_pre_stop(drm_enc, sw_event, sde_enc,
				is_vid_mode);
		break;
	case SDE_ENC_RC_EVENT_STOP:
		ret = _sde_encoder_rc_stop(drm_enc, sw_event, sde_enc);
		break;
	case SDE_ENC_RC_EVENT_PRE_MODESET:
		ret = _sde_encoder_rc_pre_modeset(drm_enc, sw_event, sde_enc);
		break;
	case SDE_ENC_RC_EVENT_POST_MODESET:
		ret = _sde_encoder_rc_post_modeset(drm_enc, sw_event, sde_enc);
		break;
	case SDE_ENC_RC_EVENT_ENTER_IDLE:
		ret = _sde_encoder_rc_idle(drm_enc, sw_event, sde_enc,
				is_vid_mode);
		break;
	case SDE_ENC_RC_EVENT_EARLY_WAKEUP:
		ret = _sde_encoder_rc_early_wakeup(drm_enc, sw_event, sde_enc,
				priv, is_vid_mode);
		break;
	default:
		SDE_EVT32(DRMID(drm_enc), sw_event, SDE_EVTLOG_ERROR);
		SDE_ERROR("unexpected sw_event: %d\n", sw_event);
		break;
	}

	SDE_EVT32_VERBOSE(DRMID(drm_enc), sw_event, sde_enc->idle_pc_enabled,
			sde_enc->rc_state, SDE_EVTLOG_FUNC_EXIT);
	return ret;
}

static void sde_encoder_virt_mode_switch(struct drm_encoder *drm_enc,
	enum sde_intf_mode intf_mode, struct drm_display_mode *adj_mode)
{
	int i = 0;
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	if (intf_mode == INTF_MODE_CMD)
		sde_enc->disp_info.curr_panel_mode = MSM_DISPLAY_VIDEO_MODE;
	else if (intf_mode == INTF_MODE_VIDEO)
		sde_enc->disp_info.curr_panel_mode = MSM_DISPLAY_CMD_MODE;

	_sde_encoder_update_rsc_client(drm_enc, true);

	if (intf_mode == INTF_MODE_CMD) {
		for (i = 0; i < sde_enc->num_phys_encs; i++)
			sde_enc->phys_encs[i] = sde_enc->phys_vid_encs[i];

		SDE_DEBUG_ENC(sde_enc, "switch to video physical encoder\n");
		SDE_EVT32(DRMID(&sde_enc->base), intf_mode, adj_mode->flags,
				adj_mode->private_flags, SDE_EVTLOG_FUNC_CASE1);
	} else if (intf_mode == INTF_MODE_VIDEO) {
		for (i = 0; i < sde_enc->num_phys_encs; i++)
			sde_enc->phys_encs[i] = sde_enc->phys_cmd_encs[i];

		SDE_DEBUG_ENC(sde_enc, "switch to command physical encoder\n");
		SDE_EVT32(DRMID(&sde_enc->base), intf_mode, adj_mode->flags,
				adj_mode->private_flags, SDE_EVTLOG_FUNC_CASE2);
	}
}

static struct drm_connector *_sde_encoder_get_connector(
		struct drm_device *dev, struct drm_encoder *drm_enc)
{
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *conn = NULL, *conn_search;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn_search, &conn_iter) {
		if (conn_search->encoder == drm_enc) {
			conn = conn_search;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return conn;
}

static void _sde_encoder_virt_populate_hw_res(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct sde_kms *sde_kms = sde_encoder_get_kms(drm_enc);
	struct sde_rm_hw_iter pp_iter, qdss_iter;
	struct sde_rm_hw_iter dsc_iter, vdc_iter;
	struct sde_rm_hw_request request_hw;
	int i, j;

	sde_rm_init_hw_iter(&pp_iter, drm_enc->base.id, SDE_HW_BLK_PINGPONG);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_pp[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &pp_iter))
			break;
		sde_enc->hw_pp[i] = (struct sde_hw_pingpong *) pp_iter.hw;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			sde_rm_init_hw_iter(&qdss_iter, drm_enc->base.id,
						SDE_HW_BLK_QDSS);
			for (j = 0; j < QDSS_MAX; j++) {
				if (sde_rm_get_hw(&sde_kms->rm, &qdss_iter)) {
					phys->hw_qdss =
					(struct sde_hw_qdss *)qdss_iter.hw;
					break;
				}
			}
		}
	}

	sde_rm_init_hw_iter(&dsc_iter, drm_enc->base.id, SDE_HW_BLK_DSC);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_dsc[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &dsc_iter))
			break;
		sde_enc->hw_dsc[i] = (struct sde_hw_dsc *) dsc_iter.hw;
	}

	sde_rm_init_hw_iter(&vdc_iter, drm_enc->base.id, SDE_HW_BLK_VDC);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_vdc[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &vdc_iter))
			break;
		sde_enc->hw_vdc[i] = (struct sde_hw_vdc *) vdc_iter.hw;
	}

	/* Get PP for DSC configuration */
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		struct sde_hw_pingpong *pp = NULL;
		unsigned long features = 0;

		if (!sde_enc->hw_dsc[i])
			continue;

		request_hw.id = sde_enc->hw_dsc[i]->base.id;
		request_hw.type = SDE_HW_BLK_PINGPONG;
		if (!sde_rm_request_hw_blk(&sde_kms->rm, &request_hw))
			break;
		pp = (struct sde_hw_pingpong *) request_hw.hw;
		features = pp->ops.get_hw_caps(pp);

		if (test_bit(SDE_PINGPONG_DSC, &features))
			sde_enc->hw_dsc_pp[i] = pp;
		else
			sde_enc->hw_dsc_pp[i] = NULL;
	}
}

static bool sde_encoder_detect_panel_mode_switch(
		struct drm_display_mode *adj_mode, enum sde_intf_mode intf_mode)
{
	/* don't rely on POMS flag as it may not be set for power-on modeset */
	if ((intf_mode == INTF_MODE_CMD &&
	     adj_mode->flags & DRM_MODE_FLAG_VID_MODE_PANEL) ||
	    (intf_mode == INTF_MODE_VIDEO &&
	     adj_mode->flags & DRM_MODE_FLAG_CMD_MODE_PANEL))
		return true;

	return false;
}

static int sde_encoder_virt_modeset_rc(struct drm_encoder *drm_enc,
		struct drm_display_mode *adj_mode, bool pre_modeset)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	enum sde_intf_mode intf_mode;
	int ret;
	bool is_cmd_mode = false;

	if (sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE))
		is_cmd_mode = true;

	if (pre_modeset) {
		intf_mode = sde_encoder_get_intf_mode(drm_enc);
		if (msm_is_mode_seamless_dms(adj_mode) ||
				(msm_is_mode_seamless_dyn_clk(adj_mode) &&
				 is_cmd_mode)) {
			/* restore resource state before releasing them */
			ret = sde_encoder_resource_control(drm_enc,
					SDE_ENC_RC_EVENT_PRE_MODESET);
			if (ret) {
				SDE_ERROR_ENC(sde_enc,
					"sde resource control failed: %d\n",
					ret);
				return ret;
			}

			/*
			 * Disable dce before switching the mode and after pre-
			 * modeset to guarantee previous kickoff has finished.
			 */
			sde_encoder_dce_disable(sde_enc);
		} else if (sde_encoder_detect_panel_mode_switch(adj_mode,
					intf_mode)) {
			_sde_encoder_modeset_helper_locked(drm_enc,
					SDE_ENC_RC_EVENT_PRE_MODESET);
			sde_encoder_virt_mode_switch(drm_enc, intf_mode,
					adj_mode);
		}
	} else {
		if (msm_is_mode_seamless_dms(adj_mode) ||
				(msm_is_mode_seamless_dyn_clk(adj_mode) &&
				is_cmd_mode))
			sde_encoder_resource_control(&sde_enc->base,
					SDE_ENC_RC_EVENT_POST_MODESET);
		else if (msm_is_mode_seamless_poms(adj_mode))
			_sde_encoder_modeset_helper_locked(drm_enc,
					SDE_ENC_RC_EVENT_POST_MODESET);
	}

	return 0;
}

static void sde_encoder_virt_mode_set(struct drm_encoder *drm_enc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_kms *sde_kms;
	struct drm_connector *conn;
	int i = 0, ret;
	int num_lm, num_intf, num_pp_per_intf;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (!sde_kms_power_resource_is_enabled(drm_enc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc));

	/*
	 * cache the crtc in sde_enc on enable for duration of use case
	 * for correctly servicing asynchronous irq events and timers
	 */
	if (!drm_enc->crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	sde_enc->crtc = drm_enc->crtc;
	sde_crtc_set_qos_dirty(drm_enc->crtc);

	/* get and store the mode_info */
	conn = _sde_encoder_get_connector(sde_kms->dev, drm_enc);
	if (!conn) {
		SDE_ERROR_ENC(sde_enc, "failed to find attached connector\n");
		return;
	} else if (!conn->state) {
		SDE_ERROR_ENC(sde_enc, "invalid connector state\n");
		return;
	}

	sde_connector_state_get_mode_info(conn->state, &sde_enc->mode_info);
	sde_encoder_dce_set_bpp(sde_enc->mode_info, sde_enc->crtc);

	/* release resources before seamless mode change */
	ret = sde_encoder_virt_modeset_rc(drm_enc, adj_mode, true);
	if (ret)
		return;

	/* reserve dynamic resources now, indicating non test-only */
	ret = sde_rm_reserve(&sde_kms->rm, drm_enc, drm_enc->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
				"failed to reserve hw resources, %d\n", ret);
		return;
	}

	/* assign the reserved HW blocks to this encoder */
	_sde_encoder_virt_populate_hw_res(drm_enc);

	/* determine left HW PP block to map to INTF */
	num_lm = sde_enc->mode_info.topology.num_lm;
	num_intf = sde_enc->mode_info.topology.num_intf;
	num_pp_per_intf = num_lm / num_intf;
	if (!num_pp_per_intf)
		num_pp_per_intf = 1;

	/* perform mode_set on phys_encs */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			if (!sde_enc->hw_pp[i * num_pp_per_intf] &&
				sde_enc->topology.num_intf) {
				SDE_ERROR_ENC(sde_enc, "invalid hw_pp[%d]\n",
						i * num_pp_per_intf);
				return;
			}
			phys->hw_pp = sde_enc->hw_pp[i * num_pp_per_intf];
			phys->connector = conn->state->connector;
			if (phys->ops.mode_set)
				phys->ops.mode_set(phys, mode, adj_mode);
		}
	}

	/* update resources after seamless mode change */
	sde_encoder_virt_modeset_rc(drm_enc, adj_mode, false);
}

void sde_encoder_control_te(struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.control_te)
			phys->ops.control_te(phys, enable);
	}
}

static int _sde_encoder_input_connect(struct input_handler *handler,
	struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int rc = 0;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = handler->name;

	rc = input_register_handle(handle);
	if (rc) {
		pr_err("failed to register input handle\n");
		goto error;
	}

	rc = input_open_device(handle);
	if (rc) {
		pr_err("failed to open input device\n");
		goto error_unregister;
	}

	return 0;

error_unregister:
	input_unregister_handle(handle);

error:
	kfree(handle);

	return rc;
}

static void _sde_encoder_input_disconnect(struct input_handle *handle)
{
	 input_close_device(handle);
	 input_unregister_handle(handle);
	 kfree(handle);
}

/**
 * Structure for specifying event parameters on which to receive callbacks.
 * This structure will trigger a callback in case of a touch event (specified by
 * EV_ABS) where there is a change in X and Y coordinates,
 */
static const struct input_device_id sde_input_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
					BIT_MASK(ABS_MT_POSITION_X) |
					BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ },
};

static void _sde_encoder_input_handler_register(
		struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	int rc;

	if (!sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE) ||
		!sde_enc->input_event_enabled)
		return;

	if (sde_enc->input_handler && !sde_enc->input_handler->private) {
		sde_enc->input_handler->private = sde_enc;

		/* register input handler if not already registered */
		rc = input_register_handler(sde_enc->input_handler);
		if (rc) {
			SDE_ERROR("input_handler_register failed, rc= %d\n",
						 rc);
			kfree(sde_enc->input_handler);
		}
	}
}

static void _sde_encoder_input_handler_unregister(
		struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	if (!sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE) ||
		!sde_enc->input_event_enabled)
		return;

	if (sde_enc->input_handler && sde_enc->input_handler->private) {
		input_unregister_handler(sde_enc->input_handler);
		sde_enc->input_handler->private = NULL;
	}

}

static int _sde_encoder_input_handler(
		struct sde_encoder_virt *sde_enc)
{
	struct input_handler *input_handler = NULL;
	int rc = 0;

	if (sde_enc->input_handler) {
		SDE_ERROR_ENC(sde_enc,
				"input_handle is active. unexpected\n");
		return -EINVAL;
	}

	input_handler = kzalloc(sizeof(*sde_enc->input_handler), GFP_KERNEL);
	if (!input_handler)
		return -ENOMEM;

	input_handler->event = sde_encoder_input_event_handler;
	input_handler->connect = _sde_encoder_input_connect;
	input_handler->disconnect = _sde_encoder_input_disconnect;
	input_handler->name = "sde";
	input_handler->id_table = sde_input_ids;

	sde_enc->input_handler = input_handler;

	return rc;
}

static void _sde_encoder_virt_enable_helper(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_kms *sde_kms;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc || !sde_enc->cur_master) {
		SDE_DEBUG("invalid sde encoder/master\n");
		return;
	}

	if (sde_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DisplayPort &&
	    sde_enc->cur_master->hw_mdptop &&
	    sde_enc->cur_master->hw_mdptop->ops.intf_audio_select)
		sde_enc->cur_master->hw_mdptop->ops.intf_audio_select(
					sde_enc->cur_master->hw_mdptop);

	if (sde_enc->cur_master->hw_mdptop &&
			sde_enc->cur_master->hw_mdptop->ops.reset_ubwc)
		sde_enc->cur_master->hw_mdptop->ops.reset_ubwc(
				sde_enc->cur_master->hw_mdptop,
				sde_kms->catalog);

	if (sde_enc->cur_master->hw_ctl &&
			sde_enc->cur_master->hw_ctl->ops.setup_intf_cfg_v1 &&
			!sde_enc->cur_master->cont_splash_enabled)
		sde_enc->cur_master->hw_ctl->ops.setup_intf_cfg_v1(
				sde_enc->cur_master->hw_ctl,
				&sde_enc->cur_master->intf_cfg_v1);

	_sde_encoder_update_vsync_source(sde_enc, &sde_enc->disp_info, false);
	sde_encoder_control_te(drm_enc, true);

	memset(&sde_enc->prv_conn_roi, 0, sizeof(sde_enc->prv_conn_roi));
	memset(&sde_enc->cur_conn_roi, 0, sizeof(sde_enc->cur_conn_roi));
}

static void _sde_encoder_setup_dither(struct sde_encoder_phys *phys)
{
	struct sde_kms *sde_kms;
	void *dither_cfg = NULL;
	int ret = 0, i = 0;
	size_t len = 0;
	enum sde_rm_topology_name topology;
	struct drm_encoder *drm_enc;
	struct msm_display_dsc_info *dsc = NULL;
	struct sde_encoder_virt *sde_enc;
	struct sde_hw_pingpong *hw_pp;
	u32 bpp, bpc;
	int num_lm;

	if (!phys || !phys->connector || !phys->hw_pp ||
			!phys->hw_pp->ops.setup_dither || !phys->parent)
		return;

	sde_kms = sde_encoder_get_kms(phys->parent);
	if (!sde_kms)
		return;

	topology = sde_connector_get_topology_name(phys->connector);
	if ((topology == SDE_RM_TOPOLOGY_NONE) ||
			((topology == SDE_RM_TOPOLOGY_PPSPLIT) &&
			(phys->split_role == ENC_ROLE_SLAVE)))
		return;

	drm_enc = phys->parent;
	sde_enc = to_sde_encoder_virt(drm_enc);
	dsc = &sde_enc->mode_info.comp_info.dsc_info;

	bpc = dsc->config.bits_per_component;
	bpp = dsc->config.bits_per_pixel;

	/* disable dither for 10 bpp or 10bpc dsc config */
	if (bpp == 10 || bpc == 10) {
		phys->hw_pp->ops.setup_dither(phys->hw_pp, NULL, 0);
		return;
	}

	ret = sde_connector_get_dither_cfg(phys->connector,
			phys->connector->state, &dither_cfg,
			&len, sde_enc->idle_pc_restore);

	/* skip reg writes when return values are invalid or no data */
	if (ret && ret == -ENODATA)
		return;

	num_lm = sde_rm_topology_get_num_lm(&sde_kms->rm, topology);
	for (i = 0; i < num_lm; i++) {
		hw_pp = sde_enc->hw_pp[i];
		phys->hw_pp->ops.setup_dither(hw_pp,
				dither_cfg, len);
	}
}

void sde_encoder_virt_restore(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	if (!sde_enc->cur_master) {
		SDE_DEBUG("virt encoder has no master\n");
		return;
	}

	memset(&sde_enc->cur_master->intf_cfg_v1, 0,
			sizeof(sde_enc->cur_master->intf_cfg_v1));
	sde_enc->idle_pc_restore = true;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys)
			continue;

		if (phys->hw_ctl && phys->hw_ctl->ops.clear_pending_flush)
			phys->hw_ctl->ops.clear_pending_flush(phys->hw_ctl);

		if ((phys != sde_enc->cur_master) && phys->ops.restore)
			phys->ops.restore(phys);

		_sde_encoder_setup_dither(phys);
	}

	if (sde_enc->cur_master->ops.restore)
		sde_enc->cur_master->ops.restore(sde_enc->cur_master);

	_sde_encoder_virt_enable_helper(drm_enc);
}

static void sde_encoder_off_work(struct kthread_work *work)
{
	struct sde_encoder_virt *sde_enc = container_of(work,
			struct sde_encoder_virt, delayed_off_work.work);
	struct drm_encoder *drm_enc;

	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}
	drm_enc = &sde_enc->base;

	SDE_ATRACE_BEGIN("sde_encoder_off_work");
	sde_encoder_idle_request(drm_enc);
	SDE_ATRACE_END("sde_encoder_off_work");
}

static void sde_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;
	struct msm_compression_info *comp_info = NULL;
	struct drm_display_mode *cur_mode = NULL;
	struct msm_display_info *disp_info;

	if (!drm_enc || !drm_enc->crtc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;

	if (!sde_kms_power_resource_is_enabled(drm_enc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	if (!sde_enc->crtc)
		sde_enc->crtc = drm_enc->crtc;

	comp_info = &sde_enc->mode_info.comp_info;
	cur_mode = &sde_enc->base.crtc->state->adjusted_mode;

	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc), cur_mode->hdisplay, cur_mode->vdisplay);

	sde_enc->cur_master = NULL;
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.is_master && phys->ops.is_master(phys)) {
			SDE_DEBUG_ENC(sde_enc, "master is now idx %d\n", i);
			sde_enc->cur_master = phys;
			break;
		}
	}

	if (!sde_enc->cur_master) {
		SDE_ERROR("virt encoder has no master! num_phys %d\n", i);
		return;
	}

	_sde_encoder_input_handler_register(drm_enc);

	if ((drm_enc->crtc->state->connectors_changed &&
			sde_encoder_in_clone_mode(drm_enc)) ||
			!(msm_is_mode_seamless_vrr(cur_mode)
			|| msm_is_mode_seamless_dms(cur_mode)
			|| msm_is_mode_seamless_dyn_clk(cur_mode)))
		kthread_init_delayed_work(&sde_enc->delayed_off_work,
			sde_encoder_off_work);

	ret = sde_encoder_resource_control(drm_enc, SDE_ENC_RC_EVENT_KICKOFF);
	if (ret) {
		SDE_ERROR_ENC(sde_enc, "sde resource control failed: %d\n",
				ret);
		return;
	}

	memset(&sde_enc->cur_master->intf_cfg_v1, 0,
			sizeof(sde_enc->cur_master->intf_cfg_v1));

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys)
			continue;

		phys->comp_type = comp_info->comp_type;
		phys->comp_ratio = comp_info->comp_ratio;
		phys->frame_trigger_mode = sde_enc->frame_trigger_mode;
		phys->poms_align_vsync = disp_info->poms_align_vsync;
		if (phys->comp_type == MSM_DISPLAY_COMPRESSION_DSC) {
			phys->dsc_extra_pclk_cycle_cnt =
				comp_info->dsc_info.pclk_per_line;
			phys->dsc_extra_disp_width =
				comp_info->dsc_info.extra_width;
			phys->dce_bytes_per_line =
				comp_info->dsc_info.bytes_per_pkt *
				comp_info->dsc_info.pkt_per_line;
		} else if (phys->comp_type == MSM_DISPLAY_COMPRESSION_VDC) {
			phys->dce_bytes_per_line =
				comp_info->vdc_info.bytes_per_pkt *
				comp_info->vdc_info.pkt_per_line;
		}

		if (phys != sde_enc->cur_master) {
			/**
			 * on DMS request, the encoder will be enabled
			 * already. Invoke restore to reconfigure the
			 * new mode.
			 */
			if ((msm_is_mode_seamless_dms(cur_mode) ||
				msm_is_mode_seamless_dyn_clk(cur_mode)) &&
					phys->ops.restore)
				phys->ops.restore(phys);
			else if (phys->ops.enable)
				phys->ops.enable(phys);
		}

		if (sde_enc->misr_enable  && phys->ops.setup_misr &&
		(sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_VIDEO_MODE)))
			phys->ops.setup_misr(phys, true,
						sde_enc->misr_frame_count);
	}

	if ((msm_is_mode_seamless_dms(cur_mode) ||
			msm_is_mode_seamless_dyn_clk(cur_mode)) &&
			sde_enc->cur_master->ops.restore)
		sde_enc->cur_master->ops.restore(sde_enc->cur_master);
	else if (sde_enc->cur_master->ops.enable)
		sde_enc->cur_master->ops.enable(sde_enc->cur_master);

	_sde_encoder_virt_enable_helper(drm_enc);
}

static void sde_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_kms *sde_kms;
	enum sde_intf_mode intf_mode;
	int i = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	} else if (!drm_enc->dev) {
		SDE_ERROR("invalid dev\n");
		return;
	} else if (!drm_enc->dev->dev_private) {
		SDE_ERROR("invalid dev_private\n");
		return;
	}

	if (!sde_kms_power_resource_is_enabled(drm_enc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	sde_kms = sde_encoder_get_kms(&sde_enc->base);
	if (!sde_kms)
		return;

	intf_mode = sde_encoder_get_intf_mode(drm_enc);

	SDE_EVT32(DRMID(drm_enc));

	/* wait for idle */
	sde_encoder_wait_for_event(drm_enc, MSM_ENC_TX_COMPLETE);

	_sde_encoder_input_handler_unregister(drm_enc);

	/*
	 * For primary command mode and video mode encoders, execute the
	 * resource control pre-stop operations before the physical encoders
	 * are disabled, to allow the rsc to transition its states properly.
	 *
	 * For other encoder types, rsc should not be enabled until after
	 * they have been fully disabled, so delay the pre-stop operations
	 * until after the physical disable calls have returned.
	 */
	if (sde_enc->disp_info.display_type == SDE_CONNECTOR_PRIMARY &&
	    (intf_mode == INTF_MODE_CMD || intf_mode == INTF_MODE_VIDEO)) {
		sde_encoder_resource_control(drm_enc,
				SDE_ENC_RC_EVENT_PRE_STOP);
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

			if (phys && phys->ops.disable)
				phys->ops.disable(phys);
		}
	} else {
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

			if (phys && phys->ops.disable)
				phys->ops.disable(phys);
		}
		sde_encoder_resource_control(drm_enc,
				SDE_ENC_RC_EVENT_PRE_STOP);
	}

	/*
	 * disable dce after the transfer is complete (for command mode)
	 * and after physical encoder is disabled, to make sure timing
	 * engine is already disabled (for video mode).
	 */
	if (!sde_in_trusted_vm(sde_kms))
		sde_encoder_dce_disable(sde_enc);

	sde_encoder_resource_control(drm_enc, SDE_ENC_RC_EVENT_STOP);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		if (sde_enc->phys_encs[i]) {
			sde_enc->phys_encs[i]->cont_splash_enabled = false;
			sde_enc->phys_encs[i]->connector = NULL;
		}
		atomic_set(&sde_enc->frame_done_cnt[i], 0);
	}

	sde_enc->cur_master = NULL;
	/*
	 * clear the cached crtc in sde_enc on use case finish, after all the
	 * outstanding events and timers have been completed
	 */
	sde_enc->crtc = NULL;
	memset(&sde_enc->mode_info, 0, sizeof(sde_enc->mode_info));

	SDE_DEBUG_ENC(sde_enc, "encoder disabled\n");

	sde_rm_release(&sde_kms->rm, drm_enc, false);
}

void sde_encoder_helper_phys_disable(struct sde_encoder_phys *phys_enc,
		struct sde_encoder_phys_wb *wb_enc)
{
	struct sde_encoder_virt *sde_enc;

	phys_enc->hw_ctl->ops.reset(phys_enc->hw_ctl);
	sde_encoder_helper_reset_mixers(phys_enc, NULL);

	if (wb_enc) {
		if (wb_enc->hw_wb->ops.bind_pingpong_blk) {
			wb_enc->hw_wb->ops.bind_pingpong_blk(wb_enc->hw_wb,
					false, phys_enc->hw_pp->idx);

			if (phys_enc->hw_ctl->ops.update_bitmask)
				phys_enc->hw_ctl->ops.update_bitmask(
						phys_enc->hw_ctl,
						SDE_HW_FLUSH_WB,
						wb_enc->hw_wb->idx, true);
		}
	} else {
		if (phys_enc->hw_intf->ops.bind_pingpong_blk) {
			phys_enc->hw_intf->ops.bind_pingpong_blk(
					phys_enc->hw_intf, false,
					phys_enc->hw_pp->idx);

			if (phys_enc->hw_ctl->ops.update_bitmask)
				phys_enc->hw_ctl->ops.update_bitmask(
						phys_enc->hw_ctl,
						SDE_HW_FLUSH_INTF,
						phys_enc->hw_intf->idx, true);
		}
	}

	if (phys_enc->hw_pp && phys_enc->hw_pp->ops.reset_3d_mode) {
		phys_enc->hw_pp->ops.reset_3d_mode(phys_enc->hw_pp);

		if (phys_enc->hw_ctl->ops.update_bitmask &&
				phys_enc->hw_pp->merge_3d)
			phys_enc->hw_ctl->ops.update_bitmask(
					phys_enc->hw_ctl, SDE_HW_FLUSH_MERGE_3D,
					phys_enc->hw_pp->merge_3d->idx, true);
	}

	if (phys_enc->hw_cdm && phys_enc->hw_cdm->ops.bind_pingpong_blk &&
			phys_enc->hw_pp) {
		phys_enc->hw_cdm->ops.bind_pingpong_blk(phys_enc->hw_cdm,
				false, phys_enc->hw_pp->idx);

		if (phys_enc->hw_ctl->ops.update_bitmask)
			phys_enc->hw_ctl->ops.update_bitmask(
					phys_enc->hw_ctl, SDE_HW_FLUSH_CDM,
					phys_enc->hw_cdm->idx, true);
	}

	sde_enc = to_sde_encoder_virt(phys_enc->parent);

	if (phys_enc == sde_enc->cur_master && phys_enc->hw_pp &&
			phys_enc->hw_ctl->ops.reset_post_disable)
		phys_enc->hw_ctl->ops.reset_post_disable(
				phys_enc->hw_ctl, &phys_enc->intf_cfg_v1,
				phys_enc->hw_pp->merge_3d ?
				phys_enc->hw_pp->merge_3d->idx : 0);

	phys_enc->hw_ctl->ops.trigger_flush(phys_enc->hw_ctl);
	phys_enc->hw_ctl->ops.trigger_start(phys_enc->hw_ctl);
}

static enum sde_intf sde_encoder_get_intf(struct sde_mdss_cfg *catalog,
		enum sde_intf_type type, u32 controller_id)
{
	int i = 0;

	for (i = 0; i < catalog->intf_count; i++) {
		if (catalog->intf[i].type == type
		    && catalog->intf[i].controller_id == controller_id) {
			return catalog->intf[i].id;
		}
	}

	return INTF_MAX;
}

static enum sde_wb sde_encoder_get_wb(struct sde_mdss_cfg *catalog,
		enum sde_intf_type type, u32 controller_id)
{
	if (controller_id < catalog->wb_count)
		return catalog->wb[controller_id].id;

	return WB_MAX;
}

void sde_encoder_perf_uidle_status(struct sde_kms *sde_kms,
	struct drm_crtc *crtc)
{
	struct sde_hw_uidle *uidle;
	struct sde_uidle_cntr cntr;
	struct sde_uidle_status status;

	if (!sde_kms || !crtc || !sde_kms->hw_uidle) {
		pr_err("invalid params %d %d\n",
			!sde_kms, !crtc);
		return;
	}

	/* check if perf counters are enabled and setup */
	if (!sde_kms->catalog->uidle_cfg.perf_cntr_en)
		return;

	uidle = sde_kms->hw_uidle;
	if ((sde_kms->catalog->uidle_cfg.debugfs_perf & SDE_PERF_UIDLE_STATUS)
			&& uidle->ops.uidle_get_status) {

		uidle->ops.uidle_get_status(uidle, &status);
		trace_sde_perf_uidle_status(
			crtc->base.id,
			status.uidle_danger_status_0,
			status.uidle_danger_status_1,
			status.uidle_safe_status_0,
			status.uidle_safe_status_1,
			status.uidle_idle_status_0,
			status.uidle_idle_status_1,
			status.uidle_fal_status_0,
			status.uidle_fal_status_1,
			status.uidle_status,
			status.uidle_en_fal10);
	}

	if ((sde_kms->catalog->uidle_cfg.debugfs_perf & SDE_PERF_UIDLE_CNT)
			&& uidle->ops.uidle_get_cntr) {

		uidle->ops.uidle_get_cntr(uidle, &cntr);
		trace_sde_perf_uidle_cntr(
			crtc->base.id,
			cntr.fal1_gate_cntr,
			cntr.fal10_gate_cntr,
			cntr.fal_wait_gate_cntr,
			cntr.fal1_num_transitions_cntr,
			cntr.fal10_num_transitions_cntr,
			cntr.min_gate_cntr,
			cntr.max_gate_cntr);
	}
}

static void sde_encoder_vblank_callback(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phy_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	unsigned long lock_flags;

	if (!drm_enc || !phy_enc)
		return;

	SDE_ATRACE_BEGIN("encoder_vblank_callback");
	sde_enc = to_sde_encoder_virt(drm_enc);

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	if (sde_enc->crtc_vblank_cb)
		sde_enc->crtc_vblank_cb(sde_enc->crtc_vblank_cb_data);
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);

	if (phy_enc->sde_kms &&
			phy_enc->sde_kms->catalog->uidle_cfg.debugfs_perf)
		sde_encoder_perf_uidle_status(phy_enc->sde_kms, sde_enc->crtc);

	atomic_inc(&phy_enc->vsync_cnt);
	SDE_ATRACE_END("encoder_vblank_callback");
}

static void sde_encoder_underrun_callback(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phy_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	if (!phy_enc)
		return;

	SDE_ATRACE_BEGIN("encoder_underrun_callback");
	atomic_inc(&phy_enc->underrun_cnt);
	SDE_EVT32(DRMID(drm_enc), atomic_read(&phy_enc->underrun_cnt));
	if (sde_enc->cur_master &&
			sde_enc->cur_master->ops.get_underrun_line_count)
		sde_enc->cur_master->ops.get_underrun_line_count(
				sde_enc->cur_master);

	trace_sde_encoder_underrun(DRMID(drm_enc),
		atomic_read(&phy_enc->underrun_cnt));

	SDE_DBG_CTRL("stop_ftrace");
	SDE_DBG_CTRL("panic_underrun");

	SDE_ATRACE_END("encoder_underrun_callback");
}

void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*vbl_cb)(void *), void *vbl_data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;
	int i;

	enable = vbl_cb ? true : false;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc), enable);

	if (sde_encoder_in_clone_mode(drm_enc)) {
		SDE_EVT32(DRMID(drm_enc), SDE_EVTLOG_ERROR);
		return;
	}

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	sde_enc->crtc_vblank_cb = vbl_cb;
	sde_enc->crtc_vblank_cb_data = vbl_data;
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.control_vblank_irq)
			phys->ops.control_vblank_irq(phys, enable);
	}
	sde_enc->vblank_enabled = enable;
}

void sde_encoder_register_frame_event_callback(struct drm_encoder *drm_enc,
			void (*frame_event_cb)(void *, u32 event),
			struct drm_crtc *crtc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;

	enable = frame_event_cb ? true : false;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc), enable, 0);

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	sde_enc->crtc_frame_event_cb = frame_event_cb;
	sde_enc->crtc_frame_event_cb_data.crtc = crtc;
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);
}

static void sde_encoder_frame_done_callback(
		struct drm_encoder *drm_enc,
		struct sde_encoder_phys *ready_phys, u32 event)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned int i;
	bool trigger = true;
	bool is_cmd_mode = false;
	enum sde_rm_topology_name topology = SDE_RM_TOPOLOGY_NONE;

	if (!drm_enc || !sde_enc->cur_master) {
		SDE_ERROR("invalid param: drm_enc %pK, cur_master %pK\n",
				drm_enc, drm_enc ? sde_enc->cur_master : 0);
		return;
	}

	sde_enc->crtc_frame_event_cb_data.connector =
				sde_enc->cur_master->connector;
	if (sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE))
		is_cmd_mode = true;

	if (event & (SDE_ENCODER_FRAME_EVENT_DONE
			| SDE_ENCODER_FRAME_EVENT_ERROR
			| SDE_ENCODER_FRAME_EVENT_PANEL_DEAD) && is_cmd_mode) {

		if (ready_phys->connector)
			topology = sde_connector_get_topology_name(
							ready_phys->connector);

		/* One of the physical encoders has become idle */
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			if (sde_enc->phys_encs[i] == ready_phys) {
				SDE_EVT32_VERBOSE(DRMID(drm_enc), i,
				     atomic_read(&sde_enc->frame_done_cnt[i]));
				if (!atomic_add_unless(
					&sde_enc->frame_done_cnt[i], 1, 2)) {
					SDE_EVT32(DRMID(drm_enc), event,
						ready_phys->intf_idx,
						SDE_EVTLOG_ERROR);
					SDE_ERROR_ENC(sde_enc,
						"intf idx:%d, event:%d\n",
						ready_phys->intf_idx, event);
					return;
				}
			}

			if (topology != SDE_RM_TOPOLOGY_PPSPLIT &&
			    atomic_read(&sde_enc->frame_done_cnt[i]) == 0)
				trigger = false;
		}

		if (trigger) {
			if (sde_enc->crtc_frame_event_cb)
				sde_enc->crtc_frame_event_cb(
					&sde_enc->crtc_frame_event_cb_data,
					event);
			for (i = 0; i < sde_enc->num_phys_encs; i++)
				atomic_add_unless(&sde_enc->frame_done_cnt[i],
						-1, 0);
		}
	} else if (sde_enc->crtc_frame_event_cb) {
		sde_enc->crtc_frame_event_cb(
				&sde_enc->crtc_frame_event_cb_data, event);
	}
}

static void sde_encoder_get_qsync_fps_callback(
	struct drm_encoder *drm_enc,
	u32 *qsync_fps)
{
	struct msm_display_info *disp_info;
	struct sde_encoder_virt *sde_enc;

	if (!qsync_fps)
		return;

	*qsync_fps = 0;
	if (!drm_enc) {
		SDE_ERROR("invalid drm encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;
	*qsync_fps = disp_info->qsync_min_fps;
}

int sde_encoder_idle_request(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc) {
		SDE_ERROR("invalid drm encoder\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	sde_encoder_resource_control(&sde_enc->base,
						SDE_ENC_RC_EVENT_ENTER_IDLE);

	return 0;
}

/**
 * _sde_encoder_trigger_flush - trigger flush for a physical encoder
 * drm_enc: Pointer to drm encoder structure
 * phys: Pointer to physical encoder structure
 * extra_flush: Additional bit mask to include in flush trigger
 * config_changed: if true new config is applied, avoid increment of retire
 *	count if false
 */
static inline void _sde_encoder_trigger_flush(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phys,
		struct sde_ctl_flush_cfg *extra_flush,
		bool config_changed)
{
	struct sde_hw_ctl *ctl;
	unsigned long lock_flags;
	struct sde_encoder_virt *sde_enc;
	int pend_ret_fence_cnt;
	struct sde_connector *c_conn;

	if (!drm_enc || !phys) {
		SDE_ERROR("invalid argument(s), drm_enc %d, phys_enc %d\n",
				!drm_enc, !phys);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	c_conn = to_sde_connector(phys->connector);

	if (!phys->hw_pp) {
		SDE_ERROR("invalid pingpong hw\n");
		return;
	}

	ctl = phys->hw_ctl;
	if (!ctl || !phys->ops.trigger_flush) {
		SDE_ERROR("missing ctl/trigger cb\n");
		return;
	}

	if (phys->split_role == ENC_ROLE_SKIP) {
		SDE_DEBUG_ENC(to_sde_encoder_virt(phys->parent),
				"skip flush pp%d ctl%d\n",
				phys->hw_pp->idx - PINGPONG_0,
				ctl->idx - CTL_0);
		return;
	}

	/* update pending counts and trigger kickoff ctl flush atomically */
	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);

	if (phys->ops.is_master && phys->ops.is_master(phys) && config_changed)
		atomic_inc(&phys->pending_retire_fence_cnt);

	pend_ret_fence_cnt = atomic_read(&phys->pending_retire_fence_cnt);

	if (phys->hw_intf && phys->hw_intf->cap->type == INTF_DP &&
			ctl->ops.update_bitmask) {
		/* perform peripheral flush on every frame update for dp dsc */
		if (phys->comp_type == MSM_DISPLAY_COMPRESSION_DSC &&
				phys->comp_ratio && c_conn->ops.update_pps) {
			c_conn->ops.update_pps(phys->connector, NULL,
					c_conn->display);
			ctl->ops.update_bitmask(ctl, SDE_HW_FLUSH_PERIPH,
					phys->hw_intf->idx, 1);
		}

		if (sde_enc->dynamic_hdr_updated)
			ctl->ops.update_bitmask(ctl, SDE_HW_FLUSH_PERIPH,
					phys->hw_intf->idx, 1);
	}

	if ((extra_flush && extra_flush->pending_flush_mask)
			&& ctl->ops.update_pending_flush)
		ctl->ops.update_pending_flush(ctl, extra_flush);

	phys->ops.trigger_flush(phys);

	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);

	if (ctl->ops.get_pending_flush) {
		struct sde_ctl_flush_cfg pending_flush = {0,};

		ctl->ops.get_pending_flush(ctl, &pending_flush);
		SDE_EVT32(DRMID(drm_enc), phys->intf_idx - INTF_0,
				ctl->idx - CTL_0,
				pending_flush.pending_flush_mask,
				pend_ret_fence_cnt);
	} else {
		SDE_EVT32(DRMID(drm_enc), phys->intf_idx - INTF_0,
				ctl->idx - CTL_0,
				pend_ret_fence_cnt);
	}
}

/**
 * _sde_encoder_trigger_start - trigger start for a physical encoder
 * phys: Pointer to physical encoder structure
 */
static inline void _sde_encoder_trigger_start(struct sde_encoder_phys *phys)
{
	struct sde_hw_ctl *ctl;
	struct sde_encoder_virt *sde_enc;

	if (!phys) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

	if (!phys->hw_pp) {
		SDE_ERROR("invalid pingpong hw\n");
		return;
	}

	if (!phys->parent) {
		SDE_ERROR("invalid parent\n");
		return;
	}
	/* avoid ctrl start for encoder in clone mode */
	if (phys->in_clone_mode)
		return;

	ctl = phys->hw_ctl;
	sde_enc = to_sde_encoder_virt(phys->parent);

	if (phys->split_role == ENC_ROLE_SKIP) {
		SDE_DEBUG_ENC(sde_enc,
				"skip start pp%d ctl%d\n",
				phys->hw_pp->idx - PINGPONG_0,
				ctl->idx - CTL_0);
		return;
	}

	if (phys->ops.trigger_start && phys->enable_state != SDE_ENC_DISABLED)
		phys->ops.trigger_start(phys);
}

void sde_encoder_helper_trigger_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_ctl *ctl;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	ctl = phys_enc->hw_ctl;
	if (ctl && ctl->ops.trigger_flush)
		ctl->ops.trigger_flush(ctl);
}

void sde_encoder_helper_trigger_start(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_ctl *ctl;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	ctl = phys_enc->hw_ctl;
	if (ctl && ctl->ops.trigger_start) {
		ctl->ops.trigger_start(ctl);
		SDE_EVT32(DRMID(phys_enc->parent), ctl->idx - CTL_0);
	}
}

void sde_encoder_helper_hw_reset(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_connector *sde_con;
	void *sde_con_disp;
	struct sde_hw_ctl *ctl;
	int rc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	ctl = phys_enc->hw_ctl;

	if (!ctl || !ctl->ops.reset)
		return;

	SDE_DEBUG_ENC(sde_enc, "ctl %d reset\n",  ctl->idx);
	SDE_EVT32(DRMID(phys_enc->parent), ctl->idx);

	if (phys_enc->ops.is_master && phys_enc->ops.is_master(phys_enc) &&
			phys_enc->connector) {
		sde_con = to_sde_connector(phys_enc->connector);
		sde_con_disp = sde_connector_get_display(phys_enc->connector);

		if (sde_con->ops.soft_reset) {
			rc = sde_con->ops.soft_reset(sde_con_disp);
			if (rc) {
				SDE_ERROR_ENC(sde_enc,
						"connector soft reset failure\n");
				SDE_DBG_DUMP("all", "dbg_bus", "vbif_dbg_bus",
								"panic");
			}
		}
	}

	phys_enc->enable_state = SDE_ENC_ENABLED;
}

/**
 * _sde_encoder_kickoff_phys - handle physical encoder kickoff
 *	Iterate through the physical encoders and perform consolidated flush
 *	and/or control start triggering as needed. This is done in the virtual
 *	encoder rather than the individual physical ones in order to handle
 *	use cases that require visibility into multiple physical encoders at
 *	a time.
 * sde_enc: Pointer to virtual encoder structure
 * config_changed: if true new config is applied. Avoid regdma_flush and
 *	incrementing the retire count if false.
 */
static void _sde_encoder_kickoff_phys(struct sde_encoder_virt *sde_enc,
		bool config_changed)
{
	struct sde_hw_ctl *ctl;
	uint32_t i;
	struct sde_ctl_flush_cfg pending_flush = {0,};
	u32 pending_kickoff_cnt;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *sde_kms = NULL;
	struct sde_crtc_misr_info crtc_misr_info = {false, 0};
	bool is_regdma_blocking = false, is_vid_mode = false;
	struct sde_crtc *sde_crtc;

	if (!sde_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_crtc = to_sde_crtc(sde_enc->crtc);
	if (sde_encoder_check_curr_mode(&sde_enc->base, MSM_DISPLAY_VIDEO_MODE))
		is_vid_mode = true;

	is_regdma_blocking = (is_vid_mode ||
			_sde_encoder_is_autorefresh_enabled(sde_enc));

	/* don't perform flush/start operations for slave encoders */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];
		enum sde_rm_topology_name topology = SDE_RM_TOPOLOGY_NONE;

		if (!phys || phys->enable_state == SDE_ENC_DISABLED)
			continue;

		ctl = phys->hw_ctl;
		if (!ctl)
			continue;

		if (phys->connector)
			topology = sde_connector_get_topology_name(
					phys->connector);

		if (!phys->ops.needs_single_flush ||
				!phys->ops.needs_single_flush(phys)) {
			if (config_changed && ctl->ops.reg_dma_flush)
				ctl->ops.reg_dma_flush(ctl, is_regdma_blocking);
			_sde_encoder_trigger_flush(&sde_enc->base, phys, 0x0,
					config_changed);
		} else if (ctl->ops.get_pending_flush) {
			ctl->ops.get_pending_flush(ctl, &pending_flush);
		}
	}

	/* for split flush, combine pending flush masks and send to master */
	if (pending_flush.pending_flush_mask && sde_enc->cur_master) {
		ctl = sde_enc->cur_master->hw_ctl;
		if (config_changed && ctl->ops.reg_dma_flush)
			ctl->ops.reg_dma_flush(ctl, is_regdma_blocking);
		_sde_encoder_trigger_flush(&sde_enc->base, sde_enc->cur_master,
						&pending_flush,
						config_changed);
	}

	/* update pending_kickoff_cnt AFTER flush but before trigger start */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || phys->enable_state == SDE_ENC_DISABLED)
			continue;

		if (!phys->ops.needs_single_flush ||
				!phys->ops.needs_single_flush(phys)) {
			pending_kickoff_cnt =
					sde_encoder_phys_inc_pending(phys);
			SDE_EVT32(pending_kickoff_cnt, SDE_EVTLOG_FUNC_CASE1);
		} else {
			pending_kickoff_cnt =
					sde_encoder_phys_inc_pending(phys);
			SDE_EVT32(pending_kickoff_cnt,
					pending_flush.pending_flush_mask,
					SDE_EVTLOG_FUNC_CASE2);
		}
	}

	if (sde_enc->misr_enable)
		sde_encoder_misr_configure(&sde_enc->base, true,
				sde_enc->misr_frame_count);

	sde_crtc_get_misr_info(sde_enc->crtc, &crtc_misr_info);
	if (crtc_misr_info.misr_enable && sde_crtc &&
				sde_crtc->misr_reconfigure) {
		sde_crtc_misr_setup(sde_enc->crtc, true,
				crtc_misr_info.misr_frame_count);
		sde_crtc->misr_reconfigure = false;
	}

	_sde_encoder_trigger_start(sde_enc->cur_master);

	if (sde_enc->elevated_ahb_vote) {
		sde_kms = sde_encoder_get_kms(&sde_enc->base);
		priv = sde_enc->base.dev->dev_private;
		if (sde_kms != NULL) {
			sde_power_scale_reg_bus(&priv->phandle,
					VOTE_INDEX_LOW,
					false);
		}
		sde_enc->elevated_ahb_vote = false;
	}

}

static void _sde_encoder_ppsplit_swap_intf_for_right_only_update(
		struct drm_encoder *drm_enc,
		unsigned long *affected_displays,
		int num_active_phys)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *master;
	enum sde_rm_topology_name topology;
	bool is_right_only;

	if (!drm_enc || !affected_displays)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);
	master = sde_enc->cur_master;
	if (!master || !master->connector)
		return;

	topology = sde_connector_get_topology_name(master->connector);
	if (topology != SDE_RM_TOPOLOGY_PPSPLIT)
		return;

	/*
	 * For pingpong split, the slave pingpong won't generate IRQs. For
	 * right-only updates, we can't swap pingpongs, or simply swap the
	 * master/slave assignment, we actually have to swap the interfaces
	 * so that the master physical encoder will use a pingpong/interface
	 * that generates irqs on which to wait.
	 */
	is_right_only = !test_bit(0, affected_displays) &&
			test_bit(1, affected_displays);

	if (is_right_only && !sde_enc->intfs_swapped) {
		/* right-only update swap interfaces */
		swap(sde_enc->phys_encs[0]->intf_idx,
				sde_enc->phys_encs[1]->intf_idx);
		sde_enc->intfs_swapped = true;
	} else if (!is_right_only && sde_enc->intfs_swapped) {
		/* left-only or full update, swap back */
		swap(sde_enc->phys_encs[0]->intf_idx,
				sde_enc->phys_encs[1]->intf_idx);
		sde_enc->intfs_swapped = false;
	}

	SDE_DEBUG_ENC(sde_enc,
			"right_only %d swapped %d phys0->intf%d, phys1->intf%d\n",
			is_right_only, sde_enc->intfs_swapped,
			sde_enc->phys_encs[0]->intf_idx - INTF_0,
			sde_enc->phys_encs[1]->intf_idx - INTF_0);
	SDE_EVT32(DRMID(drm_enc), is_right_only, sde_enc->intfs_swapped,
			sde_enc->phys_encs[0]->intf_idx - INTF_0,
			sde_enc->phys_encs[1]->intf_idx - INTF_0,
			*affected_displays);

	/* ppsplit always uses master since ppslave invalid for irqs*/
	if (num_active_phys == 1)
		*affected_displays = BIT(0);
}

static void _sde_encoder_update_master(struct drm_encoder *drm_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	int i, num_active_phys;
	bool master_assigned = false;

	if (!drm_enc || !params)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);

	if (sde_enc->num_phys_encs <= 1)
		return;

	/* count bits set */
	num_active_phys = hweight_long(params->affected_displays);

	SDE_DEBUG_ENC(sde_enc, "affected_displays 0x%lx num_active_phys %d\n",
			params->affected_displays, num_active_phys);
	SDE_EVT32_VERBOSE(DRMID(drm_enc), params->affected_displays,
			num_active_phys);

	/* for left/right only update, ppsplit master switches interface */
	_sde_encoder_ppsplit_swap_intf_for_right_only_update(drm_enc,
			&params->affected_displays, num_active_phys);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		enum sde_enc_split_role prv_role, new_role;
		bool active = false;

		phys = sde_enc->phys_encs[i];
		if (!phys || !phys->ops.update_split_role || !phys->hw_pp)
			continue;

		active = test_bit(i, &params->affected_displays);
		prv_role = phys->split_role;

		if (active && num_active_phys == 1)
			new_role = ENC_ROLE_SOLO;
		else if (active && !master_assigned)
			new_role = ENC_ROLE_MASTER;
		else if (active)
			new_role = ENC_ROLE_SLAVE;
		else
			new_role = ENC_ROLE_SKIP;

		phys->ops.update_split_role(phys, new_role);
		if (new_role == ENC_ROLE_SOLO || new_role == ENC_ROLE_MASTER) {
			sde_enc->cur_master = phys;
			master_assigned = true;
		}

		SDE_DEBUG_ENC(sde_enc, "pp %d role prv %d new %d active %d\n",
				phys->hw_pp->idx - PINGPONG_0, prv_role,
				phys->split_role, active);
		SDE_EVT32(DRMID(drm_enc), params->affected_displays,
				phys->hw_pp->idx - PINGPONG_0, prv_role,
				phys->split_role, active, num_active_phys);
	}
}

bool sde_encoder_check_curr_mode(struct drm_encoder *drm_enc, u32 mode)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_display_info *disp_info;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return false;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;

	return (disp_info->curr_panel_mode == mode);
}

void sde_encoder_trigger_kickoff_pending(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned int i;
	struct sde_hw_ctl *ctl;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];

		if (phys && phys->hw_ctl && (phys == sde_enc->cur_master) &&
			sde_encoder_check_curr_mode(drm_enc,
					MSM_DISPLAY_CMD_MODE)) {
			ctl = phys->hw_ctl;
			if (ctl->ops.trigger_pending)
			/* update only for command mode primary ctl */
				ctl->ops.trigger_pending(ctl);
		}
	}
	sde_enc->idle_pc_restore = false;
}

static void sde_encoder_esd_trigger_work_handler(struct kthread_work *work)
{
	struct sde_encoder_virt *sde_enc = container_of(work,
				struct sde_encoder_virt, esd_trigger_work);

	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}

	sde_encoder_resource_control(&sde_enc->base,
			SDE_ENC_RC_EVENT_KICKOFF);
}

static void sde_encoder_input_event_work_handler(struct kthread_work *work)
{
	struct sde_encoder_virt *sde_enc = container_of(work,
				struct sde_encoder_virt, input_event_work);

	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}

	sde_encoder_resource_control(&sde_enc->base,
			SDE_ENC_RC_EVENT_EARLY_WAKEUP);
}

static void sde_encoder_early_wakeup_work_handler(struct kthread_work *work)
{
	struct sde_encoder_virt *sde_enc = container_of(work,
			struct sde_encoder_virt, early_wakeup_work);

	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}

	SDE_ATRACE_BEGIN("encoder_early_wakeup");
	sde_encoder_resource_control(&sde_enc->base,
			SDE_ENC_RC_EVENT_EARLY_WAKEUP);
	SDE_ATRACE_END("encoder_early_wakeup");
}

void sde_encoder_early_wakeup(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_thread *disp_thread = NULL;
	struct msm_drm_private *priv = NULL;

	priv = drm_enc->dev->dev_private;
	sde_enc = to_sde_encoder_virt(drm_enc);

	if (!sde_encoder_check_curr_mode(drm_enc, MSM_DISPLAY_CMD_MODE)) {
		SDE_DEBUG_ENC(sde_enc,
			"should only early wake up command mode display\n");
		return;
	}

	if (!sde_enc->crtc || (sde_enc->crtc->index
			>= ARRAY_SIZE(priv->event_thread))) {
		SDE_DEBUG_ENC(sde_enc, "invalid CRTC: %d or crtc index: %d\n",
			sde_enc->crtc == NULL,
			sde_enc->crtc ? sde_enc->crtc->index : -EINVAL);
		return;
	}

	disp_thread = &priv->disp_thread[sde_enc->crtc->index];

	SDE_ATRACE_BEGIN("queue_early_wakeup_work");
	kthread_queue_work(&disp_thread->worker,
				&sde_enc->early_wakeup_work);
	SDE_ATRACE_END("queue_early_wakeup_work");
}

int sde_encoder_poll_line_counts(struct drm_encoder *drm_enc)
{
	static const uint64_t timeout_us = 50000;
	static const uint64_t sleep_us = 20;
	struct sde_encoder_virt *sde_enc;
	ktime_t cur_ktime, exp_ktime;
	uint32_t line_count, tmp, i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->cur_master ||
			!sde_enc->cur_master->ops.get_line_count) {
		SDE_DEBUG_ENC(sde_enc, "can't get master line count\n");
		SDE_EVT32(DRMID(drm_enc), SDE_EVTLOG_ERROR);
		return -EINVAL;
	}

	exp_ktime = ktime_add_ms(ktime_get(), timeout_us / 1000);

	line_count = sde_enc->cur_master->ops.get_line_count(
			sde_enc->cur_master);

	for (i = 0; i < (timeout_us * 2 / sleep_us); ++i) {
		tmp = line_count;
		line_count = sde_enc->cur_master->ops.get_line_count(
				sde_enc->cur_master);
		if (line_count < tmp) {
			SDE_EVT32(DRMID(drm_enc), line_count);
			return 0;
		}

		cur_ktime = ktime_get();
		if (ktime_compare_safe(exp_ktime, cur_ktime) <= 0)
			break;

		usleep_range(sleep_us / 2, sleep_us);
	}

	SDE_EVT32(DRMID(drm_enc), line_count, SDE_EVTLOG_ERROR);
	return -ETIMEDOUT;
}

static int _helper_flush_qsync(struct sde_encoder_phys *phys_enc)
{
	struct drm_encoder *drm_enc;
	struct sde_rm_hw_iter rm_iter;
	bool lm_valid = false;
	bool intf_valid = false;

	if (!phys_enc || !phys_enc->parent) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	drm_enc = phys_enc->parent;

	/* Flush the interfaces for AVR update or Qsync with INTF TE */
	if (phys_enc->intf_mode == INTF_MODE_VIDEO ||
			(phys_enc->intf_mode == INTF_MODE_CMD &&
			phys_enc->has_intf_te)) {
		sde_rm_init_hw_iter(&rm_iter, drm_enc->base.id,
				SDE_HW_BLK_INTF);
		while (sde_rm_get_hw(&phys_enc->sde_kms->rm, &rm_iter)) {
			struct sde_hw_intf *hw_intf =
				(struct sde_hw_intf *)rm_iter.hw;

			if (!hw_intf)
				continue;

			if (phys_enc->hw_ctl->ops.update_bitmask)
				phys_enc->hw_ctl->ops.update_bitmask(
						phys_enc->hw_ctl,
						SDE_HW_FLUSH_INTF,
						hw_intf->idx, 1);

			intf_valid = true;
		}

		if (!intf_valid) {
			SDE_ERROR_ENC(to_sde_encoder_virt(drm_enc),
				"intf not found to flush\n");
			return -EFAULT;
		}
	} else {
		sde_rm_init_hw_iter(&rm_iter, drm_enc->base.id, SDE_HW_BLK_LM);
		while (sde_rm_get_hw(&phys_enc->sde_kms->rm, &rm_iter)) {
			struct sde_hw_mixer *hw_lm =
					(struct sde_hw_mixer *)rm_iter.hw;

			if (!hw_lm)
				continue;

			/* update LM flush for HW without INTF TE */
			if (phys_enc->hw_ctl->ops.update_bitmask_mixer)
				phys_enc->hw_ctl->ops.update_bitmask_mixer(
						phys_enc->hw_ctl,
						hw_lm->idx, 1);

			lm_valid = true;
		}

		if (!lm_valid) {
			SDE_ERROR_ENC(to_sde_encoder_virt(drm_enc),
				"lm not found to flush\n");
			return -EFAULT;
		}
	}

	return 0;
}

static void _sde_encoder_helper_hdr_plus_mempool_update(
		struct sde_encoder_virt *sde_enc)
{
	struct sde_connector_dyn_hdr_metadata *dhdr_meta = NULL;
	struct sde_hw_mdp *mdptop = NULL;

	sde_enc->dynamic_hdr_updated = false;
	if (sde_enc->cur_master) {
		mdptop = sde_enc->cur_master->hw_mdptop;
		dhdr_meta = sde_connector_get_dyn_hdr_meta(
				sde_enc->cur_master->connector);
	}

	if (!mdptop || !dhdr_meta || !dhdr_meta->dynamic_hdr_update)
		return;

	if (mdptop->ops.set_hdr_plus_metadata) {
		sde_enc->dynamic_hdr_updated = true;
		mdptop->ops.set_hdr_plus_metadata(
				mdptop, dhdr_meta->dynamic_hdr_payload,
				dhdr_meta->dynamic_hdr_payload_size,
				sde_enc->cur_master->intf_idx == INTF_0 ?
				0 : 1);
	}
}

void sde_encoder_needs_hw_reset(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct sde_encoder_phys *phys;
	int i;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.hw_reset)
			phys->ops.hw_reset(phys);
	}
}

int sde_encoder_prepare_for_kickoff(struct drm_encoder *drm_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	struct sde_kms *sde_kms = NULL;
	struct sde_crtc *sde_crtc;
	bool needs_hw_reset = false, is_cmd_mode;
	int i, rc, ret = 0;
	struct msm_display_info *disp_info;

	if (!drm_enc || !params || !drm_enc->dev ||
		!drm_enc->dev->dev_private) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms)
		return -EINVAL;

	disp_info = &sde_enc->disp_info;
	sde_crtc = to_sde_crtc(sde_enc->crtc);

	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc));

	is_cmd_mode = sde_encoder_check_curr_mode(drm_enc,
				MSM_DISPLAY_CMD_MODE);
	if (sde_enc->cur_master && sde_enc->cur_master->connector
			&& is_cmd_mode)
		sde_enc->frame_trigger_mode = sde_connector_get_property(
			sde_enc->cur_master->connector->state,
			CONNECTOR_PROP_CMD_FRAME_TRIGGER_MODE);

	_sde_encoder_helper_hdr_plus_mempool_update(sde_enc);

	/* prepare for next kickoff, may include waiting on previous kickoff */
	SDE_ATRACE_BEGIN("sde_encoder_prepare_for_kickoff");
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		params->frame_trigger_mode = sde_enc->frame_trigger_mode;
		params->recovery_events_enabled =
					sde_enc->recovery_events_enabled;
		if (phys) {
			if (phys->ops.prepare_for_kickoff) {
				rc = phys->ops.prepare_for_kickoff(
						phys, params);
				if (rc)
					ret = rc;
			}
			if (phys->enable_state == SDE_ENC_ERR_NEEDS_HW_RESET)
				needs_hw_reset = true;
			_sde_encoder_setup_dither(phys);

			if (sde_enc->cur_master &&
					sde_connector_is_qsync_updated(
					sde_enc->cur_master->connector)) {
				_helper_flush_qsync(phys);
			}
		}
	}

	rc = sde_encoder_resource_control(drm_enc, SDE_ENC_RC_EVENT_KICKOFF);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "resource kickoff failed rc %d\n", rc);
		ret = rc;
		goto end;
	}

	/* if any phys needs reset, reset all phys, in-order */
	if (needs_hw_reset)
		sde_encoder_needs_hw_reset(drm_enc);

	_sde_encoder_update_master(drm_enc, params);

	_sde_encoder_update_roi(drm_enc);

	if (sde_enc->cur_master && sde_enc->cur_master->connector) {
		rc = sde_connector_pre_kickoff(sde_enc->cur_master->connector);
		if (rc) {
			SDE_ERROR_ENC(sde_enc, "kickoff conn%d failed rc %d\n",
					sde_enc->cur_master->connector->base.id,
					rc);
			ret = rc;
		}
	}

	if (sde_enc->cur_master &&
		((is_cmd_mode && sde_enc->cur_master->cont_splash_enabled) ||
			!sde_enc->cur_master->cont_splash_enabled)) {
		rc = sde_encoder_dce_setup(sde_enc, params);
		if (rc) {
			SDE_ERROR_ENC(sde_enc, "failed to setup DSC: %d\n", rc);
			ret = rc;
		}
	}

	sde_encoder_dce_flush(sde_enc);

	if (sde_enc->cur_master && !sde_enc->cur_master->cont_splash_enabled)
		sde_configure_qdss(sde_enc, sde_enc->cur_master->hw_qdss,
				sde_enc->cur_master, sde_kms->qdss_enabled);

end:
	SDE_ATRACE_END("sde_encoder_prepare_for_kickoff");
	return ret;
}

/**
 * _sde_encoder_reset_ctl_hw - reset h/w configuration for all ctl's associated
 *	with the specified encoder, and unstage all pipes from it
 * @encoder:	encoder pointer
 * Returns: 0 on success
 */
static int _sde_encoder_reset_ctl_hw(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned int i;
	int rc = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	SDE_ATRACE_BEGIN("encoder_release_lm");
	SDE_DEBUG_ENC(sde_enc, "\n");

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (!phys)
			continue;

		SDE_EVT32(DRMID(drm_enc), phys->intf_idx - INTF_0);

		rc = sde_encoder_helper_reset_mixers(phys, NULL);
		if (rc)
			SDE_EVT32(DRMID(drm_enc), rc, SDE_EVTLOG_ERROR);
	}

	SDE_ATRACE_END("encoder_release_lm");
	return rc;
}

void sde_encoder_kickoff(struct drm_encoder *drm_enc, bool is_error,
		bool config_changed)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_ATRACE_BEGIN("encoder_kickoff");
	sde_enc = to_sde_encoder_virt(drm_enc);

	SDE_DEBUG_ENC(sde_enc, "\n");

	/* create a 'no pipes' commit to release buffers on errors */
	if (is_error)
		_sde_encoder_reset_ctl_hw(drm_enc);

	/* All phys encs are ready to go, trigger the kickoff */
	_sde_encoder_kickoff_phys(sde_enc, config_changed);

	/* allow phys encs to handle any post-kickoff business */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.handle_post_kickoff)
			phys->ops.handle_post_kickoff(phys);
	}

	SDE_ATRACE_END("encoder_kickoff");
}

void sde_encoder_helper_get_pp_line_count(struct drm_encoder *drm_enc,
			struct sde_hw_pp_vsync_info *info)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	int i, ret;

	if (!drm_enc || !info)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->hw_intf && phys->hw_pp
				&& phys->hw_intf->ops.get_vsync_info) {
			ret = phys->hw_intf->ops.get_vsync_info(
						phys->hw_intf, &info[i]);
			if (!ret) {
				info[i].pp_idx = phys->hw_pp->idx - PINGPONG_0;
				info[i].intf_idx = phys->hw_intf->idx - INTF_0;
			}
		}
	}
}

void sde_encoder_helper_get_transfer_time(struct drm_encoder *drm_enc,
			u32 *transfer_time_us)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_mode_info *info;

	if (!drm_enc || !transfer_time_us) {
		SDE_ERROR("bad arg: encoder:%d transfer_time:%d\n", !drm_enc,
				!transfer_time_us);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	info = &sde_enc->mode_info;

	*transfer_time_us = info->mdp_transfer_time_us;
}

int sde_encoder_helper_reset_mixers(struct sde_encoder_phys *phys_enc,
		struct drm_framebuffer *fb)
{
	struct drm_encoder *drm_enc;
	struct sde_hw_mixer_cfg mixer;
	struct sde_rm_hw_iter lm_iter;
	bool lm_valid = false;

	if (!phys_enc || !phys_enc->parent) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	drm_enc = phys_enc->parent;
	memset(&mixer, 0, sizeof(mixer));

	/* reset associated CTL/LMs */
	if (phys_enc->hw_ctl->ops.clear_all_blendstages)
		phys_enc->hw_ctl->ops.clear_all_blendstages(phys_enc->hw_ctl);

	sde_rm_init_hw_iter(&lm_iter, drm_enc->base.id, SDE_HW_BLK_LM);
	while (sde_rm_get_hw(&phys_enc->sde_kms->rm, &lm_iter)) {
		struct sde_hw_mixer *hw_lm = (struct sde_hw_mixer *)lm_iter.hw;

		if (!hw_lm)
			continue;

		/* need to flush LM to remove it */
		if (phys_enc->hw_ctl->ops.update_bitmask_mixer)
			phys_enc->hw_ctl->ops.update_bitmask_mixer(
					phys_enc->hw_ctl,
					hw_lm->idx, 1);

		if (fb) {
			/* assume a single LM if targeting a frame buffer */
			if (lm_valid)
				continue;

			mixer.out_height = fb->height;
			mixer.out_width = fb->width;

			if (hw_lm->ops.setup_mixer_out)
				hw_lm->ops.setup_mixer_out(hw_lm, &mixer);
		}

		lm_valid = true;

		/* only enable border color on LM */
		if (phys_enc->hw_ctl->ops.setup_blendstage)
			phys_enc->hw_ctl->ops.setup_blendstage(
				phys_enc->hw_ctl, hw_lm->idx, NULL, false);
	}

	if (!lm_valid) {
		SDE_ERROR_ENC(to_sde_encoder_virt(drm_enc), "lm not found\n");
		return -EFAULT;
	}
	return 0;
}

int sde_encoder_prepare_commit(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	int i, rc = 0, ret = 0;
	struct sde_hw_ctl *ctl;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	/* update the qsync parameters for the current frame */
	if (sde_enc->cur_master)
		sde_connector_set_qsync_params(
				sde_enc->cur_master->connector);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.prepare_commit)
			phys->ops.prepare_commit(phys);

		if (phys && phys->enable_state == SDE_ENC_ERR_NEEDS_HW_RESET)
			ret = -ETIMEDOUT;

		if (phys && phys->hw_ctl) {
			ctl = phys->hw_ctl;
			/*
			 * avoid clearing the pending flush during the first
			 * frame update after idle power collpase as the
			 * restore path would have updated the pending flush
			 */
			if (!sde_enc->idle_pc_restore &&
					ctl->ops.clear_pending_flush)
				ctl->ops.clear_pending_flush(ctl);
		}
	}

	if (sde_enc->cur_master && sde_enc->cur_master->connector) {
		rc = sde_connector_prepare_commit(
				  sde_enc->cur_master->connector);
		if (rc)
			SDE_ERROR_ENC(sde_enc,
				      "prepare commit failed conn %d rc %d\n",
				      sde_enc->cur_master->connector->base.id,
				      rc);
	}

	return ret;
}

void sde_encoder_helper_setup_misr(struct sde_encoder_phys *phys_enc,
						bool enable, u32 frame_count)
{
	if (!phys_enc)
		return;

	if (phys_enc->hw_intf && phys_enc->hw_intf->ops.setup_misr)
		phys_enc->hw_intf->ops.setup_misr(phys_enc->hw_intf,
				enable, frame_count);
}

int sde_encoder_helper_collect_misr(struct sde_encoder_phys *phys_enc,
		bool nonblock, u32 *misr_value)
{
	if (!phys_enc)
		return -EINVAL;

	return phys_enc->hw_intf && phys_enc->hw_intf->ops.collect_misr ?
			phys_enc->hw_intf->ops.collect_misr(phys_enc->hw_intf,
			nonblock, misr_value) : -ENOTSUPP;
}

#ifdef CONFIG_DEBUG_FS
static int _sde_encoder_status_show(struct seq_file *s, void *data)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	if (!s || !s->private)
		return -EINVAL;

	sde_enc = s->private;

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys)
			continue;

		seq_printf(s, "intf:%d    vsync:%8d     underrun:%8d    ",
				phys->intf_idx - INTF_0,
				atomic_read(&phys->vsync_cnt),
				atomic_read(&phys->underrun_cnt));

		switch (phys->intf_mode) {
		case INTF_MODE_VIDEO:
			seq_puts(s, "mode: video\n");
			break;
		case INTF_MODE_CMD:
			seq_puts(s, "mode: command\n");
			break;
		case INTF_MODE_WB_BLOCK:
			seq_puts(s, "mode: wb block\n");
			break;
		case INTF_MODE_WB_LINE:
			seq_puts(s, "mode: wb line\n");
			break;
		default:
			seq_puts(s, "mode: ???\n");
			break;
		}
	}
	mutex_unlock(&sde_enc->enc_lock);

	return 0;
}

static int _sde_encoder_debugfs_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, _sde_encoder_status_show, inode->i_private);
}

static ssize_t _sde_encoder_misr_setup(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_encoder_virt *sde_enc;
	char buf[MISR_BUFF_SIZE + 1];
	size_t buff_copy;
	u32 frame_count, enable;
	struct sde_kms *sde_kms = NULL;
	struct drm_encoder *drm_enc;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_enc = file->private_data;
	if (!sde_enc)
		return -EINVAL;

	sde_kms = sde_encoder_get_kms(&sde_enc->base);
	if (!sde_kms)
		return -EINVAL;

	drm_enc = &sde_enc->base;

	if (sde_kms_is_secure_session_inprogress(sde_kms)) {
		SDE_DEBUG_ENC(sde_enc, "misr enable/disable not allowed\n");
		return -ENOTSUPP;
	}

	buff_copy = min_t(size_t, count, MISR_BUFF_SIZE);
	if (copy_from_user(buf, user_buf, buff_copy))
		return -EINVAL;

	buf[buff_copy] = 0; /* end of string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2)
		return -EINVAL;

	sde_enc->misr_enable = enable;
	sde_enc->misr_reconfigure = true;
	sde_enc->misr_frame_count = frame_count;
	return count;
}

static ssize_t _sde_encoder_misr_read(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_kms *sde_kms = NULL;
	struct drm_encoder *drm_enc;
	int i = 0, len = 0;
	char buf[MISR_BUFF_SIZE + 1] = {'\0'};
	int rc;

	if (*ppos)
		return 0;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_enc = file->private_data;
	sde_kms = sde_encoder_get_kms(&sde_enc->base);
	if (!sde_kms)
		return -EINVAL;

	if (sde_kms_is_secure_session_inprogress(sde_kms)) {
		SDE_DEBUG_ENC(sde_enc, "misr read not allowed\n");
		return -ENOTSUPP;
	}
	drm_enc = &sde_enc->base;

	rc = pm_runtime_get_sync(drm_enc->dev->dev);
	if (rc < 0)
		return rc;

	if (!sde_enc->misr_enable) {
		len += scnprintf(buf + len, MISR_BUFF_SIZE - len,
				"disabled\n");
		goto buff_check;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];
		u32 misr_value = 0;

		if (!phys || !phys->ops.collect_misr) {
			len += scnprintf(buf + len, MISR_BUFF_SIZE - len,
					"invalid\n");
			SDE_ERROR_ENC(sde_enc, "invalid misr ops\n");
			continue;
		}

		rc = phys->ops.collect_misr(phys, false, &misr_value);
		if (rc) {
			len += scnprintf(buf + len, MISR_BUFF_SIZE - len,
					"invalid\n");
			SDE_ERROR_ENC(sde_enc, "failed to collect misr %d\n",
					rc);
			continue;
		} else {
			len += scnprintf(buf + len, MISR_BUFF_SIZE - len,
					"Intf idx:%d\n",
					phys->intf_idx - INTF_0);
			len += scnprintf(buf + len, MISR_BUFF_SIZE - len,
					"0x%x\n", misr_value);
		}
	}

buff_check:
	if (count <= len) {
		len = 0;
		goto end;
	}

	if (copy_to_user(user_buff, buf, len)) {
		len = -EFAULT;
		goto end;
	}

	*ppos += len;   /* increase offset */

end:
	pm_runtime_put_sync(drm_enc->dev->dev);
	return len;
}

static int _sde_encoder_init_debugfs(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_kms *sde_kms;
	int i;

	static const struct file_operations debugfs_status_fops = {
		.open =		_sde_encoder_debugfs_status_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};

	static const struct file_operations debugfs_misr_fops = {
		.open = simple_open,
		.read = _sde_encoder_misr_read,
		.write = _sde_encoder_misr_setup,
	};

	char name[SDE_NAME_SIZE];

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	sde_kms = sde_encoder_get_kms(drm_enc);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	snprintf(name, SDE_NAME_SIZE, "encoder%u", drm_enc->base.id);

	/* create overall sub-directory for the encoder */
	sde_enc->debugfs_root = debugfs_create_dir(name,
			drm_enc->dev->primary->debugfs_root);
	if (!sde_enc->debugfs_root)
		return -ENOMEM;

	/* don't error check these */
	debugfs_create_file("status", 0400,
		sde_enc->debugfs_root, sde_enc, &debugfs_status_fops);

	debugfs_create_file("misr_data", 0600,
		sde_enc->debugfs_root, sde_enc, &debugfs_misr_fops);

	debugfs_create_bool("idle_power_collapse", 0600, sde_enc->debugfs_root,
			&sde_enc->idle_pc_enabled);

	debugfs_create_u32("frame_trigger_mode", 0400, sde_enc->debugfs_root,
			&sde_enc->frame_trigger_mode);

	for (i = 0; i < sde_enc->num_phys_encs; i++)
		if (sde_enc->phys_encs[i] &&
				sde_enc->phys_encs[i]->ops.late_register)
			sde_enc->phys_encs[i]->ops.late_register(
					sde_enc->phys_encs[i],
					sde_enc->debugfs_root);

	return 0;
}

static void _sde_encoder_destroy_debugfs(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc)
		return;

	sde_enc = to_sde_encoder_virt(drm_enc);
	debugfs_remove_recursive(sde_enc->debugfs_root);
}
#else
static int _sde_encoder_init_debugfs(struct drm_encoder *drm_enc)
{
	return 0;
}

static void _sde_encoder_destroy_debugfs(struct drm_encoder *drm_enc)
{
}
#endif

static int sde_encoder_late_register(struct drm_encoder *encoder)
{
	return _sde_encoder_init_debugfs(encoder);
}

static void sde_encoder_early_unregister(struct drm_encoder *encoder)
{
	_sde_encoder_destroy_debugfs(encoder);
}

static int sde_encoder_virt_add_phys_encs(
		struct msm_display_info *disp_info,
		struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;
	u32 display_caps = disp_info->capabilities;

	SDE_DEBUG_ENC(sde_enc, "\n");

	/*
	 * We may create up to NUM_PHYS_ENCODER_TYPES physical encoder types
	 * in this function, check up-front.
	 */
	if (sde_enc->num_phys_encs + NUM_PHYS_ENCODER_TYPES >=
			ARRAY_SIZE(sde_enc->phys_encs)) {
		SDE_ERROR_ENC(sde_enc, "too many physical encoders %d\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	if (display_caps & MSM_DISPLAY_CAP_VID_MODE) {
		enc = sde_encoder_phys_vid_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			SDE_ERROR_ENC(sde_enc, "failed to init vid enc: %ld\n",
				PTR_ERR(enc));
			return !enc ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_vid_encs[sde_enc->num_phys_encs] = enc;
	}

	if (display_caps & MSM_DISPLAY_CAP_CMD_MODE) {
		enc = sde_encoder_phys_cmd_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			SDE_ERROR_ENC(sde_enc, "failed to init cmd enc: %ld\n",
				PTR_ERR(enc));
			return !enc ? -EINVAL : PTR_ERR(enc);
		}
		sde_enc->phys_cmd_encs[sde_enc->num_phys_encs] = enc;
	}

	if (disp_info->curr_panel_mode == MSM_DISPLAY_VIDEO_MODE)
		sde_enc->phys_encs[sde_enc->num_phys_encs] =
			sde_enc->phys_vid_encs[sde_enc->num_phys_encs];
	else
		sde_enc->phys_encs[sde_enc->num_phys_encs] =
			sde_enc->phys_cmd_encs[sde_enc->num_phys_encs];

	++sde_enc->num_phys_encs;

	return 0;
}

static int sde_encoder_virt_add_phys_enc_wb(struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;

	if (!sde_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	SDE_DEBUG_ENC(sde_enc, "\n");

	if (sde_enc->num_phys_encs + 1 >= ARRAY_SIZE(sde_enc->phys_encs)) {
		SDE_ERROR_ENC(sde_enc, "too many physical encoders %d\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	enc = sde_encoder_phys_wb_init(params);

	if (IS_ERR_OR_NULL(enc)) {
		SDE_ERROR_ENC(sde_enc, "failed to init wb enc: %ld\n",
			PTR_ERR(enc));
		return !enc ? -EINVAL : PTR_ERR(enc);
	}

	sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
	++sde_enc->num_phys_encs;

	return 0;
}

static int sde_encoder_setup_display(struct sde_encoder_virt *sde_enc,
				 struct sde_kms *sde_kms,
				 struct msm_display_info *disp_info,
				 int *drm_enc_mode)
{
	int ret = 0;
	int i = 0;
	enum sde_intf_type intf_type;
	struct sde_encoder_virt_ops parent_ops = {
		sde_encoder_vblank_callback,
		sde_encoder_underrun_callback,
		sde_encoder_frame_done_callback,
		sde_encoder_get_qsync_fps_callback,
	};
	struct sde_enc_phys_init_params phys_params;

	if (!sde_enc || !sde_kms) {
		SDE_ERROR("invalid arg(s), enc %d kms %d\n",
				!sde_enc, !sde_kms);
		return -EINVAL;
	}

	memset(&phys_params, 0, sizeof(phys_params));
	phys_params.sde_kms = sde_kms;
	phys_params.parent = &sde_enc->base;
	phys_params.parent_ops = parent_ops;
	phys_params.enc_spinlock = &sde_enc->enc_spinlock;
	phys_params.vblank_ctl_lock = &sde_enc->vblank_ctl_lock;

	SDE_DEBUG("\n");

	if (disp_info->intf_type == DRM_MODE_CONNECTOR_DSI) {
		*drm_enc_mode = DRM_MODE_ENCODER_DSI;
		intf_type = INTF_DSI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_HDMIA) {
		*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_HDMI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_DisplayPort) {
		if (disp_info->capabilities & MSM_DISPLAY_CAP_MST_MODE)
			*drm_enc_mode = DRM_MODE_ENCODER_DPMST;
		else
			*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_DP;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_VIRTUAL) {
		*drm_enc_mode = DRM_MODE_ENCODER_VIRTUAL;
		intf_type = INTF_WB;
	} else {
		SDE_ERROR_ENC(sde_enc, "unsupported display interface type\n");
		return -EINVAL;
	}

	WARN_ON(disp_info->num_of_h_tiles < 1);

	sde_enc->display_num_of_h_tiles = disp_info->num_of_h_tiles;
	sde_enc->te_source = disp_info->te_source;

	SDE_DEBUG("dsi_info->num_of_h_tiles %d\n", disp_info->num_of_h_tiles);

	if ((disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) ||
	    (disp_info->capabilities & MSM_DISPLAY_CAP_VID_MODE))
		sde_enc->idle_pc_enabled = sde_kms->catalog->has_idle_pc;

	sde_enc->input_event_enabled = sde_kms->catalog->wakeup_with_touch;

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < disp_info->num_of_h_tiles && !ret; i++) {
		/*
		 * Left-most tile is at index 0, content is controller id
		 * h_tile_instance_ids[2] = {0, 1}; DSI0 = left, DSI1 = right
		 * h_tile_instance_ids[2] = {1, 0}; DSI1 = left, DSI0 = right
		 */
		u32 controller_id = disp_info->h_tile_instance[i];

		if (disp_info->num_of_h_tiles > 1) {
			if (i == 0)
				phys_params.split_role = ENC_ROLE_MASTER;
			else
				phys_params.split_role = ENC_ROLE_SLAVE;
		} else {
			phys_params.split_role = ENC_ROLE_SOLO;
		}

		SDE_DEBUG("h_tile_instance %d = %d, split_role %d\n",
				i, controller_id, phys_params.split_role);

		if (sde_enc->ops.phys_init) {
			struct sde_encoder_phys *enc;

			enc = sde_enc->ops.phys_init(intf_type,
					controller_id,
					&phys_params);
			if (enc) {
				sde_enc->phys_encs[sde_enc->num_phys_encs] =
					enc;
				++sde_enc->num_phys_encs;
			} else
				SDE_ERROR_ENC(sde_enc,
						"failed to add phys encs\n");

			continue;
		}

		if (intf_type == INTF_WB) {
			phys_params.intf_idx = INTF_MAX;
			phys_params.wb_idx = sde_encoder_get_wb(
					sde_kms->catalog,
					intf_type, controller_id);
			if (phys_params.wb_idx == WB_MAX) {
				SDE_ERROR_ENC(sde_enc,
					"could not get wb: type %d, id %d\n",
					intf_type, controller_id);
				ret = -EINVAL;
			}
		} else {
			phys_params.wb_idx = WB_MAX;
			phys_params.intf_idx = sde_encoder_get_intf(
					sde_kms->catalog, intf_type,
					controller_id);
			if (phys_params.intf_idx == INTF_MAX) {
				SDE_ERROR_ENC(sde_enc,
					"could not get wb: type %d, id %d\n",
					intf_type, controller_id);
				ret = -EINVAL;
			}
		}

		if (!ret) {
			if (intf_type == INTF_WB)
				ret = sde_encoder_virt_add_phys_enc_wb(sde_enc,
						&phys_params);
			else
				ret = sde_encoder_virt_add_phys_encs(
						disp_info,
						sde_enc,
						&phys_params);
			if (ret)
				SDE_ERROR_ENC(sde_enc,
						"failed to add phys encs\n");
		}
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *vid_phys = sde_enc->phys_vid_encs[i];
		struct sde_encoder_phys *cmd_phys = sde_enc->phys_cmd_encs[i];

		if (vid_phys) {
			atomic_set(&vid_phys->vsync_cnt, 0);
			atomic_set(&vid_phys->underrun_cnt, 0);
		}

		if (cmd_phys) {
			atomic_set(&cmd_phys->vsync_cnt, 0);
			atomic_set(&cmd_phys->underrun_cnt, 0);
		}

	}
	mutex_unlock(&sde_enc->enc_lock);

	return ret;
}

static const struct drm_encoder_helper_funcs sde_encoder_helper_funcs = {
	.mode_set = sde_encoder_virt_mode_set,
	.disable = sde_encoder_virt_disable,
	.enable = sde_encoder_virt_enable,
	.atomic_check = sde_encoder_virt_atomic_check,
};

static const struct drm_encoder_funcs sde_encoder_funcs = {
		.destroy = sde_encoder_destroy,
		.late_register = sde_encoder_late_register,
		.early_unregister = sde_encoder_early_unregister,
};

struct drm_encoder *sde_encoder_init_with_ops(
		struct drm_device *dev,
		struct msm_display_info *disp_info,
		const struct sde_encoder_ops *ops)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	char name[SDE_NAME_SIZE];
	int ret = 0, i, intf_index = INTF_MAX;
	struct sde_encoder_phys *phys = NULL;

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	if (ops)
		sde_enc->ops = *ops;

	mutex_init(&sde_enc->enc_lock);
	ret = sde_encoder_setup_display(sde_enc, sde_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	sde_enc->cur_master = NULL;
	spin_lock_init(&sde_enc->enc_spinlock);
	mutex_init(&sde_enc->vblank_ctl_lock);
	for (i = 0; i < MAX_PHYS_ENCODERS_PER_VIRTUAL; i++)
		atomic_set(&sde_enc->frame_done_cnt[i], 0);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs, drm_enc_mode, NULL);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (!phys)
			continue;
		if (phys->ops.is_master && phys->ops.is_master(phys))
			intf_index = phys->intf_idx - INTF_0;
	}
	snprintf(name, SDE_NAME_SIZE, "rsc_enc%u", drm_enc->base.id);
	sde_enc->rsc_client = sde_rsc_client_create(SDE_RSC_INDEX, name,
		(disp_info->display_type == SDE_CONNECTOR_PRIMARY) ?
		SDE_RSC_PRIMARY_DISP_CLIENT :
		SDE_RSC_EXTERNAL_DISP_CLIENT, intf_index + 1);
	if (IS_ERR_OR_NULL(sde_enc->rsc_client)) {
		SDE_DEBUG("sde rsc client create failed :%ld\n",
						PTR_ERR(sde_enc->rsc_client));
		sde_enc->rsc_client = NULL;
	}

	if (disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE &&
		sde_enc->input_event_enabled) {
		ret = _sde_encoder_input_handler(sde_enc);
		if (ret)
			SDE_ERROR(
			"input handler registration failed, rc = %d\n", ret);
	}

	mutex_init(&sde_enc->rc_lock);
	kthread_init_delayed_work(&sde_enc->delayed_off_work,
			sde_encoder_off_work);
	sde_enc->vblank_enabled = false;
	sde_enc->qdss_status = false;

	kthread_init_work(&sde_enc->input_event_work,
			sde_encoder_input_event_work_handler);

	kthread_init_work(&sde_enc->early_wakeup_work,
			sde_encoder_early_wakeup_work_handler);

	kthread_init_work(&sde_enc->esd_trigger_work,
			sde_encoder_esd_trigger_work_handler);

	memcpy(&sde_enc->disp_info, disp_info, sizeof(*disp_info));

	SDE_DEBUG_ENC(sde_enc, "created\n");

	return drm_enc;

fail:
	SDE_ERROR("failed to create encoder\n");
	if (drm_enc)
		sde_encoder_destroy(drm_enc);

	return ERR_PTR(ret);
}

struct drm_encoder *sde_encoder_init(
		struct drm_device *dev,
		struct msm_display_info *disp_info)
{
	return sde_encoder_init_with_ops(dev, disp_info, NULL);
}

int sde_encoder_wait_for_event(struct drm_encoder *drm_enc,
	enum msm_event_wait event)
{
	int (*fn_wait)(struct sde_encoder_phys *phys_enc) = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;
	char atrace_buf[32];

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		switch (event) {
		case MSM_ENC_COMMIT_DONE:
			fn_wait = phys->ops.wait_for_commit_done;
			break;
		case MSM_ENC_TX_COMPLETE:
			fn_wait = phys->ops.wait_for_tx_complete;
			break;
		case MSM_ENC_VBLANK:
			fn_wait = phys->ops.wait_for_vblank;
			break;
		case MSM_ENC_ACTIVE_REGION:
			fn_wait = phys->ops.wait_for_active;
			break;
		default:
			SDE_ERROR_ENC(sde_enc, "unknown wait event %d\n",
					event);
			return -EINVAL;
		}

		if (phys && fn_wait) {
			snprintf(atrace_buf, sizeof(atrace_buf),
				"wait_completion_event_%d", event);
			SDE_ATRACE_BEGIN(atrace_buf);
			ret = fn_wait(phys);
			SDE_ATRACE_END(atrace_buf);
			if (ret)
				return ret;
		}
	}

	return ret;
}

void sde_encoder_helper_get_jitter_bounds_ns(struct drm_encoder *drm_enc,
		u64 *l_bound, u64 *u_bound)
{
	struct sde_encoder_virt *sde_enc;
	u64 jitter_ns, frametime_ns;
	struct msm_mode_info *info;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	info = &sde_enc->mode_info;

	frametime_ns = (1 * 1000000000) / info->frame_rate;
	jitter_ns =  info->jitter_numer * frametime_ns;
	do_div(jitter_ns, info->jitter_denom * 100);

	*l_bound = frametime_ns - jitter_ns;
	*u_bound = frametime_ns + jitter_ns;
}

u32 sde_encoder_get_fps(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return 0;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc->mode_info.frame_rate;
}

enum sde_intf_mode sde_encoder_get_intf_mode(struct drm_encoder *encoder)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i;

	if (!encoder) {
		SDE_ERROR("invalid encoder\n");
		return INTF_MODE_NONE;
	}
	sde_enc = to_sde_encoder_virt(encoder);

	if (sde_enc->cur_master)
		return sde_enc->cur_master->intf_mode;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys)
			return phys->intf_mode;
	}

	return INTF_MODE_NONE;
}

static void _sde_encoder_cache_hw_res_cont_splash(
		struct drm_encoder *encoder,
		struct sde_kms *sde_kms)
{
	int i, idx;
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys_enc;
	struct sde_rm_hw_iter dsc_iter, pp_iter, ctl_iter, intf_iter;

	sde_enc = to_sde_encoder_virt(encoder);

	sde_rm_init_hw_iter(&pp_iter, encoder->base.id, SDE_HW_BLK_PINGPONG);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_pp[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &pp_iter))
			break;
		sde_enc->hw_pp[i] = (struct sde_hw_pingpong *) pp_iter.hw;
	}

	sde_rm_init_hw_iter(&dsc_iter, encoder->base.id, SDE_HW_BLK_DSC);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_dsc[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &dsc_iter))
			break;
		sde_enc->hw_dsc[i] = (struct sde_hw_dsc *) dsc_iter.hw;
	}

	/*
	 * If we have multiple phys encoders with one controller, make
	 * sure to populate the controller pointer in both phys encoders.
	 */
	for (idx = 0; idx < sde_enc->num_phys_encs; idx++) {
		phys_enc = sde_enc->phys_encs[idx];
		phys_enc->hw_ctl = NULL;

		sde_rm_init_hw_iter(&ctl_iter, encoder->base.id,
				SDE_HW_BLK_CTL);
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			if (sde_rm_get_hw(&sde_kms->rm, &ctl_iter)) {
				phys_enc->hw_ctl =
					(struct sde_hw_ctl *) ctl_iter.hw;
				pr_debug("HW CTL intf_idx:%d hw_ctl:[0x%pK]\n",
					phys_enc->intf_idx, phys_enc->hw_ctl);
			}
		}
	}

	sde_rm_init_hw_iter(&intf_iter, encoder->base.id, SDE_HW_BLK_INTF);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		phys->hw_intf = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &intf_iter))
			break;
		phys->hw_intf = (struct sde_hw_intf *) intf_iter.hw;
	}
}

/**
 * sde_encoder_update_caps_for_cont_splash - update encoder settings during
 *	device bootup when cont_splash is enabled
 * @drm_enc:	Pointer to drm encoder structure
 * @splash_display: Pointer to sde_splash_display corresponding to this encoder
 * @enable:	boolean indicates enable or displae state of splash
 * @Return:	true if successful in updating the encoder structure
 */
int sde_encoder_update_caps_for_cont_splash(struct drm_encoder *encoder,
		struct sde_splash_display *splash_display, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_connector *conn = NULL;
	struct sde_connector *sde_conn = NULL;
	struct sde_connector_state *sde_conn_state = NULL;
	struct drm_display_mode *drm_mode = NULL;
	struct sde_encoder_phys *phys_enc;
	int ret = 0, i;

	if (!encoder) {
		SDE_ERROR("invalid drm enc\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(encoder);
	sde_kms = sde_encoder_get_kms(&sde_enc->base);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	priv = encoder->dev->dev_private;

	if (!priv->num_connectors) {
		SDE_ERROR_ENC(sde_enc, "No connectors registered\n");
		return -EINVAL;
	}
	SDE_DEBUG_ENC(sde_enc,
			"num of connectors: %d\n", priv->num_connectors);

	SDE_DEBUG_ENC(sde_enc, "enable: %d\n", enable);
	if (!enable) {
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			phys_enc = sde_enc->phys_encs[i];
			if (phys_enc)
				phys_enc->cont_splash_enabled = false;
		}
		return ret;
	}

	if (!splash_display) {
		SDE_ERROR_ENC(sde_enc, "invalid splash data\n");
		return  -EINVAL;
	}

	for (i = 0; i < priv->num_connectors; i++) {
		SDE_DEBUG_ENC(sde_enc, "connector id: %d\n",
				priv->connectors[i]->base.id);
		sde_conn = to_sde_connector(priv->connectors[i]);
		if (!sde_conn->encoder) {
			SDE_DEBUG_ENC(sde_enc,
				"encoder not attached to connector\n");
			continue;
		}
		if (sde_conn->encoder->base.id
				== encoder->base.id) {
			conn = (priv->connectors[i]);
			break;
		}
	}

	if (!conn || !conn->state) {
		SDE_ERROR_ENC(sde_enc, "connector not found\n");
		return -EINVAL;
	}

	sde_conn_state = to_sde_connector_state(conn->state);

	if (!sde_conn->ops.get_mode_info) {
		SDE_ERROR_ENC(sde_enc, "conn: get_mode_info ops not found\n");
		return -EINVAL;
	}

	ret = sde_connector_get_mode_info(&sde_conn->base,
			&encoder->crtc->state->adjusted_mode,
			&sde_conn_state->mode_info);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
			"conn: ->get_mode_info failed. ret=%d\n", ret);
		return ret;
	}

	if (sde_conn->encoder) {
		conn->state->best_encoder = sde_conn->encoder;
		SDE_DEBUG_ENC(sde_enc,
			"configured cstate->best_encoder to ID = %d\n",
			conn->state->best_encoder->base.id);
	} else {
		SDE_ERROR_ENC(sde_enc, "No encoder mapped to connector=%d\n",
				conn->base.id);
	}

	ret = sde_rm_reserve(&sde_kms->rm, encoder, encoder->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
			"failed to reserve hw resources, %d\n", ret);
		return ret;
	}

	SDE_DEBUG_ENC(sde_enc, "connector topology = %llu\n",
			sde_connector_get_topology_name(conn));
	drm_mode = &encoder->crtc->state->adjusted_mode;
	SDE_DEBUG_ENC(sde_enc, "hdisplay = %d, vdisplay = %d\n",
			drm_mode->hdisplay, drm_mode->vdisplay);
	drm_set_preferred_mode(conn, drm_mode->hdisplay, drm_mode->vdisplay);

	if (encoder->bridge) {
		SDE_DEBUG_ENC(sde_enc, "Bridge mapped to encoder\n");
		/*
		 * For cont-splash use case, we update the mode
		 * configurations manually. This will skip the
		 * usually mode set call when actual frame is
		 * pushed from framework. The bridge needs to
		 * be updated with the current drm mode by
		 * calling the bridge mode set ops.
		 */
		if (encoder->bridge->funcs) {
			SDE_DEBUG_ENC(sde_enc, "calling mode_set\n");
			encoder->bridge->funcs->mode_set(encoder->bridge,
						drm_mode, drm_mode);
		}
	} else {
		SDE_ERROR_ENC(sde_enc, "No bridge attached to encoder\n");
	}

	_sde_encoder_cache_hw_res_cont_splash(encoder, sde_kms);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys) {
			SDE_ERROR_ENC(sde_enc,
				"phys encoders not initialized\n");
			return -EINVAL;
		}

		/* update connector for master and slave phys encoders */
		phys->connector = conn;
		phys->cont_splash_enabled = true;

		phys->hw_pp = sde_enc->hw_pp[i];
		if (phys->ops.cont_splash_mode_set)
			phys->ops.cont_splash_mode_set(phys, drm_mode);

		if (phys->ops.is_master && phys->ops.is_master(phys))
			sde_enc->cur_master = phys;
	}

	return ret;
}

int sde_encoder_display_failure_notification(struct drm_encoder *enc,
	bool skip_pre_kickoff)
{
	struct msm_drm_thread *event_thread = NULL;
	struct msm_drm_private *priv = NULL;
	struct sde_encoder_virt *sde_enc = NULL;

	if (!enc || !enc->dev || !enc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	priv = enc->dev->dev_private;
	sde_enc = to_sde_encoder_virt(enc);
	if (!sde_enc->crtc || (sde_enc->crtc->index
			>= ARRAY_SIZE(priv->event_thread))) {
		SDE_DEBUG_ENC(sde_enc,
			"invalid cached CRTC: %d or crtc index: %d\n",
			sde_enc->crtc == NULL,
			sde_enc->crtc ? sde_enc->crtc->index : -EINVAL);
		return -EINVAL;
	}

	SDE_EVT32_VERBOSE(DRMID(enc));

	event_thread = &priv->event_thread[sde_enc->crtc->index];

	if (!skip_pre_kickoff) {
		kthread_queue_work(&event_thread->worker,
				   &sde_enc->esd_trigger_work);
		kthread_flush_work(&sde_enc->esd_trigger_work);
	}

	/*
	 * panel may stop generating te signal (vsync) during esd failure. rsc
	 * hardware may hang without vsync. Avoid rsc hang by generating the
	 * vsync from watchdog timer instead of panel.
	 */
	sde_encoder_helper_switch_vsync(enc, true);

	if (!skip_pre_kickoff)
		sde_encoder_wait_for_event(enc, MSM_ENC_TX_COMPLETE);

	return 0;
}

bool sde_encoder_recovery_events_enabled(struct drm_encoder *encoder)
{
	struct sde_encoder_virt *sde_enc;

	if (!encoder) {
		SDE_ERROR("invalid drm enc\n");
		return false;
	}

	sde_enc = to_sde_encoder_virt(encoder);

	return sde_enc->recovery_events_enabled;
}

void sde_encoder_recovery_events_handler(struct drm_encoder *encoder,
		bool enabled)
{
	struct sde_encoder_virt *sde_enc;

	if (!encoder) {
		SDE_ERROR("invalid drm enc\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(encoder);
	sde_enc->recovery_events_enabled = enabled;
}
