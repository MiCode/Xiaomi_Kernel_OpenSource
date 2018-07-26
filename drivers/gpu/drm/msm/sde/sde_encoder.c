/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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
#include <linux/seq_file.h>
#include <linux/sde_rsc.h>

#include "msm_drv.h"
#include "sde_kms.h"
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_ctl.h"
#include "sde_formats.h"
#include "sde_encoder_phys.h"
#include "sde_power_handle.h"
#include "sde_hw_dsc.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_core_irq.h"

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

/*
 * Two to anticipate panels that can do cmd/vid dynamic switching
 * plan is to create all possible physical encoder types, and switch between
 * them at runtime
 */
#define NUM_PHYS_ENCODER_TYPES 2

#define MAX_PHYS_ENCODERS_PER_VIRTUAL \
	(MAX_H_TILES_PER_DISPLAY * NUM_PHYS_ENCODER_TYPES)

#define MAX_CHANNELS_PER_ENC 2

#define MISR_BUFF_SIZE			256

#define IDLE_SHORT_TIMEOUT	1

#define FAULT_TOLERENCE_DELTA_IN_MS 2

#define FAULT_TOLERENCE_WAIT_IN_MS 5

/* Maximum number of VSYNC wait attempts for RSC state transition */
#define MAX_RSC_WAIT	5

#define TOPOLOGY_DUALPIPE_MERGE_MODE(x) \
		(((x) == SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE) || \
		((x) == SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE) || \
		((x) == SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC))

/**
 * enum sde_enc_rc_events - events for resource control state machine
 * @SDE_ENC_RC_EVENT_KICKOFF:
 *	This event happens at NORMAL priority.
 *	Event that signals the start of the transfer. When this event is
 *	received, enable MDP/DSI core clocks and request RSC with CMD state.
 *	Regardless of the previous state, the resource should be in ON state
 *	at the end of this event.
 * @SDE_ENC_RC_EVENT_FRAME_DONE:
 *	This event happens at INTERRUPT level.
 *	Event signals the end of the data transfer after the PP FRAME_DONE
 *	event. At the end of this event, a delayed work is scheduled to go to
 *	IDLE_PC state after IDLE_POWERCOLLAPSE_DURATION time.
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
	SDE_ENC_RC_EVENT_FRAME_DONE,
	SDE_ENC_RC_EVENT_PRE_STOP,
	SDE_ENC_RC_EVENT_STOP,
	SDE_ENC_RC_EVENT_PRE_MODESET,
	SDE_ENC_RC_EVENT_POST_MODESET,
	SDE_ENC_RC_EVENT_ENTER_IDLE,
	SDE_ENC_RC_EVENT_EARLY_WAKEUP,
};

/*
 * enum sde_enc_rc_states - states that the resource control maintains
 * @SDE_ENC_RC_STATE_OFF: Resource is in OFF state
 * @SDE_ENC_RC_STATE_PRE_OFF: Resource is transitioning to OFF state
 * @SDE_ENC_RC_STATE_ON: Resource is in ON state
 * @SDE_ENC_RC_STATE_MODESET: Resource is in modeset state
 * @SDE_ENC_RC_STATE_IDLE: Resource is in IDLE state
 */
enum sde_enc_rc_states {
	SDE_ENC_RC_STATE_OFF,
	SDE_ENC_RC_STATE_PRE_OFF,
	SDE_ENC_RC_STATE_ON,
	SDE_ENC_RC_STATE_MODESET,
	SDE_ENC_RC_STATE_IDLE
};

/**
 * struct sde_encoder_virt - virtual encoder. Container of one or more physical
 *	encoders. Virtual encoder manages one "logical" display. Physical
 *	encoders manage one intf block, tied to a specific panel/sub-panel.
 *	Virtual encoder defers as much as possible to the physical encoders.
 *	Virtual encoder registers itself with the DRM Framework as the encoder.
 * @base:		drm_encoder base class for registration with DRM
 * @enc_spin_lock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 * @bus_scaling_client:	Client handle to the bus scaling interface
 * @num_phys_encs:	Actual number of physical encoders contained.
 * @phys_encs:		Container of physical encoders managed.
 * @cur_master:		Pointer to the current master in this mode. Optimization
 *			Only valid after enable. Cleared as disable.
 * @hw_pp		Handle to the pingpong blocks used for the display. No.
 *			pingpong blocks can be different than num_phys_encs.
 * @hw_dsc:		Array of DSC block handles used for the display.
 * @intfs_swapped	Whether or not the phys_enc interfaces have been swapped
 *			for partial update right-only cases, such as pingpong
 *			split where virtual pingpong does not generate IRQs
 * @crtc_vblank_cb:	Callback into the upper layer / CRTC for
 *			notification of the VBLANK
 * @crtc_vblank_cb_data:	Data from upper layer for VBLANK notification
 * @crtc_kickoff_cb:		Callback into CRTC that will flush & start
 *				all CTL paths
 * @crtc_kickoff_cb_data:	Opaque user data given to crtc_kickoff_cb
 * @debugfs_root:		Debug file system root file node
 * @enc_lock:			Lock around physical encoder create/destroy and
				access.
 * @frame_busy_mask:		Bitmask tracking which phys_enc we are still
 *				busy processing current command.
 *				Bit0 = phys_encs[0] etc.
 * @crtc_frame_event_cb:	callback handler for frame event
 * @crtc_frame_event_cb_data:	callback handler private data
 * @vsync_event_timer:		vsync timer
 * @rsc_client:			rsc client pointer
 * @rsc_state_init:		boolean to indicate rsc config init
 * @disp_info:			local copy of msm_display_info struct
 * @misr_enable:		misr enable/disable status
 * @misr_frame_count:		misr frame count before start capturing the data
 * @idle_pc_enabled:		indicate if idle power collapse is enabled
 *				currently. This can be controlled by user-mode
 * @rc_lock:			resource control mutex lock to protect
 *				virt encoder over various state changes
 * @rc_state:			resource controller state
 * @delayed_off_work:		delayed worker to schedule disabling of
 *				clks and resources after IDLE_TIMEOUT time.
 * @vsync_event_work:		worker to handle vsync event for autorefresh
 * @input_event_work:		worker to handle input device touch events
 * @esd_trigger_work:		worker to handle esd trigger events
 * @input_handler:			handler for input device events
 * @topology:                   topology of the display
 * @vblank_enabled:		boolean to track userspace vblank vote
 * @idle_pc_restore:		flag to indicate idle_pc_restore happened
 * @rsc_config:			rsc configuration for display vtotal, fps, etc.
 * @cur_conn_roi:		current connector roi
 * @prv_conn_roi:		previous connector roi to optimize if unchanged
 * @crtc			pointer to drm_crtc
 * @recovery_events_enabled:	status of hw recovery feature enable by client
 */
struct sde_encoder_virt {
	struct drm_encoder base;
	spinlock_t enc_spinlock;
	struct mutex vblank_ctl_lock;
	uint32_t bus_scaling_client;

	uint32_t display_num_of_h_tiles;

	unsigned int num_phys_encs;
	struct sde_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *cur_master;
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];
	struct sde_hw_dsc *hw_dsc[MAX_CHANNELS_PER_ENC];

	bool intfs_swapped;

	void (*crtc_vblank_cb)(void *);
	void *crtc_vblank_cb_data;

	struct dentry *debugfs_root;
	struct mutex enc_lock;
	DECLARE_BITMAP(frame_busy_mask, MAX_PHYS_ENCODERS_PER_VIRTUAL);
	void (*crtc_frame_event_cb)(void *, u32 event);
	struct sde_crtc_frame_event_cb_data crtc_frame_event_cb_data;

	struct timer_list vsync_event_timer;

	struct sde_rsc_client *rsc_client;
	bool rsc_state_init;
	struct msm_display_info disp_info;
	bool misr_enable;
	u32 misr_frame_count;

	bool idle_pc_enabled;
	struct mutex rc_lock;
	enum sde_enc_rc_states rc_state;
	struct kthread_delayed_work delayed_off_work;
	struct kthread_work vsync_event_work;
	struct kthread_work input_event_work;
	struct kthread_work esd_trigger_work;
	struct input_handler *input_handler;
	struct msm_display_topology topology;
	bool vblank_enabled;
	bool idle_pc_restore;

	struct sde_rsc_cmd_config rsc_config;
	struct sde_rect cur_conn_roi;
	struct sde_rect prv_conn_roi;
	struct drm_crtc *crtc;

	bool recovery_events_enabled;
};

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

static void _sde_encoder_pm_qos_add_request(struct drm_encoder *drm_enc)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct pm_qos_request *req;
	u32 cpu_mask;
	u32 cpu_dma_latency;
	int cpu;

	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return;
	}

	priv = drm_enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_kms = to_sde_kms(priv->kms);
	if (!sde_kms || !sde_kms->catalog)
		return;

	cpu_mask = sde_kms->catalog->perf.cpu_mask;
	cpu_dma_latency = sde_kms->catalog->perf.cpu_dma_latency;
	if (!cpu_mask)
		return;

	req = &sde_kms->pm_qos_cpu_req;
	req->type = PM_QOS_REQ_AFFINE_CORES;
	cpumask_empty(&req->cpus_affine);
	for_each_possible_cpu(cpu) {
		if ((1 << cpu) & cpu_mask)
			cpumask_set_cpu(cpu, &req->cpus_affine);
	}
	pm_qos_add_request(req, PM_QOS_CPU_DMA_LATENCY, cpu_dma_latency);

	SDE_EVT32_VERBOSE(DRMID(drm_enc), cpu_mask, cpu_dma_latency);
}

static void _sde_encoder_pm_qos_remove_request(struct drm_encoder *drm_enc)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return;
	}

	priv = drm_enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_kms = to_sde_kms(priv->kms);
	if (!sde_kms || !sde_kms->catalog || !sde_kms->catalog->perf.cpu_mask)
		return;

	pm_qos_remove_request(&sde_kms->pm_qos_cpu_req);
}

static struct drm_connector_state *_sde_encoder_get_conn_state(
		struct drm_encoder *drm_enc)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct list_head *connector_list;
	struct drm_connector *conn_iter;

	if (!drm_enc) {
		SDE_ERROR("invalid argument\n");
		return NULL;
	}

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	connector_list = &sde_kms->dev->mode_config.connector_list;

	list_for_each_entry(conn_iter, connector_list, head)
		if (conn_iter->encoder == drm_enc)
			return conn_iter->state;

	return NULL;
}

static int _sde_encoder_get_mode_info(struct drm_encoder *drm_enc,
		struct msm_mode_info *mode_info)
{
	struct drm_connector_state *conn_state;

	if (!drm_enc || !mode_info) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	conn_state = _sde_encoder_get_conn_state(drm_enc);
	if (!conn_state) {
		SDE_ERROR("invalid connector state for the encoder: %d\n",
			drm_enc->base.id);
		return -EINVAL;
	}

	return sde_connector_get_mode_info(conn_state, mode_info);
}

static bool _sde_encoder_is_dsc_enabled(struct drm_encoder *drm_enc)
{
	struct msm_compression_info *comp_info;
	struct msm_mode_info mode_info;
	int rc = 0;

	if (!drm_enc)
		return false;

	rc = _sde_encoder_get_mode_info(drm_enc, &mode_info);
	if (rc) {
		SDE_ERROR("failed to get mode info, enc: %d\n",
			drm_enc->base.id);
		return false;
	}

	comp_info = &mode_info.comp_info;

	return (comp_info->comp_type == MSM_DISPLAY_COMPRESSION_DSC);
}

bool sde_encoder_is_dsc_merge(struct drm_encoder *drm_enc)
{
	enum sde_rm_topology_name topology;
	struct sde_encoder_virt *sde_enc;
	struct drm_connector *drm_conn;

	if (!drm_enc)
		return false;

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->cur_master)
		return false;

	drm_conn = sde_enc->cur_master->connector;
	if (!drm_conn)
		return false;

	topology = sde_connector_get_topology_name(drm_conn);
	if (topology == SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE)
		return true;

	return false;
}

int sde_encoder_in_clone_mode(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc && sde_enc->cur_master &&
		sde_enc->cur_master->in_clone_mode;
}

static inline int _sde_encoder_power_enable(struct sde_encoder_virt *sde_enc,
								bool enable)
{
	struct drm_encoder *drm_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!sde_enc) {
		SDE_ERROR("invalid sde enc\n");
		return -EINVAL;
	}

	drm_enc = &sde_enc->base;
	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}

	priv = drm_enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);

	return sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
									enable);
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
	int ret;

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

	ret = sde_encoder_helper_wait_event_timeout(
			DRMID(phys_enc->parent),
			irq->hw_idx,
			wait_info);

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
	struct msm_mode_info mode_info;
	int rc, i = 0;

	if (!hw_res || !drm_enc || !conn_state) {
		SDE_ERROR("invalid argument(s), drm_enc %d, res %d, state %d\n",
				drm_enc != 0, hw_res != 0, conn_state != 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	/* Query resources used by phys encs, expected to be without overlap */
	memset(hw_res, 0, sizeof(*hw_res));
	hw_res->display_num_of_h_tiles = sde_enc->display_num_of_h_tiles;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.get_hw_resources)
			phys->ops.get_hw_resources(phys, hw_res, conn_state);
	}

	/**
	 * NOTE: Do not use sde_encoder_get_mode_info here as this function is
	 * called from atomic_check phase. Use the below API to get mode
	 * information of the temporary conn_state passed.
	 */
	rc = sde_connector_get_mode_info(conn_state, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return;
	}

	hw_res->topology = mode_info.topology;
	hw_res->is_primary = sde_enc->disp_info.is_primary;
}

void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	mutex_lock(&sde_enc->enc_lock);
	sde_rsc_client_destroy(sde_enc->rsc_client);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

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

	if (sde_enc->input_handler) {
		kfree(sde_enc->input_handler);
		sde_enc->input_handler = NULL;
	}

	kfree(sde_enc);
}

void sde_encoder_helper_update_intf_cfg(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_hw_intf_cfg_v1 *intf_cfg;
	enum sde_3d_blend_mode mode_3d;

	if (!phys_enc) {
		SDE_ERROR("invalid arg, encoder %d\n", phys_enc != 0);
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
	struct split_pipe_cfg cfg = { 0 };
	struct sde_hw_mdp *hw_mdptop;
	enum sde_rm_topology_name topology;
	struct msm_display_info *disp_info;

	if (!phys_enc || !phys_enc->hw_mdptop || !phys_enc->parent) {
		SDE_ERROR("invalid arg(s), encoder %d\n", phys_enc != 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	hw_mdptop = phys_enc->hw_mdptop;
	disp_info = &sde_enc->disp_info;

	if (disp_info->intf_type != DRM_MODE_CONNECTOR_DSI)
		return;

	/**
	 * disable split modes since encoder will be operating in as the only
	 * encoder, either for the entire use case in the case of, for example,
	 * single DSI, or for this frame in the case of left/right only partial
	 * update.
	 */
	if (phys_enc->split_role == ENC_ROLE_SOLO) {
		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
		if (hw_mdptop->ops.setup_pp_split)
			hw_mdptop->ops.setup_pp_split(hw_mdptop, &cfg);
		return;
	}

	cfg.en = true;
	cfg.mode = phys_enc->intf_mode;
	cfg.intf = interface;

	if (cfg.en && phys_enc->ops.needs_single_flush &&
			phys_enc->ops.needs_single_flush(phys_enc))
		cfg.split_flush_en = true;

	topology = sde_connector_get_topology_name(phys_enc->connector);
	if (topology == SDE_RM_TOPOLOGY_PPSPLIT)
		cfg.pp_split_slave = cfg.intf;
	else
		cfg.pp_split_slave = INTF_MAX;

	if (phys_enc->split_role == ENC_ROLE_MASTER) {
		SDE_DEBUG_ENC(sde_enc, "enable %d\n", cfg.en);

		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
	} else if (sde_enc->hw_pp[0]) {
		/*
		 * slave encoder
		 * - determine split index from master index,
		 *   assume master is first pp
		 */
		cfg.pp_split_index = sde_enc->hw_pp[0]->idx - PINGPONG_0;
		SDE_DEBUG_ENC(sde_enc, "master using pp%d\n",
				cfg.pp_split_index);

		if (hw_mdptop->ops.setup_pp_split)
			hw_mdptop->ops.setup_pp_split(hw_mdptop, &cfg);
	}
}

static int sde_encoder_virt_atomic_check(
		struct drm_encoder *drm_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	const struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	struct sde_connector *sde_conn = NULL;
	struct sde_connector_state *sde_conn_state = NULL;
	struct sde_crtc_state *sde_crtc_state = NULL;
	int i = 0;
	int ret = 0;

	if (!drm_enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arg(s), drm_enc %d, crtc/conn state %d/%d\n",
				drm_enc != 0, crtc_state != 0, conn_state != 0);
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;
	sde_conn = to_sde_connector(conn_state->connector);
	sde_conn_state = to_sde_connector_state(conn_state);
	sde_crtc_state = to_sde_crtc_state(crtc_state);

	SDE_EVT32(DRMID(drm_enc), drm_atomic_crtc_needs_modeset(crtc_state));

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

	if (!ret && drm_atomic_crtc_needs_modeset(crtc_state)) {
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

		if (ret)
			return ret;
	}

	if (!ret) {
		/**
		 * record topology in previous atomic state to be able to handle
		 * topology transitions correctly.
		 */
		enum sde_rm_topology_name old_top;

		old_top  = sde_connector_get_property(conn_state,
				CONNECTOR_PROP_TOPOLOGY_NAME);
		ret = sde_connector_set_old_topology_name(conn_state, old_top);
		if (ret)
			return ret;
	}

	if (!ret && sde_conn && drm_atomic_crtc_needs_modeset(crtc_state)) {
		struct msm_display_topology *topology = NULL;

		ret = sde_conn->ops.get_mode_info(&sde_conn->base, adj_mode,
				&sde_conn_state->mode_info,
				sde_kms->catalog->max_mixer_width,
				sde_conn->display);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"failed to get mode info, rc = %d\n", ret);
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

		ret = sde_rm_update_topology(conn_state, topology);
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

	ret = sde_connector_roi_v1_check_roi(conn_state);
	if (ret) {
		SDE_ERROR_ENC(sde_enc, "connector roi check failed, rc: %d",
				ret);
		return ret;
	}

	if (!ret)
		drm_mode_set_crtcinfo(adj_mode, 0);

	SDE_EVT32(DRMID(drm_enc), adj_mode->flags, adj_mode->private_flags);

	return ret;
}

static int _sde_encoder_dsc_update_pic_dim(struct msm_display_dsc_info *dsc,
		int pic_width, int pic_height)
{
	if (!dsc || !pic_width || !pic_height) {
		SDE_ERROR("invalid input: pic_width=%d pic_height=%d\n",
			pic_width, pic_height);
		return -EINVAL;
	}

	if ((pic_width % dsc->slice_width) ||
	    (pic_height % dsc->slice_height)) {
		SDE_ERROR("pic_dim=%dx%d has to be multiple of slice=%dx%d\n",
			pic_width, pic_height,
			dsc->slice_width, dsc->slice_height);
		return -EINVAL;
	}

	dsc->pic_width = pic_width;
	dsc->pic_height = pic_height;

	return 0;
}

static void _sde_encoder_dsc_pclk_param_calc(struct msm_display_dsc_info *dsc,
		int intf_width)
{
	int slice_per_pkt, slice_per_intf;
	int bytes_in_slice, total_bytes_per_intf;

	if (!dsc || !dsc->slice_width || !dsc->slice_per_pkt ||
	    (intf_width < dsc->slice_width)) {
		SDE_ERROR("invalid input: intf_width=%d slice_width=%d\n",
			intf_width, dsc ? dsc->slice_width : -1);
		return;
	}

	slice_per_pkt = dsc->slice_per_pkt;
	slice_per_intf = DIV_ROUND_UP(intf_width, dsc->slice_width);

	/*
	 * If slice_per_pkt is greater than slice_per_intf then default to 1.
	 * This can happen during partial update.
	 */
	if (slice_per_pkt > slice_per_intf)
		slice_per_pkt = 1;

	bytes_in_slice = DIV_ROUND_UP(dsc->slice_width * dsc->bpp, 8);
	total_bytes_per_intf = bytes_in_slice * slice_per_intf;

	dsc->eol_byte_num = total_bytes_per_intf % 3;
	dsc->pclk_per_line =  DIV_ROUND_UP(total_bytes_per_intf, 3);
	dsc->bytes_in_slice = bytes_in_slice;
	dsc->bytes_per_pkt = bytes_in_slice * slice_per_pkt;
	dsc->pkt_per_line = slice_per_intf / slice_per_pkt;
}

static int _sde_encoder_dsc_initial_line_calc(struct msm_display_dsc_info *dsc,
		int enc_ip_width)
{
	int ssm_delay, total_pixels, soft_slice_per_enc;

	soft_slice_per_enc = enc_ip_width / dsc->slice_width;

	/*
	 * minimum number of initial line pixels is a sum of:
	 * 1. sub-stream multiplexer delay (83 groups for 8bpc,
	 *    91 for 10 bpc) * 3
	 * 2. for two soft slice cases, add extra sub-stream multiplexer * 3
	 * 3. the initial xmit delay
	 * 4. total pipeline delay through the "lock step" of encoder (47)
	 * 5. 6 additional pixels as the output of the rate buffer is
	 *    48 bits wide
	 */
	ssm_delay = ((dsc->bpc < 10) ? 84 : 92);
	total_pixels = ssm_delay * 3 + dsc->initial_xmit_delay + 47;
	if (soft_slice_per_enc > 1)
		total_pixels += (ssm_delay * 3);
	dsc->initial_lines = DIV_ROUND_UP(total_pixels, dsc->slice_width);
	return 0;
}

static bool _sde_encoder_dsc_ich_reset_override_needed(bool pu_en,
		struct msm_display_dsc_info *dsc)
{
	/*
	 * As per the DSC spec, ICH_RESET can be either end of the slice line
	 * or at the end of the slice. HW internally generates ich_reset at
	 * end of the slice line if DSC_MERGE is used or encoder has two
	 * soft slices. However, if encoder has only 1 soft slice and DSC_MERGE
	 * is not used then it will generate ich_reset at the end of slice.
	 *
	 * Now as per the spec, during one PPS session, position where
	 * ich_reset is generated should not change. Now if full-screen frame
	 * has more than 1 soft slice then HW will automatically generate
	 * ich_reset at the end of slice_line. But for the same panel, if
	 * partial frame is enabled and only 1 encoder is used with 1 slice,
	 * then HW will generate ich_reset at end of the slice. This is a
	 * mismatch. Prevent this by overriding HW's decision.
	 */
	return pu_en && dsc && (dsc->full_frame_slices > 1) &&
		(dsc->slice_width == dsc->pic_width);
}

static void _sde_encoder_dsc_pipe_cfg(struct sde_hw_dsc *hw_dsc,
		struct sde_hw_pingpong *hw_pp, struct msm_display_dsc_info *dsc,
		u32 common_mode, bool ich_reset, bool enable)
{
	if (!enable) {
		if (hw_pp->ops.disable_dsc)
			hw_pp->ops.disable_dsc(hw_pp);
		return;
	}

	if (hw_dsc->ops.dsc_config)
		hw_dsc->ops.dsc_config(hw_dsc, dsc, common_mode, ich_reset);

	if (hw_dsc->ops.dsc_config_thresh)
		hw_dsc->ops.dsc_config_thresh(hw_dsc, dsc);

	if (hw_pp->ops.setup_dsc)
		hw_pp->ops.setup_dsc(hw_pp);

	if (hw_dsc->ops.bind_pingpong_blk)
		hw_dsc->ops.bind_pingpong_blk(hw_dsc, true, hw_pp->idx);

	if (hw_pp->ops.enable_dsc)
		hw_pp->ops.enable_dsc(hw_pp);
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

static int _sde_encoder_dsc_n_lm_1_enc_1_intf(struct sde_encoder_virt *sde_enc)
{
	int this_frame_slices;
	int intf_ip_w, enc_ip_w;
	int ich_res, dsc_common_mode = 0;

	struct sde_hw_pingpong *hw_pp = sde_enc->hw_pp[0];
	struct sde_hw_dsc *hw_dsc = sde_enc->hw_dsc[0];
	struct sde_encoder_phys *enc_master = sde_enc->cur_master;
	const struct sde_rect *roi = &sde_enc->cur_conn_roi;
	struct msm_mode_info mode_info;
	struct msm_display_dsc_info *dsc = NULL;
	struct sde_hw_ctl *hw_ctl;
	struct sde_ctl_dsc_cfg cfg;
	int rc;

	if (hw_dsc == NULL || hw_pp == NULL || !enc_master) {
		SDE_ERROR_ENC(sde_enc, "invalid params for DSC\n");
		return -EINVAL;
	}

	rc = _sde_encoder_get_mode_info(&sde_enc->base, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return -EINVAL;
	}

	hw_ctl = enc_master->hw_ctl;

	memset(&cfg, 0, sizeof(cfg));
	dsc = &mode_info.comp_info.dsc_info;
	_sde_encoder_dsc_update_pic_dim(dsc, roi->w, roi->h);

	this_frame_slices = roi->w / dsc->slice_width;
	intf_ip_w = this_frame_slices * dsc->slice_width;
	_sde_encoder_dsc_pclk_param_calc(dsc, intf_ip_w);

	enc_ip_w = intf_ip_w;
	_sde_encoder_dsc_initial_line_calc(dsc, enc_ip_w);

	ich_res = _sde_encoder_dsc_ich_reset_override_needed(false, dsc);

	if (enc_master->intf_mode == INTF_MODE_VIDEO)
		dsc_common_mode = DSC_MODE_VIDEO;

	SDE_DEBUG_ENC(sde_enc, "pic_w: %d pic_h: %d mode:%d\n",
		roi->w, roi->h, dsc_common_mode);
	SDE_EVT32(DRMID(&sde_enc->base), roi->w, roi->h, dsc_common_mode);

	_sde_encoder_dsc_pipe_cfg(hw_dsc, hw_pp, dsc, dsc_common_mode,
			ich_res, true);
	if (cfg.dsc_count >= MAX_DSC_PER_CTL_V1) {
		pr_err("Invalid dsc count:%d\n", cfg.dsc_count);
		return -EINVAL;
	}
	cfg.dsc[cfg.dsc_count++] = hw_dsc->idx;

	/* setup dsc active configuration in the control path */
	if (hw_ctl->ops.setup_dsc_cfg) {
		hw_ctl->ops.setup_dsc_cfg(hw_ctl, &cfg);
		SDE_DEBUG_ENC(sde_enc,
				"setup dsc_cfg hw_ctl[%d], count:%d,dsc[0]:%d, dsc[1]:%d\n",
				hw_ctl->idx,
				cfg.dsc_count,
				cfg.dsc[0],
				cfg.dsc[1]);
	}

	if (hw_ctl->ops.update_bitmask_dsc)
		hw_ctl->ops.update_bitmask_dsc(hw_ctl, hw_dsc->idx, 1);

	return 0;
}

static int _sde_encoder_dsc_2_lm_2_enc_2_intf(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	int this_frame_slices;
	int intf_ip_w, enc_ip_w;
	int ich_res, dsc_common_mode;

	struct sde_encoder_phys *enc_master = sde_enc->cur_master;
	const struct sde_rect *roi = &sde_enc->cur_conn_roi;
	struct sde_hw_dsc *hw_dsc[MAX_CHANNELS_PER_ENC];
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];
	struct msm_display_dsc_info dsc[MAX_CHANNELS_PER_ENC];
	struct msm_mode_info mode_info;
	bool half_panel_partial_update;
	struct sde_hw_ctl *hw_ctl = enc_master->hw_ctl;
	struct sde_ctl_dsc_cfg cfg;
	int i, rc;

	memset(&cfg, 0, sizeof(cfg));

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		hw_pp[i] = sde_enc->hw_pp[i];
		hw_dsc[i] = sde_enc->hw_dsc[i];

		if (!hw_pp[i] || !hw_dsc[i]) {
			SDE_ERROR_ENC(sde_enc, "invalid params for DSC\n");
			return -EINVAL;
		}
	}

	rc = _sde_encoder_get_mode_info(&sde_enc->base, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return -EINVAL;
	}

	half_panel_partial_update =
			hweight_long(params->affected_displays) == 1;

	dsc_common_mode = 0;
	if (!half_panel_partial_update)
		dsc_common_mode |= DSC_MODE_SPLIT_PANEL;
	if (enc_master->intf_mode == INTF_MODE_VIDEO)
		dsc_common_mode |= DSC_MODE_VIDEO;

	memcpy(&dsc[0], &mode_info.comp_info.dsc_info, sizeof(dsc[0]));
	memcpy(&dsc[1], &mode_info.comp_info.dsc_info, sizeof(dsc[1]));

	/*
	 * Since both DSC use same pic dimension, set same pic dimension
	 * to both DSC structures.
	 */
	_sde_encoder_dsc_update_pic_dim(&dsc[0], roi->w, roi->h);
	_sde_encoder_dsc_update_pic_dim(&dsc[1], roi->w, roi->h);

	this_frame_slices = roi->w / dsc[0].slice_width;
	intf_ip_w = this_frame_slices * dsc[0].slice_width;

	if (!half_panel_partial_update)
		intf_ip_w /= 2;

	/*
	 * In this topology when both interfaces are active, they have same
	 * load so intf_ip_w will be same.
	 */
	_sde_encoder_dsc_pclk_param_calc(&dsc[0], intf_ip_w);
	_sde_encoder_dsc_pclk_param_calc(&dsc[1], intf_ip_w);

	/*
	 * In this topology, since there is no dsc_merge, uncompressed input
	 * to encoder and interface is same.
	 */
	enc_ip_w = intf_ip_w;
	_sde_encoder_dsc_initial_line_calc(&dsc[0], enc_ip_w);
	_sde_encoder_dsc_initial_line_calc(&dsc[1], enc_ip_w);

	/*
	 * __is_ich_reset_override_needed should be called only after
	 * updating pic dimension, mdss_panel_dsc_update_pic_dim.
	 */
	ich_res = _sde_encoder_dsc_ich_reset_override_needed(
			half_panel_partial_update, &dsc[0]);

	SDE_DEBUG_ENC(sde_enc, "pic_w: %d pic_h: %d mode:%d\n",
			roi->w, roi->h, dsc_common_mode);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		bool active = !!((1 << i) & params->affected_displays);

		SDE_EVT32(DRMID(&sde_enc->base), roi->w, roi->h,
				dsc_common_mode, i, active);
		_sde_encoder_dsc_pipe_cfg(hw_dsc[i], hw_pp[i], &dsc[i],
				dsc_common_mode, ich_res, active);

		if (active) {
			if (cfg.dsc_count >= MAX_DSC_PER_CTL_V1) {
				pr_err("Invalid dsc count:%d\n",
						cfg.dsc_count);
				return -EINVAL;
			}
			cfg.dsc[i] = hw_dsc[i]->idx;
			cfg.dsc_count++;

			if (hw_ctl->ops.update_bitmask_dsc)
				hw_ctl->ops.update_bitmask_dsc(hw_ctl,
						hw_dsc[i]->idx, 1);
		}
	}

	/* setup dsc active configuration in the control path */
	if (hw_ctl->ops.setup_dsc_cfg) {
		hw_ctl->ops.setup_dsc_cfg(hw_ctl, &cfg);
		SDE_DEBUG_ENC(sde_enc,
				"setup dsc_cfg hw_ctl[%d], count:%d,dsc[0]:%d, dsc[1]:%d\n",
				hw_ctl->idx,
				cfg.dsc_count,
				cfg.dsc[0],
				cfg.dsc[1]);
	}
	return 0;
}

static int _sde_encoder_dsc_2_lm_2_enc_1_intf(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	int this_frame_slices;
	int intf_ip_w, enc_ip_w;
	int ich_res, dsc_common_mode;

	struct sde_encoder_phys *enc_master = sde_enc->cur_master;
	const struct sde_rect *roi = &sde_enc->cur_conn_roi;
	struct sde_hw_dsc *hw_dsc[MAX_CHANNELS_PER_ENC];
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];
	struct msm_display_dsc_info *dsc = NULL;
	struct msm_mode_info mode_info;
	bool half_panel_partial_update;
	struct sde_hw_ctl *hw_ctl = enc_master->hw_ctl;
	struct sde_ctl_dsc_cfg cfg;
	int i, rc;

	memset(&cfg, 0, sizeof(cfg));

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		hw_pp[i] = sde_enc->hw_pp[i];
		hw_dsc[i] = sde_enc->hw_dsc[i];

		if (!hw_pp[i] || !hw_dsc[i]) {
			SDE_ERROR_ENC(sde_enc, "invalid params for DSC\n");
			return -EINVAL;
		}
	}

	rc = _sde_encoder_get_mode_info(&sde_enc->base, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return -EINVAL;
	}

	dsc = &mode_info.comp_info.dsc_info;

	half_panel_partial_update =
			hweight_long(params->affected_displays) == 1;

	dsc_common_mode = 0;
	if (!half_panel_partial_update)
		dsc_common_mode |= DSC_MODE_SPLIT_PANEL | DSC_MODE_MULTIPLEX;
	if (enc_master->intf_mode == INTF_MODE_VIDEO)
		dsc_common_mode |= DSC_MODE_VIDEO;

	_sde_encoder_dsc_update_pic_dim(dsc, roi->w, roi->h);

	this_frame_slices = roi->w / dsc->slice_width;
	intf_ip_w = this_frame_slices * dsc->slice_width;
	_sde_encoder_dsc_pclk_param_calc(dsc, intf_ip_w);

	/*
	 * dsc merge case: when using 2 encoders for the same stream,
	 * no. of slices need to be same on both the encoders.
	 */
	enc_ip_w = intf_ip_w / 2;
	_sde_encoder_dsc_initial_line_calc(dsc, enc_ip_w);

	ich_res = _sde_encoder_dsc_ich_reset_override_needed(
			half_panel_partial_update, dsc);

	SDE_DEBUG_ENC(sde_enc, "pic_w: %d pic_h: %d mode:%d\n",
			roi->w, roi->h, dsc_common_mode);
	SDE_EVT32(DRMID(&sde_enc->base), roi->w, roi->h,
			dsc_common_mode, i, params->affected_displays);

	_sde_encoder_dsc_pipe_cfg(hw_dsc[0], hw_pp[0], dsc, dsc_common_mode,
			ich_res, true);
	cfg.dsc[0] = hw_dsc[0]->idx;
	cfg.dsc_count++;
	if (hw_ctl->ops.update_bitmask_dsc)
		hw_ctl->ops.update_bitmask_dsc(hw_ctl, hw_dsc[0]->idx, 1);


	_sde_encoder_dsc_pipe_cfg(hw_dsc[1], hw_pp[1], dsc, dsc_common_mode,
			ich_res, !half_panel_partial_update);
	if (!half_panel_partial_update) {
		cfg.dsc[1] = hw_dsc[1]->idx;
		cfg.dsc_count++;
		if (hw_ctl->ops.update_bitmask_dsc)
			hw_ctl->ops.update_bitmask_dsc(hw_ctl, hw_dsc[1]->idx,
					1);
	}
	/* setup dsc active configuration in the control path */
	if (hw_ctl->ops.setup_dsc_cfg) {
		hw_ctl->ops.setup_dsc_cfg(hw_ctl, &cfg);
		SDE_DEBUG_ENC(sde_enc,
				"setup_dsc_cfg hw_ctl[%d], count:%d,dsc[0]:%d, dsc[1]:%d\n",
				hw_ctl->idx,
				cfg.dsc_count,
				cfg.dsc[0],
				cfg.dsc[1]);
	}
	return 0;
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

static int _sde_encoder_dsc_setup(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	enum sde_rm_topology_name topology;
	struct drm_connector *drm_conn;
	int ret = 0;

	if (!sde_enc || !params || !sde_enc->phys_encs[0] ||
			!sde_enc->phys_encs[0]->connector)
		return -EINVAL;

	drm_conn = sde_enc->phys_encs[0]->connector;

	topology = sde_connector_get_topology_name(drm_conn);
	if (topology == SDE_RM_TOPOLOGY_NONE) {
		SDE_ERROR_ENC(sde_enc, "topology not set yet\n");
		return -EINVAL;
	}

	SDE_DEBUG_ENC(sde_enc, "topology:%d\n", topology);
	SDE_EVT32(DRMID(&sde_enc->base), topology,
			sde_enc->cur_conn_roi.x,
			sde_enc->cur_conn_roi.y,
			sde_enc->cur_conn_roi.w,
			sde_enc->cur_conn_roi.h,
			sde_enc->prv_conn_roi.x,
			sde_enc->prv_conn_roi.y,
			sde_enc->prv_conn_roi.w,
			sde_enc->prv_conn_roi.h,
			sde_enc->cur_master->cached_mode.hdisplay,
			sde_enc->cur_master->cached_mode.vdisplay);

	if (sde_kms_rect_is_equal(&sde_enc->cur_conn_roi,
			&sde_enc->prv_conn_roi))
		return ret;

	switch (topology) {
	case SDE_RM_TOPOLOGY_SINGLEPIPE_DSC:
	case SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC:
		ret = _sde_encoder_dsc_n_lm_1_enc_1_intf(sde_enc);
		break;
	case SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE:
		ret = _sde_encoder_dsc_2_lm_2_enc_1_intf(sde_enc, params);
		break;
	case SDE_RM_TOPOLOGY_DUALPIPE_DSC:
		ret = _sde_encoder_dsc_2_lm_2_enc_2_intf(sde_enc, params);
		break;
	default:
		SDE_ERROR_ENC(sde_enc, "No DSC support for topology %d",
				topology);
		return -EINVAL;
	};

	return ret;
}

void sde_encoder_helper_vsync_config(struct sde_encoder_phys *phys_enc,
			u32 vsync_source, bool is_dummy)
{
	struct sde_vsync_source_cfg vsync_cfg = { 0 };
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_hw_mdp *hw_mdptop;
	struct drm_encoder *drm_enc;
	struct msm_mode_info mode_info;
	struct sde_encoder_virt *sde_enc;
	int i, rc = 0;

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

	drm_enc = &sde_enc->base;
	/* this pointers are checked in virt_enable_helper */
	priv = drm_enc->dev->dev_private;

	sde_kms = to_sde_kms(priv->kms);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	}

	hw_mdptop = sde_kms->hw_mdp;
	if (!hw_mdptop) {
		SDE_ERROR("invalid mdptop\n");
		return;
	}

	rc = _sde_encoder_get_mode_info(drm_enc, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return;
	}

	if (hw_mdptop->ops.setup_vsync_source) {
		for (i = 0; i < sde_enc->num_phys_encs; i++)
			vsync_cfg.ppnumber[i] = sde_enc->hw_pp[i]->idx;

		vsync_cfg.pp_count = sde_enc->num_phys_encs;
		vsync_cfg.frame_rate = mode_info.frame_rate;
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

	if (disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) {

		if (is_dummy)
			vsync_source = SDE_VSYNC_SOURCE_WD_TIMER_1;
		else if (disp_info->is_te_using_watchdog_timer)
			vsync_source = SDE_VSYNC_SOURCE_WD_TIMER_0;
		else
			vsync_source =
				sde_enc->cur_master->hw_pp->caps->te_source;

		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			phys = sde_enc->phys_encs[i];

			if (phys && phys->ops.setup_vsync_source)
				phys->ops.setup_vsync_source(phys,
					vsync_source, is_dummy);
		}
	}
}

static int _sde_encoder_dsc_disable(struct sde_encoder_virt *sde_enc)
{
	int i, ret = 0;
	struct sde_hw_pingpong *hw_pp = NULL;
	struct sde_hw_dsc *hw_dsc = NULL;

	if (!sde_enc || !sde_enc->phys_encs[0] ||
			!sde_enc->phys_encs[0]->connector) {
		SDE_ERROR("invalid params %d %d\n",
			!sde_enc, sde_enc ? !sde_enc->phys_encs[0] : -1);
		return -EINVAL;
	}

	/* Disable DSC for all the pp's present in this topology */
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		hw_pp = sde_enc->hw_pp[i];
		hw_dsc = sde_enc->hw_dsc[i];

		if (hw_pp && hw_pp->ops.disable_dsc)
			hw_pp->ops.disable_dsc(hw_pp);

		if (hw_dsc && hw_dsc->ops.dsc_disable)
			hw_dsc->ops.dsc_disable(hw_dsc);
	}

	return ret;
}

static int _sde_encoder_switch_to_watchdog_vsync(struct drm_encoder *drm_enc)
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
	disp_info.is_te_using_watchdog_timer = true;
	_sde_encoder_update_vsync_source(sde_enc, &disp_info, false);

	sde_encoder_control_te(drm_enc, true);

	return 0;
}

static int _sde_encoder_update_rsc_client(
		struct drm_encoder *drm_enc,
		struct sde_encoder_rsc_config *config, bool enable)
{
	struct sde_encoder_virt *sde_enc;
	struct drm_crtc *crtc;
	enum sde_rsc_state rsc_state;
	struct sde_rsc_cmd_config *rsc_config;
	int ret, prefill_lines;
	struct msm_display_info *disp_info;
	struct msm_mode_info mode_info;
	int wait_vblank_crtc_id = SDE_RSC_INVALID_CRTC_ID;
	int wait_count = 0;
	struct drm_crtc *primary_crtc;
	int pipe = -1;
	int rc = 0;
	int wait_refcount, i;
	struct sde_encoder_phys *phys;
	u32 qsync_mode = 0;

	if (!drm_enc || !drm_enc->dev) {
		SDE_ERROR("invalid encoder arguments\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
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

	rc = _sde_encoder_get_mode_info(drm_enc, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to mode info\n");
		return 0;
	}

	/**
	 * only primary command mode panel without Qsync can request CMD state.
	 * all other panels/displays can request for VID state including
	 * secondary command mode panel.
	 * Clone mode encoder can request CLK STATE only.
	 */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];

		if (phys) {
			qsync_mode = sde_connector_get_property(
					phys->connector->state,
					CONNECTOR_PROP_QSYNC_MODE);
			break;
		}
	}

	if (sde_encoder_in_clone_mode(drm_enc))
		rsc_state = enable ? SDE_RSC_CLK_STATE : SDE_RSC_IDLE_STATE;
	else
		rsc_state = enable ?
			(((disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE)
			  && disp_info->is_primary && !qsync_mode) ?
			 SDE_RSC_CMD_STATE : SDE_RSC_VID_STATE) :
			SDE_RSC_IDLE_STATE;

	SDE_EVT32(rsc_state, qsync_mode);

	prefill_lines = config ? mode_info.prefill_lines +
		config->inline_rotate_prefill : mode_info.prefill_lines;

	/* compare specific items and reconfigure the rsc */
	if ((rsc_config->fps != mode_info.frame_rate) ||
	    (rsc_config->vtotal != mode_info.vtotal) ||
	    (rsc_config->prefill_lines != prefill_lines) ||
	    (rsc_config->jitter_numer != mode_info.jitter_numer) ||
	    (rsc_config->jitter_denom != mode_info.jitter_denom)) {
		rsc_config->fps = mode_info.frame_rate;
		rsc_config->vtotal = mode_info.vtotal;
		rsc_config->prefill_lines = prefill_lines;
		rsc_config->jitter_numer = mode_info.jitter_numer;
		rsc_config->jitter_denom = mode_info.jitter_denom;
		sde_enc->rsc_state_init = false;
	}

	if (rsc_state != SDE_RSC_IDLE_STATE && !sde_enc->rsc_state_init
					&& disp_info->is_primary) {
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

	if (wait_vblank_crtc_id)
		wait_refcount =
			sde_rsc_client_get_vsync_refcount(sde_enc->rsc_client);
	SDE_EVT32_VERBOSE(DRMID(drm_enc), wait_vblank_crtc_id, wait_refcount,
			SDE_EVTLOG_FUNC_ENTRY);

	if (crtc->base.id != wait_vblank_crtc_id) {
		primary_crtc = drm_crtc_find(drm_enc->dev, wait_vblank_crtc_id);
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
				_sde_encoder_switch_to_watchdog_vsync(drm_enc);
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

static void _sde_encoder_irq_control(struct drm_encoder *drm_enc, bool enable)
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

static void _sde_encoder_resource_control_rsc_update(
		struct drm_encoder *drm_enc, bool enable)
{
	struct sde_encoder_rsc_config rsc_cfg = { 0 };
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder argument\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->crtc) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	if (enable) {
		rsc_cfg.inline_rotate_prefill =
				sde_crtc_get_inline_prefill(sde_enc->crtc);

		_sde_encoder_update_rsc_client(drm_enc, &rsc_cfg, true);
	} else {
		_sde_encoder_update_rsc_client(drm_enc, NULL, false);
	}
}

static int _sde_encoder_resource_control_helper(struct drm_encoder *drm_enc,
		bool enable)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_encoder_virt *sde_enc;
	int rc;
	bool is_cmd_mode, is_primary;

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	is_cmd_mode = sde_enc->disp_info.capabilities &
			MSM_DISPLAY_CAP_CMD_MODE;
	is_primary = sde_enc->disp_info.is_primary;

	SDE_DEBUG_ENC(sde_enc, "enable:%d\n", enable);
	SDE_EVT32(DRMID(drm_enc), enable);

	if (!sde_enc->cur_master) {
		SDE_ERROR("encoder master not set\n");
		return -EINVAL;
	}

	if (enable) {
		/* enable SDE core clks */
		rc = sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, true);
		if (rc) {
			SDE_ERROR("failed to enable power resource %d\n", rc);
			SDE_EVT32(rc, SDE_EVTLOG_ERROR);
			return rc;
		}

		/* enable DSI clks */
		rc = sde_connector_clk_ctrl(sde_enc->cur_master->connector,
				true);
		if (rc) {
			SDE_ERROR("failed to enable clk control %d\n", rc);
			sde_power_resource_enable(&priv->phandle,
					sde_kms->core_client, false);
			return rc;
		}

		/* enable all the irq */
		_sde_encoder_irq_control(drm_enc, true);

		if (is_cmd_mode)
			_sde_encoder_pm_qos_add_request(drm_enc);

	} else {
		if (is_cmd_mode)
			_sde_encoder_pm_qos_remove_request(drm_enc);

		/* disable all the irq */
		_sde_encoder_irq_control(drm_enc, false);

		/* disable DSI clks */
		sde_connector_clk_ctrl(sde_enc->cur_master->connector, false);

		/* disable SDE core clks */
		sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, false);
	}

	return 0;
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

static int sde_encoder_resource_control(struct drm_encoder *drm_enc,
		u32 sw_event)
{
	bool autorefresh_enabled = false;
	unsigned int lp, idle_pc_duration;
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct msm_drm_thread *disp_thread;
	int ret;
	bool is_vid_mode = false;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid encoder parameters, sw_event:%u\n",
				sw_event);
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	is_vid_mode = sde_enc->disp_info.capabilities &
						MSM_DISPLAY_CAP_VID_MODE;

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
		/* cancel delayed off work, if any */
		if (kthread_cancel_delayed_work_sync(
				&sde_enc->delayed_off_work))
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work cancelled\n",
					sw_event);

		mutex_lock(&sde_enc->rc_lock);

		/* return if the resource control is already in ON state */
		if (sde_enc->rc_state == SDE_ENC_RC_STATE_ON) {
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in ON state\n",
					sw_event);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_FUNC_CASE1);
			mutex_unlock(&sde_enc->rc_lock);
			return 0;
		} else if (sde_enc->rc_state != SDE_ENC_RC_STATE_OFF &&
				sde_enc->rc_state != SDE_ENC_RC_STATE_IDLE) {
			SDE_ERROR_ENC(sde_enc, "sw_event:%d, rc in state %d\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			mutex_unlock(&sde_enc->rc_lock);
			return -EINVAL;
		}

		if (is_vid_mode && sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
			_sde_encoder_irq_control(drm_enc, true);
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
				mutex_unlock(&sde_enc->rc_lock);
				return ret;
			}

			_sde_encoder_resource_control_rsc_update(drm_enc, true);
		}

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE1);
		sde_enc->rc_state = SDE_ENC_RC_STATE_ON;

		mutex_unlock(&sde_enc->rc_lock);
		break;

	case SDE_ENC_RC_EVENT_FRAME_DONE:
		if (!sde_enc->crtc) {
			SDE_ERROR("invalid crtc, sw_event:%u\n", sw_event);
			return -EINVAL;
		}

		if (sde_enc->crtc->index >= ARRAY_SIZE(priv->disp_thread)) {
			SDE_ERROR("invalid crtc index :%u\n",
					sde_enc->crtc->index);
			return -EINVAL;
		}
		disp_thread = &priv->disp_thread[sde_enc->crtc->index];

		/*
		 * mutex lock is not used as this event happens at interrupt
		 * context. And locking is not required as, the other events
		 * like KICKOFF and STOP does a wait-for-idle before executing
		 * the resource_control
		 */
		if (sde_enc->rc_state != SDE_ENC_RC_STATE_ON) {
			SDE_ERROR_ENC(sde_enc, "sw_event:%d,rc:%d-unexpected\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			return -EINVAL;
		}

		/*
		 * schedule off work item only when there are no
		 * frames pending
		 */
		if (sde_crtc_frame_pending(sde_enc->crtc) > 1) {
			SDE_DEBUG_ENC(sde_enc, "skip schedule work");
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_FUNC_CASE2);
			return 0;
		}

		/* schedule delayed off work if autorefresh is disabled */
		if (sde_enc->cur_master &&
			sde_enc->cur_master->ops.is_autorefresh_enabled)
			autorefresh_enabled =
				sde_enc->cur_master->ops.is_autorefresh_enabled(
							sde_enc->cur_master);

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

		if (!autorefresh_enabled)
			kthread_mod_delayed_work(
				&disp_thread->worker,
				&sde_enc->delayed_off_work,
				msecs_to_jiffies(idle_pc_duration));
		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				autorefresh_enabled,
				idle_pc_duration, SDE_EVTLOG_FUNC_CASE2);
		SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work scheduled\n",
				sw_event);
		break;

	case SDE_ENC_RC_EVENT_PRE_STOP:
		/* cancel delayed off work, if any */
		if (kthread_cancel_delayed_work_sync(
				&sde_enc->delayed_off_work))
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work cancelled\n",
					sw_event);

		mutex_lock(&sde_enc->rc_lock);

		if (is_vid_mode &&
			  sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
			_sde_encoder_irq_control(drm_enc, true);
		}
		/* skip if is already OFF or IDLE, resources are off already */
		else if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF ||
				sde_enc->rc_state == SDE_ENC_RC_STATE_IDLE) {
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in %d state\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_FUNC_CASE3);
			mutex_unlock(&sde_enc->rc_lock);
			return 0;
		}

		/**
		 * IRQs are still enabled currently, which allows wait for
		 * VBLANK which RSC may require to correctly transition to OFF
		 */
		_sde_encoder_resource_control_rsc_update(drm_enc, false);

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_ENC_RC_STATE_PRE_OFF,
				SDE_EVTLOG_FUNC_CASE3);

		sde_enc->rc_state = SDE_ENC_RC_STATE_PRE_OFF;

		mutex_unlock(&sde_enc->rc_lock);
		break;

	case SDE_ENC_RC_EVENT_STOP:
		/* cancel vsync event work and timer */
		kthread_cancel_work_sync(&sde_enc->vsync_event_work);
		if (sde_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DSI)
			del_timer_sync(&sde_enc->vsync_event_timer);

		mutex_lock(&sde_enc->rc_lock);
		/* return if the resource control is already in OFF state */
		if (sde_enc->rc_state == SDE_ENC_RC_STATE_OFF) {
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc in OFF state\n",
					sw_event);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_EVTLOG_FUNC_CASE4);
			mutex_unlock(&sde_enc->rc_lock);
			return 0;
		} else if (sde_enc->rc_state == SDE_ENC_RC_STATE_ON ||
			   sde_enc->rc_state == SDE_ENC_RC_STATE_MODESET) {
			SDE_ERROR_ENC(sde_enc, "sw_event:%d, rc in state %d\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			mutex_unlock(&sde_enc->rc_lock);
			return -EINVAL;
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

		mutex_unlock(&sde_enc->rc_lock);
		break;

	case SDE_ENC_RC_EVENT_PRE_MODESET:
		/* cancel delayed off work, if any */
		if (kthread_cancel_delayed_work_sync(
				&sde_enc->delayed_off_work))
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, work cancelled\n",
					sw_event);

		mutex_lock(&sde_enc->rc_lock);

		/* return if the resource control is already in ON state */
		if (sde_enc->rc_state != SDE_ENC_RC_STATE_ON) {
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
				mutex_unlock(&sde_enc->rc_lock);
				return ret;
			}

			_sde_encoder_resource_control_rsc_update(drm_enc, true);

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
			mutex_unlock(&sde_enc->rc_lock);
			return -EINVAL;
		}

		_sde_encoder_irq_control(drm_enc, false);
		_sde_encoder_modeset_helper_locked(drm_enc, sw_event);

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
			SDE_ENC_RC_STATE_MODESET, SDE_EVTLOG_FUNC_CASE5);

		sde_enc->rc_state = SDE_ENC_RC_STATE_MODESET;
		mutex_unlock(&sde_enc->rc_lock);
		break;

	case SDE_ENC_RC_EVENT_POST_MODESET:
		mutex_lock(&sde_enc->rc_lock);

		/* return if the resource control is already in ON state */
		if (sde_enc->rc_state != SDE_ENC_RC_STATE_MODESET) {
			SDE_ERROR_ENC(sde_enc,
					"sw_event:%d, rc:%d !MODESET state\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			mutex_unlock(&sde_enc->rc_lock);
			return -EINVAL;
		}

		_sde_encoder_modeset_helper_locked(drm_enc, sw_event);
		_sde_encoder_irq_control(drm_enc, true);

		_sde_encoder_update_rsc_client(drm_enc, NULL, true);

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE6);

		sde_enc->rc_state = SDE_ENC_RC_STATE_ON;

		mutex_unlock(&sde_enc->rc_lock);
		break;

	case SDE_ENC_RC_EVENT_ENTER_IDLE:
		mutex_lock(&sde_enc->rc_lock);

		if (sde_enc->rc_state != SDE_ENC_RC_STATE_ON) {
			SDE_DEBUG_ENC(sde_enc, "sw_event:%d, rc:%d !ON state\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			mutex_unlock(&sde_enc->rc_lock);
			return 0;
		}

		/*
		 * if we are in ON but a frame was just kicked off,
		 * ignore the IDLE event, it's probably a stale timer event
		 */
		if (sde_enc->frame_busy_mask[0]) {
			SDE_ERROR_ENC(sde_enc,
					"sw_event:%d, rc:%d frame pending\n",
					sw_event, sde_enc->rc_state);
			SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
					SDE_EVTLOG_ERROR);
			mutex_unlock(&sde_enc->rc_lock);
			return 0;
		}

		if (is_vid_mode) {
			_sde_encoder_irq_control(drm_enc, false);
		} else {
			/* disable all the clks and resources */
			_sde_encoder_resource_control_rsc_update(drm_enc,
								false);
			_sde_encoder_resource_control_helper(drm_enc, false);
		}

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_ENC_RC_STATE_IDLE, SDE_EVTLOG_FUNC_CASE7);
		sde_enc->rc_state = SDE_ENC_RC_STATE_IDLE;

		mutex_unlock(&sde_enc->rc_lock);
		break;
	case SDE_ENC_RC_EVENT_EARLY_WAKEUP:
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
				mutex_unlock(&sde_enc->rc_lock);
				return 0;
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
				mutex_unlock(&sde_enc->rc_lock);
				return ret;
			}

			_sde_encoder_resource_control_rsc_update(drm_enc, true);

			kthread_mod_delayed_work(&disp_thread->worker,
						&sde_enc->delayed_off_work,
						msecs_to_jiffies(
						IDLE_POWERCOLLAPSE_DURATION));

			sde_enc->rc_state = SDE_ENC_RC_STATE_ON;
		}

		SDE_EVT32(DRMID(drm_enc), sw_event, sde_enc->rc_state,
				SDE_ENC_RC_STATE_ON, SDE_EVTLOG_FUNC_CASE8);

		mutex_unlock(&sde_enc->rc_lock);
		break;
	default:
		SDE_EVT32(DRMID(drm_enc), sw_event, SDE_EVTLOG_ERROR);
		SDE_ERROR("unexpected sw_event: %d\n", sw_event);
		break;
	}

	SDE_EVT32_VERBOSE(DRMID(drm_enc), sw_event, sde_enc->idle_pc_enabled,
			sde_enc->rc_state, SDE_EVTLOG_FUNC_EXIT);
	return 0;
}

static void sde_encoder_virt_mode_set(struct drm_encoder *drm_enc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct list_head *connector_list;
	struct drm_connector *conn = NULL, *conn_iter;
	struct sde_connector_state *sde_conn_state = NULL;
	struct sde_connector *sde_conn = NULL;
	struct sde_rm_hw_iter dsc_iter, pp_iter;
	int i = 0, ret;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (!sde_kms_power_resource_is_enabled(drm_enc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	connector_list = &sde_kms->dev->mode_config.connector_list;

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

	list_for_each_entry(conn_iter, connector_list, head)
		if (conn_iter->encoder == drm_enc)
			conn = conn_iter;

	if (!conn) {
		SDE_ERROR_ENC(sde_enc, "failed to find attached connector\n");
		return;
	} else if (!conn->state) {
		SDE_ERROR_ENC(sde_enc, "invalid connector state\n");
		return;
	}

	sde_conn = to_sde_connector(conn);
	sde_conn_state = to_sde_connector_state(conn->state);
	if (sde_conn && sde_conn_state) {
		ret = sde_conn->ops.get_mode_info(&sde_conn->base, adj_mode,
				&sde_conn_state->mode_info,
				sde_kms->catalog->max_mixer_width,
				sde_conn->display);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
				"failed to get mode info from the display\n");
			return;
		}
	}

	/* release resources before seamless mode change */
	if (msm_is_mode_seamless_dms(adj_mode)) {
		/* restore resource state before releasing them */
		ret = sde_encoder_resource_control(drm_enc,
				SDE_ENC_RC_EVENT_PRE_MODESET);
		if (ret) {
			SDE_ERROR_ENC(sde_enc,
					"sde resource control failed: %d\n",
					ret);
			return;
		}

		/*
		 * Disable dsc before switch the mode and after pre_modeset,
		 * to guarantee that previous kickoff finished.
		 */
		_sde_encoder_dsc_disable(sde_enc);
	}

	/* Reserve dynamic resources now. Indicating non-AtomicTest phase */
	ret = sde_rm_reserve(&sde_kms->rm, drm_enc, drm_enc->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
				"failed to reserve hw resources, %d\n", ret);
		return;
	}

	sde_rm_init_hw_iter(&pp_iter, drm_enc->base.id, SDE_HW_BLK_PINGPONG);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_pp[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &pp_iter))
			break;
		sde_enc->hw_pp[i] = (struct sde_hw_pingpong *) pp_iter.hw;
	}

	sde_rm_init_hw_iter(&dsc_iter, drm_enc->base.id, SDE_HW_BLK_DSC);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_dsc[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &dsc_iter))
			break;
		sde_enc->hw_dsc[i] = (struct sde_hw_dsc *) dsc_iter.hw;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			if (!sde_enc->hw_pp[i]) {
				SDE_ERROR_ENC(sde_enc,
				    "invalid pingpong block for the encoder\n");
				return;
			}
			phys->hw_pp = sde_enc->hw_pp[i];
			phys->connector = conn->state->connector;
			if (phys->ops.mode_set)
				phys->ops.mode_set(phys, mode, adj_mode);
		}
	}

	/* update resources after seamless mode change */
	if (msm_is_mode_seamless_dms(adj_mode))
		sde_encoder_resource_control(&sde_enc->base,
						SDE_ENC_RC_EVENT_POST_MODESET);
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

static int _sde_encoder_input_handler_register(
		struct input_handler *input_handler)
{
	int rc = 0;

	rc = input_register_handler(input_handler);
	if (rc) {
		pr_err("input_register_handler failed, rc= %d\n", rc);
		kfree(input_handler);
		return rc;
	}

	return rc;
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
	input_handler->private = sde_enc;

	sde_enc->input_handler = input_handler;

	return rc;
}

static void _sde_encoder_virt_enable_helper(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc || !sde_enc->cur_master) {
		SDE_ERROR("invalid sde encoder/master\n");
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
			!sde_kms->splash_data.cont_splash_en)
		sde_enc->cur_master->hw_ctl->ops.setup_intf_cfg_v1(
				sde_enc->cur_master->hw_ctl,
				&sde_enc->cur_master->intf_cfg_v1);

	_sde_encoder_update_vsync_source(sde_enc, &sde_enc->disp_info, false);
	sde_encoder_control_te(drm_enc, true);

	memset(&sde_enc->prv_conn_roi, 0, sizeof(sde_enc->prv_conn_roi));
	memset(&sde_enc->cur_conn_roi, 0, sizeof(sde_enc->cur_conn_roi));
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
	}

	if (sde_enc->cur_master && sde_enc->cur_master->ops.restore)
		sde_enc->cur_master->ops.restore(sde_enc->cur_master);

	_sde_encoder_virt_enable_helper(drm_enc);
}

static void sde_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;
	struct msm_compression_info *comp_info = NULL;
	struct drm_display_mode *cur_mode = NULL;
	struct msm_mode_info mode_info;
	struct msm_display_info *disp_info;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;

	if (!sde_kms_power_resource_is_enabled(drm_enc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	ret = _sde_encoder_get_mode_info(drm_enc, &mode_info);
	if (ret) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return;
	}

	if (drm_enc->crtc && !sde_enc->crtc)
		sde_enc->crtc = drm_enc->crtc;

	comp_info = &mode_info.comp_info;
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

	if (sde_enc->input_handler) {
		ret = _sde_encoder_input_handler_register(
				sde_enc->input_handler);
		if (ret)
			SDE_ERROR(
			"input handler registration failed, rc = %d\n", ret);
	}

	sde_enc->delayed_off_work.work.worker = NULL;

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
		if (phys != sde_enc->cur_master) {
			/**
			 * on DMS request, the encoder will be enabled
			 * already. Invoke restore to reconfigure the
			 * new mode.
			 */
			if (msm_is_mode_seamless_dms(cur_mode) &&
					phys->ops.restore)
				phys->ops.restore(phys);
			else if (phys->ops.enable)
				phys->ops.enable(phys);
		}

		if (sde_enc->misr_enable && (sde_enc->disp_info.capabilities &
		     MSM_DISPLAY_CAP_VID_MODE) && phys->ops.setup_misr)
			phys->ops.setup_misr(phys, true,
						sde_enc->misr_frame_count);
	}

	if (msm_is_mode_seamless_dms(cur_mode) &&
			sde_enc->cur_master->ops.restore)
		sde_enc->cur_master->ops.restore(sde_enc->cur_master);
	else if (sde_enc->cur_master->ops.enable)
		sde_enc->cur_master->ops.enable(sde_enc->cur_master);

	_sde_encoder_virt_enable_helper(drm_enc);
}

static void sde_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_private *priv;
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

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	intf_mode = sde_encoder_get_intf_mode(drm_enc);

	SDE_EVT32(DRMID(drm_enc));

	/* wait for idle */
	sde_encoder_wait_for_event(drm_enc, MSM_ENC_TX_COMPLETE);

	kthread_flush_work(&sde_enc->input_event_work);

	if (sde_enc->input_handler)
		input_unregister_handler(sde_enc->input_handler);

	/*
	 * For primary command mode encoders, execute the resource control
	 * pre-stop operations before the physical encoders are disabled, to
	 * allow the rsc to transition its states properly.
	 *
	 * For other encoder types, rsc should not be enabled until after
	 * they have been fully disabled, so delay the pre-stop operations
	 * until after the physical disable calls have returned.
	 */
	if (sde_enc->disp_info.is_primary && intf_mode == INTF_MODE_CMD) {
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
	 * disable dsc after the transfer is complete (for command mode)
	 * and after physical encoder is disabled, to make sure timing
	 * engine is already disabled (for video mode).
	 */
	_sde_encoder_dsc_disable(sde_enc);

	sde_encoder_resource_control(drm_enc, SDE_ENC_RC_EVENT_STOP);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		if (sde_enc->phys_encs[i]) {
			sde_enc->phys_encs[i]->cont_splash_settings = false;
			sde_enc->phys_encs[i]->cont_splash_single_flush = 0;
			sde_enc->phys_encs[i]->connector = NULL;
		}
	}

	sde_enc->cur_master = NULL;
	/*
	 * clear the cached crtc in sde_enc on use case finish, after all the
	 * outstanding events and timers have been completed
	 */
	sde_enc->crtc = NULL;

	SDE_DEBUG_ENC(sde_enc, "encoder disabled\n");

	sde_rm_release(&sde_kms->rm, drm_enc);
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

	atomic_inc(&phy_enc->vsync_cnt);
	SDE_ATRACE_END("encoder_vblank_callback");
}

static void sde_encoder_underrun_callback(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phy_enc)
{
	if (!phy_enc)
		return;

	SDE_ATRACE_BEGIN("encoder_underrun_callback");
	atomic_inc(&phy_enc->underrun_cnt);
	SDE_EVT32(DRMID(drm_enc), atomic_read(&phy_enc->underrun_cnt));

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

	sde_enc->crtc_frame_event_cb_data.connector =
				sde_enc->cur_master->connector;

	if (event & (SDE_ENCODER_FRAME_EVENT_DONE
			| SDE_ENCODER_FRAME_EVENT_ERROR
			| SDE_ENCODER_FRAME_EVENT_PANEL_DEAD)) {

		if (!sde_enc->frame_busy_mask[0]) {
			/**
			 * suppress frame_done without waiter,
			 * likely autorefresh
			 */
			SDE_EVT32(DRMID(drm_enc), event, ready_phys->intf_idx);
			return;
		}

		/* One of the physical encoders has become idle */
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			if (sde_enc->phys_encs[i] == ready_phys) {
				clear_bit(i, sde_enc->frame_busy_mask);
				SDE_EVT32_VERBOSE(DRMID(drm_enc), i,
					sde_enc->frame_busy_mask[0]);
			}
		}

		if (!sde_enc->frame_busy_mask[0]) {
			sde_encoder_resource_control(drm_enc,
					SDE_ENC_RC_EVENT_FRAME_DONE);

			if (sde_enc->crtc_frame_event_cb)
				sde_enc->crtc_frame_event_cb(
					&sde_enc->crtc_frame_event_cb_data,
					event);
		}
	} else {
		if (sde_enc->crtc_frame_event_cb)
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

	sde_encoder_idle_request(drm_enc);
}

/**
 * _sde_encoder_trigger_flush - trigger flush for a physical encoder
 * drm_enc: Pointer to drm encoder structure
 * phys: Pointer to physical encoder structure
 * extra_flush: Additional bit mask to include in flush trigger
 */
static inline void _sde_encoder_trigger_flush(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phys,
		struct sde_ctl_flush_cfg *extra_flush)
{
	struct sde_hw_ctl *ctl;
	unsigned long lock_flags;
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc || !phys) {
		SDE_ERROR("invalid argument(s), drm_enc %d, phys_enc %d\n",
				drm_enc != 0, phys != 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

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

	if (phys->ops.is_master && phys->ops.is_master(phys))
		atomic_inc(&phys->pending_retire_fence_cnt);

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
				pending_flush.pending_flush_mask);
	} else {
		SDE_EVT32(DRMID(drm_enc), phys->intf_idx - INTF_0,
				ctl->idx - CTL_0);
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

	/* Start rotator before CTL_START for async inline mode */
	if (sde_crtc_get_rotator_op_mode(sde_enc->crtc) ==
			SDE_CTL_ROT_OP_MODE_INLINE_ASYNC &&
			ctl->ops.trigger_rot_start) {
		SDE_DEBUG_ENC(sde_enc, "trigger rotator start ctl%d\n",
				ctl->idx - CTL_0);
		ctl->ops.trigger_rot_start(ctl);
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

static int _sde_encoder_wait_timeout(int32_t drm_id, int32_t hw_id,
	s64 timeout_ms, struct sde_encoder_wait_info *info)
{
	int rc = 0;
	s64 wait_time_jiffies = msecs_to_jiffies(timeout_ms);
	ktime_t cur_ktime;
	ktime_t exp_ktime = ktime_add_ms(ktime_get(), timeout_ms);

	do {
		rc = wait_event_timeout(*(info->wq),
			atomic_read(info->atomic_cnt) == 0, wait_time_jiffies);
		cur_ktime = ktime_get();

		SDE_EVT32(drm_id, hw_id, rc, ktime_to_ms(cur_ktime),
			timeout_ms, atomic_read(info->atomic_cnt));
	/* If we timed out, counter is valid and time is less, wait again */
	} while (atomic_read(info->atomic_cnt) && (rc == 0) &&
			(ktime_compare_safe(exp_ktime, cur_ktime) > 0));

	return rc;
}

int sde_encoder_helper_wait_event_timeout(int32_t drm_id, int32_t hw_id,
	struct sde_encoder_wait_info *info)
{
	int rc;
	ktime_t exp_ktime = ktime_add_ms(ktime_get(), info->timeout_ms);

	rc = _sde_encoder_wait_timeout(drm_id, hw_id, info->timeout_ms, info);

	/**
	 * handle disabled irq case where timer irq is also delayed.
	 * wait for additional timeout of FAULT_TOLERENCE_WAIT_IN_MS
	 * if it event_timeout expired late detected.
	 */
	if (atomic_read(info->atomic_cnt) && (!rc) &&
	    (ktime_compare_safe(ktime_get(), ktime_add_ms(exp_ktime,
	     FAULT_TOLERENCE_DELTA_IN_MS)) > 0))
		rc = _sde_encoder_wait_timeout(drm_id, hw_id,
			FAULT_TOLERENCE_WAIT_IN_MS, info);

	return rc;
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
 */
static void _sde_encoder_kickoff_phys(struct sde_encoder_virt *sde_enc)
{
	struct sde_hw_ctl *ctl;
	uint32_t i;
	struct sde_ctl_flush_cfg pending_flush = {0,};
	u32 pending_kickoff_cnt;

	if (!sde_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	/* don't perform flush/start operations for slave encoders */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];
		enum sde_rm_topology_name topology = SDE_RM_TOPOLOGY_NONE;
		bool wait_for_dma = false;

		if (!phys || phys->enable_state == SDE_ENC_DISABLED)
			continue;

		ctl = phys->hw_ctl;
		if (!ctl)
			continue;

		if (phys->ops.wait_dma_trigger)
			wait_for_dma = phys->ops.wait_dma_trigger(
					phys);

		if (phys->hw_ctl->ops.reg_dma_flush)
			phys->hw_ctl->ops.reg_dma_flush(phys->hw_ctl,
					wait_for_dma);

		if (phys->connector)
			topology = sde_connector_get_topology_name(
					phys->connector);

		/*
		 * don't wait on ppsplit slaves or skipped encoders because
		 * they dont receive irqs
		 */
		if (!(topology == SDE_RM_TOPOLOGY_PPSPLIT &&
				phys->split_role == ENC_ROLE_SLAVE) &&
				phys->split_role != ENC_ROLE_SKIP)
			set_bit(i, sde_enc->frame_busy_mask);

		if (!phys->ops.needs_single_flush ||
				!phys->ops.needs_single_flush(phys))
			_sde_encoder_trigger_flush(&sde_enc->base, phys, 0x0);
		else if (ctl->ops.get_pending_flush)
			ctl->ops.get_pending_flush(ctl, &pending_flush);
	}

	/* for split flush, combine pending flush masks and send to master */
	if (pending_flush.pending_flush_mask && sde_enc->cur_master) {
		_sde_encoder_trigger_flush(
				&sde_enc->base,
				sde_enc->cur_master,
				&pending_flush);
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

	_sde_encoder_trigger_start(sde_enc->cur_master);
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
		bool active;

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

bool sde_encoder_check_mode(struct drm_encoder *drm_enc, u32 mode)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_display_info *disp_info;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return false;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;

	return (disp_info->capabilities & mode);
}

void sde_encoder_trigger_kickoff_pending(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned int i;
	struct sde_hw_ctl *ctl;
	struct msm_display_info *disp_info;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	disp_info = &sde_enc->disp_info;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];

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

			/* update only for command mode primary ctl */
			if ((phys == sde_enc->cur_master) &&
			   (disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE)
			    && ctl->ops.trigger_pending)
				ctl->ops.trigger_pending(ctl);
		}
	}
	sde_enc->idle_pc_restore = false;
}

static void _sde_encoder_setup_dither(struct sde_encoder_phys *phys)
{
	void *dither_cfg;
	int ret = 0, rc, i = 0;
	size_t len = 0;
	enum sde_rm_topology_name topology;
	struct drm_encoder *drm_enc;
	struct msm_mode_info mode_info;
	struct msm_display_dsc_info *dsc = NULL;
	struct sde_encoder_virt *sde_enc;
	struct sde_hw_pingpong *hw_pp;

	if (!phys || !phys->connector || !phys->hw_pp ||
			!phys->hw_pp->ops.setup_dither || !phys->parent)
		return;

	topology = sde_connector_get_topology_name(phys->connector);
	if ((topology == SDE_RM_TOPOLOGY_PPSPLIT) &&
			(phys->split_role == ENC_ROLE_SLAVE))
		return;

	drm_enc = phys->parent;
	sde_enc = to_sde_encoder_virt(drm_enc);
	rc = _sde_encoder_get_mode_info(&sde_enc->base, &mode_info);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "failed to get mode info\n");
		return;
	}

	dsc = &mode_info.comp_info.dsc_info;
	/* disable dither for 10 bpp or 10bpc dsc config */
	if (dsc->bpp == 10 || dsc->bpc == 10) {
		phys->hw_pp->ops.setup_dither(phys->hw_pp, NULL, 0);
		return;
	}

	ret = sde_connector_get_dither_cfg(phys->connector,
			phys->connector->state, &dither_cfg, &len);
	if (ret)
		return;

	if (TOPOLOGY_DUALPIPE_MERGE_MODE(topology)) {
		for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
			hw_pp = sde_enc->hw_pp[i];
			if (hw_pp) {
				phys->hw_pp->ops.setup_dither(hw_pp, dither_cfg,
								len);
			}
		}
	} else {
		phys->hw_pp->ops.setup_dither(phys->hw_pp, dither_cfg, len);
	}
}

static u32 _sde_encoder_calculate_linetime(struct sde_encoder_virt *sde_enc,
		struct drm_display_mode *mode)
{
	u64 pclk_rate;
	u32 pclk_period;
	u32 line_time;

	/*
	 * For linetime calculation, only operate on master encoder.
	 */
	if (!sde_enc->cur_master)
		return 0;

	if (!sde_enc->cur_master->ops.get_line_count) {
		SDE_ERROR("get_line_count function not defined\n");
		return 0;
	}

	pclk_rate = mode->clock; /* pixel clock in kHz */
	if (pclk_rate == 0) {
		SDE_ERROR("pclk is 0, cannot calculate line time\n");
		return 0;
	}

	pclk_period = DIV_ROUND_UP_ULL(1000000000ull, pclk_rate);
	if (pclk_period == 0) {
		SDE_ERROR("pclk period is 0\n");
		return 0;
	}

	/*
	 * Line time calculation based on Pixel clock and HTOTAL.
	 * Final unit is in ns.
	 */
	line_time = (pclk_period * mode->htotal) / 1000;
	if (line_time == 0) {
		SDE_ERROR("line time calculation is 0\n");
		return 0;
	}

	SDE_DEBUG_ENC(sde_enc,
			"clk_rate=%lldkHz, clk_period=%d, linetime=%dns\n",
			pclk_rate, pclk_period, line_time);

	return line_time;
}

static int _sde_encoder_wakeup_time(struct drm_encoder *drm_enc,
		ktime_t *wakeup_time)
{
	struct drm_display_mode *mode;
	struct sde_encoder_virt *sde_enc;
	u32 cur_line;
	u32 line_time;
	u32 vtotal, time_to_vsync;
	ktime_t cur_time;

	sde_enc = to_sde_encoder_virt(drm_enc);
	mode = &sde_enc->cur_master->cached_mode;

	line_time = _sde_encoder_calculate_linetime(sde_enc, mode);
	if (!line_time)
		return -EINVAL;

	cur_line = sde_enc->cur_master->ops.get_line_count(sde_enc->cur_master);

	vtotal = mode->vtotal;
	if (cur_line >= vtotal)
		time_to_vsync = line_time * vtotal;
	else
		time_to_vsync = line_time * (vtotal - cur_line);

	if (time_to_vsync == 0) {
		SDE_ERROR("time to vsync should not be zero, vtotal=%d\n",
				vtotal);
		return -EINVAL;
	}

	cur_time = ktime_get();
	*wakeup_time = ktime_add_ns(cur_time, time_to_vsync);

	SDE_DEBUG_ENC(sde_enc,
			"cur_line=%u vtotal=%u time_to_vsync=%u, cur_time=%lld, wakeup_time=%lld\n",
			cur_line, vtotal, time_to_vsync,
			ktime_to_ms(cur_time),
			ktime_to_ms(*wakeup_time));
	return 0;
}

static void sde_encoder_vsync_event_handler(unsigned long data)
{
	struct drm_encoder *drm_enc = (struct drm_encoder *) data;
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct msm_drm_thread *event_thread;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid encoder parameters\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	if (!sde_enc->crtc) {
		SDE_ERROR("invalid crtc");
		return;
	}

	if (sde_enc->crtc->index >= ARRAY_SIZE(priv->event_thread)) {
		SDE_ERROR("invalid crtc index:%u\n",
				sde_enc->crtc->index);
		return;
	}
	event_thread = &priv->event_thread[sde_enc->crtc->index];
	if (!event_thread) {
		SDE_ERROR("event_thread not found for crtc:%d\n",
				sde_enc->crtc->index);
		return;
	}

	kthread_queue_work(&event_thread->worker,
				&sde_enc->vsync_event_work);
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

static void sde_encoder_vsync_event_work_handler(struct kthread_work *work)
{
	struct sde_encoder_virt *sde_enc = container_of(work,
			struct sde_encoder_virt, vsync_event_work);
	bool autorefresh_enabled = false;
	int rc = 0;
	ktime_t wakeup_time;

	if (!sde_enc) {
		SDE_ERROR("invalid sde encoder\n");
		return;
	}

	rc = _sde_encoder_power_enable(sde_enc, true);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "sde enc power enabled failed:%d\n", rc);
		return;
	}

	if (sde_enc->cur_master &&
		sde_enc->cur_master->ops.is_autorefresh_enabled)
		autorefresh_enabled =
			sde_enc->cur_master->ops.is_autorefresh_enabled(
						sde_enc->cur_master);

	/* Update timer if autorefresh is enabled else return */
	if (!autorefresh_enabled)
		goto exit;

	rc = _sde_encoder_wakeup_time(&sde_enc->base, &wakeup_time);
	if (rc)
		goto exit;

	SDE_EVT32_VERBOSE(ktime_to_ms(wakeup_time));
	mod_timer(&sde_enc->vsync_event_timer,
			nsecs_to_jiffies(ktime_to_ns(wakeup_time)));

exit:
	_sde_encoder_power_enable(sde_enc, false);
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

static int _helper_flush_mixer(struct sde_encoder_phys *phys_enc)
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

	sde_rm_init_hw_iter(&lm_iter, drm_enc->base.id, SDE_HW_BLK_LM);
	while (sde_rm_get_hw(&phys_enc->sde_kms->rm, &lm_iter)) {
		struct sde_hw_mixer *hw_lm = (struct sde_hw_mixer *)lm_iter.hw;

		if (!hw_lm)
			continue;

		/* update LM flush */
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

	return 0;
}

int sde_encoder_prepare_for_kickoff(struct drm_encoder *drm_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	struct sde_kms *sde_kms = NULL;
	struct msm_drm_private *priv = NULL;
	bool needs_hw_reset = false;
	uint32_t ln_cnt1, ln_cnt2;
	unsigned int i;
	int rc, ret = 0;
	struct msm_display_info *disp_info;

	if (!drm_enc || !params || !drm_enc->dev ||
		!drm_enc->dev->dev_private) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	disp_info = &sde_enc->disp_info;

	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc));

	/* save this for later, in case of errors */
	if (sde_enc->cur_master && sde_enc->cur_master->ops.get_wr_line_count)
		ln_cnt1 = sde_enc->cur_master->ops.get_wr_line_count(
				sde_enc->cur_master);
	else
		ln_cnt1 = -EINVAL;

	/* prepare for next kickoff, may include waiting on previous kickoff */
	SDE_ATRACE_BEGIN("enc_prepare_for_kickoff");
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		params->is_primary = sde_enc->disp_info.is_primary;
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

			/* flush the mixer if qsync is enabled */
			if (sde_enc->cur_master && sde_connector_qsync_updated(
					sde_enc->cur_master->connector)) {
				_helper_flush_mixer(phys);
			}
		}
	}
	SDE_ATRACE_END("enc_prepare_for_kickoff");

	rc = sde_encoder_resource_control(drm_enc, SDE_ENC_RC_EVENT_KICKOFF);
	if (rc) {
		SDE_ERROR_ENC(sde_enc, "resource kickoff failed rc %d\n", rc);
		return rc;
	}

	/* if any phys needs reset, reset all phys, in-order */
	if (needs_hw_reset) {
		/* query line count before cur_master is updated */
		if (sde_enc->cur_master &&
				sde_enc->cur_master->ops.get_wr_line_count)
			ln_cnt2 = sde_enc->cur_master->ops.get_wr_line_count(
					sde_enc->cur_master);
		else
			ln_cnt2 = -EINVAL;

		SDE_EVT32(DRMID(drm_enc), ln_cnt1, ln_cnt2,
				SDE_EVTLOG_FUNC_CASE1);
		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			phys = sde_enc->phys_encs[i];
			if (phys && phys->ops.hw_reset)
				phys->ops.hw_reset(phys);
		}
	}

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

	if (_sde_encoder_is_dsc_enabled(drm_enc) &&
		!sde_kms->splash_data.cont_splash_en) {
		rc = _sde_encoder_dsc_setup(sde_enc, params);
		if (rc) {
			SDE_ERROR_ENC(sde_enc, "failed to setup DSC: %d\n", rc);
			ret = rc;
		}
	}

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

void sde_encoder_kickoff(struct drm_encoder *drm_enc, bool is_error)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	ktime_t wakeup_time;
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
	_sde_encoder_kickoff_phys(sde_enc);

	/* allow phys encs to handle any post-kickoff business */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.handle_post_kickoff)
			phys->ops.handle_post_kickoff(phys);
	}

	if (sde_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DSI &&
			sde_enc->disp_info.is_primary &&
			!_sde_encoder_wakeup_time(drm_enc, &wakeup_time)) {
		SDE_EVT32_VERBOSE(ktime_to_ms(wakeup_time));
		mod_timer(&sde_enc->vsync_event_timer,
				nsecs_to_jiffies(ktime_to_ns(wakeup_time)));
	}

	SDE_ATRACE_END("encoder_kickoff");
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
					phys_enc->hw_ctl, hw_lm->idx, NULL);
	}

	if (!lm_valid) {
		SDE_ERROR_ENC(to_sde_encoder_virt(drm_enc), "lm not found\n");
		return -EFAULT;
	}
	return 0;
}

void sde_encoder_prepare_commit(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.prepare_commit)
			phys->ops.prepare_commit(phys);
	}
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
	int i = 0, rc;
	char buf[MISR_BUFF_SIZE + 1];
	size_t buff_copy;
	u32 frame_count, enable;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_enc = file->private_data;

	buff_copy = min_t(size_t, count, MISR_BUFF_SIZE);
	if (copy_from_user(buf, user_buf, buff_copy))
		return -EINVAL;

	buf[buff_copy] = 0; /* end of string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2)
		return -EINVAL;

	rc = _sde_encoder_power_enable(sde_enc, true);
	if (rc)
		return rc;

	mutex_lock(&sde_enc->enc_lock);
	sde_enc->misr_enable = enable;
	sde_enc->misr_frame_count = frame_count;
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || !phys->ops.setup_misr)
			continue;

		phys->ops.setup_misr(phys, enable, frame_count);
	}
	mutex_unlock(&sde_enc->enc_lock);
	_sde_encoder_power_enable(sde_enc, false);

	return count;
}

static ssize_t _sde_encoder_misr_read(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct sde_encoder_virt *sde_enc;
	int i = 0, len = 0;
	char buf[MISR_BUFF_SIZE + 1] = {'\0'};
	int rc;

	if (*ppos)
		return 0;

	if (!file || !file->private_data)
		return -EINVAL;

	sde_enc = file->private_data;

	rc = _sde_encoder_power_enable(sde_enc, true);
	if (rc)
		return rc;

	mutex_lock(&sde_enc->enc_lock);
	if (!sde_enc->misr_enable) {
		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"disabled\n");
		goto buff_check;
	} else if (sde_enc->disp_info.capabilities &
						~MSM_DISPLAY_CAP_VID_MODE) {
		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"unsupported\n");
		goto buff_check;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || !phys->ops.collect_misr)
			continue;

		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"Intf idx:%d\n", phys->intf_idx - INTF_0);
		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "0x%x\n",
					phys->ops.collect_misr(phys));
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
	mutex_unlock(&sde_enc->enc_lock);
	_sde_encoder_power_enable(sde_enc, false);
	return len;
}

static int _sde_encoder_init_debugfs(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
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

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid encoder or kms\n");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	snprintf(name, SDE_NAME_SIZE, "encoder%u", drm_enc->base.id);

	/* create overall sub-directory for the encoder */
	sde_enc->debugfs_root = debugfs_create_dir(name,
			drm_enc->dev->primary->debugfs_root);
	if (!sde_enc->debugfs_root)
		return -ENOMEM;

	/* don't error check these */
	debugfs_create_file("status", 0600,
		sde_enc->debugfs_root, sde_enc, &debugfs_status_fops);

	debugfs_create_file("misr_data", 0600,
		sde_enc->debugfs_root, sde_enc, &debugfs_misr_fops);

	debugfs_create_bool("idle_power_collapse", 0600, sde_enc->debugfs_root,
			&sde_enc->idle_pc_enabled);

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
		u32 display_caps,
		struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;

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
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
		++sde_enc->num_phys_encs;
	}

	if (display_caps & MSM_DISPLAY_CAP_CMD_MODE) {
		enc = sde_encoder_phys_cmd_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			SDE_ERROR_ENC(sde_enc, "failed to init cmd enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
		++sde_enc->num_phys_encs;
	}

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
		return enc == 0 ? -EINVAL : PTR_ERR(enc);
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
				sde_enc != 0, sde_kms != 0);
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

	SDE_DEBUG("dsi_info->num_of_h_tiles %d\n", disp_info->num_of_h_tiles);

	if ((disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) ||
	    (disp_info->capabilities & MSM_DISPLAY_CAP_VID_MODE))
		sde_enc->idle_pc_enabled = sde_kms->catalog->has_idle_pc;

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
						disp_info->capabilities,
						sde_enc,
						&phys_params);
			if (ret)
				SDE_ERROR_ENC(sde_enc,
						"failed to add phys encs\n");
		}
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			atomic_set(&phys->vsync_cnt, 0);
			atomic_set(&phys->underrun_cnt, 0);
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

struct drm_encoder *sde_encoder_init(
		struct drm_device *dev,
		struct msm_display_info *disp_info)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	char name[SDE_NAME_SIZE];
	int ret = 0;

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	mutex_init(&sde_enc->enc_lock);
	ret = sde_encoder_setup_display(sde_enc, sde_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	sde_enc->cur_master = NULL;
	spin_lock_init(&sde_enc->enc_spinlock);
	mutex_init(&sde_enc->vblank_ctl_lock);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs, drm_enc_mode, NULL);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);

	if ((disp_info->intf_type == DRM_MODE_CONNECTOR_DSI) &&
			disp_info->is_primary)
		setup_timer(&sde_enc->vsync_event_timer,
				sde_encoder_vsync_event_handler,
				(unsigned long)sde_enc);

	snprintf(name, SDE_NAME_SIZE, "rsc_enc%u", drm_enc->base.id);
	sde_enc->rsc_client = sde_rsc_client_create(SDE_RSC_INDEX, name,
					disp_info->is_primary);
	if (IS_ERR_OR_NULL(sde_enc->rsc_client)) {
		SDE_DEBUG("sde rsc client create failed :%ld\n",
						PTR_ERR(sde_enc->rsc_client));
		sde_enc->rsc_client = NULL;
	}

	if (disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) {
		ret = _sde_encoder_input_handler(sde_enc);
		if (ret)
			SDE_ERROR(
			"input handler registration failed, rc = %d\n", ret);
	}

	mutex_init(&sde_enc->rc_lock);
	kthread_init_delayed_work(&sde_enc->delayed_off_work,
			sde_encoder_off_work);
	sde_enc->vblank_enabled = false;

	kthread_init_work(&sde_enc->vsync_event_work,
			sde_encoder_vsync_event_work_handler);

	kthread_init_work(&sde_enc->input_event_work,
			sde_encoder_input_event_work_handler);

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

int sde_encoder_wait_for_event(struct drm_encoder *drm_enc,
	enum msm_event_wait event)
{
	int (*fn_wait)(struct sde_encoder_phys *phys_enc) = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;

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
		};

		if (phys && fn_wait) {
			SDE_ATRACE_BEGIN("wait_for_completion_event");
			ret = fn_wait(phys);
			SDE_ATRACE_END("wait_for_completion_event");
			if (ret)
				return ret;
		}
	}

	return ret;
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

/**
 * sde_encoder_update_caps_for_cont_splash - update encoder settings during
 *	device bootup when cont_splash is enabled
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:	true if successful in updating the encoder structure
 */
int sde_encoder_update_caps_for_cont_splash(struct drm_encoder *encoder)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_connector *conn = NULL;
	struct sde_connector *sde_conn = NULL;
	struct sde_connector_state *sde_conn_state = NULL;
	struct drm_display_mode *drm_mode = NULL;
	struct sde_rm_hw_iter dsc_iter, pp_iter, ctl_iter, intf_iter;
	struct sde_encoder_phys *phys_enc;
	int ret = 0, i, idx;

	if (!encoder) {
		SDE_ERROR("invalid drm enc\n");
		return -EINVAL;
	}

	if (!encoder->dev || !encoder->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}

	priv = encoder->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);
	sde_enc = to_sde_encoder_virt(encoder);
	if (!priv->num_connectors) {
		SDE_ERROR_ENC(sde_enc, "No connectors registered\n");
		return -EINVAL;
	}
	SDE_DEBUG_ENC(sde_enc,
			"num of connectors: %d\n", priv->num_connectors);

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

	ret = sde_conn->ops.get_mode_info(&sde_conn->base,
			&encoder->crtc->state->adjusted_mode,
			&sde_conn_state->mode_info,
			sde_kms->catalog->max_mixer_width,
			sde_conn->display);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
			"conn: ->get_mode_info failed. ret=%d\n", ret);
		return ret;
	}

	ret = sde_rm_reserve(&sde_kms->rm, encoder, encoder->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
			"failed to reserve hw resources, %d\n", ret);
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

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys) {
			SDE_ERROR_ENC(sde_enc,
				"phys encoders not initialized\n");
			return -EINVAL;
		}

		/* update connector for master and slave phys encoders */
		phys->connector = conn;
		phys->cont_splash_single_flush =
			sde_kms->splash_data.single_flush_en;
		phys->cont_splash_settings = true;

		phys->hw_pp = sde_enc->hw_pp[i];
		if (phys->ops.cont_splash_mode_set)
			phys->ops.cont_splash_mode_set(phys, drm_mode);

		if (phys->ops.is_master && phys->ops.is_master(phys))
			sde_enc->cur_master = phys;
	}

	return ret;
}

int sde_encoder_display_failure_notification(struct drm_encoder *enc)
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

	kthread_queue_work(&event_thread->worker,
			   &sde_enc->esd_trigger_work);
	kthread_flush_work(&sde_enc->esd_trigger_work);

	/**
	 * panel may stop generating te signal (vsync) during esd failure. rsc
	 * hardware may hang without vsync. Avoid rsc hang by generating the
	 * vsync from watchdog timer instead of panel.
	 */
	_sde_encoder_switch_to_watchdog_vsync(enc);

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
