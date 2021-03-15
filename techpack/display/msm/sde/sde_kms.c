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

#include <drm/drm_crtc.h>
#include <drm/drm_fixed.h>
#include <drm/drm_panel.h>
#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-buf.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>
#include <soc/qcom/scm.h>

#include "msm_drv.h"
#include "msm_mmu.h"
#include "msm_gem.h"

#include "dsi_display.h"
#include "dsi_drm.h"
#include "sde_wb.h"
#include "dp_display.h"
#include "dp_drm.h"

#include "sde_kms.h"
#include "sde_core_irq.h"
#include "sde_formats.h"
#include "sde_hw_vbif.h"
#include "sde_vbif.h"
#include "sde_encoder.h"
#include "sde_plane.h"
#include "sde_crtc.h"
#include "sde_reg_dma.h"
#include "sde_connector.h"

#include <soc/qcom/scm.h>
#include "soc/qcom/secure_buffer.h"
#include "soc/qcom/qtee_shmbridge.h"

#define CREATE_TRACE_POINTS
#include "sde_trace.h"

/* defines for secure channel call */
#define MEM_PROTECT_SD_CTRL_SWITCH 0x18
#define MDP_DEVICE_ID            0x1A

#define TCSR_DISP_HF_SF_ARES_GLITCH_MASK        0x01FCA084

static const char * const iommu_ports[] = {
		"mdp_0",
};

/**
 * Controls size of event log buffer. Specified as a power of 2.
 */
#define SDE_EVTLOG_SIZE	1024

/*
 * To enable overall DRM driver logging
 * # echo 0x2 > /sys/module/drm/parameters/debug
 *
 * To enable DRM driver h/w logging
 * # echo <mask> > /sys/kernel/debug/dri/0/debug/hw_log_mask
 *
 * See sde_hw_mdss.h for h/w logging mask definitions (search for SDE_DBG_MASK_)
 */
#define SDE_DEBUGFS_DIR "msm_sde"
#define SDE_DEBUGFS_HWMASKNAME "hw_log_mask"

#define SDE_KMS_MODESET_LOCK_TIMEOUT_US 500
#define SDE_KMS_MODESET_LOCK_MAX_TRIALS 20

#define SDE_KMS_PM_QOS_CPU_DMA_LATENCY 300

/**
 * sdecustom - enable certain driver customizations for sde clients
 *	Enabling this modifies the standard DRM behavior slightly and assumes
 *	that the clients have specific knowledge about the modifications that
 *	are involved, so don't enable this unless you know what you're doing.
 *
 *	Parts of the driver that are affected by this setting may be located by
 *	searching for invocations of the 'sde_is_custom_client()' function.
 *
 *	This is disabled by default.
 */
static bool sdecustom = true;
module_param(sdecustom, bool, 0400);
MODULE_PARM_DESC(sdecustom, "Enable customizations for sde clients");

static int sde_kms_hw_init(struct msm_kms *kms);
static int _sde_kms_mmu_destroy(struct sde_kms *sde_kms);
static int _sde_kms_mmu_init(struct sde_kms *sde_kms);
static int _sde_kms_register_events(struct msm_kms *kms,
		struct drm_mode_object *obj, u32 event, bool en);
bool sde_is_custom_client(void)
{
	return sdecustom;
}

#ifdef CONFIG_DEBUG_FS
void *sde_debugfs_get_root(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev_private)
		return NULL;

	priv = sde_kms->dev->dev_private;
	return priv->debug_root;
}

static int _sde_debugfs_init(struct sde_kms *sde_kms)
{
	void *p;
	int rc;
	void *debugfs_root;

	p = sde_hw_util_get_log_mask_ptr();

	if (!sde_kms || !p)
		return -EINVAL;

	debugfs_root = sde_debugfs_get_root(sde_kms);
	if (!debugfs_root)
		return -EINVAL;

	/* allow debugfs_root to be NULL */
	debugfs_create_x32(SDE_DEBUGFS_HWMASKNAME, 0600, debugfs_root, p);

	(void) sde_debugfs_vbif_init(sde_kms, debugfs_root);
	(void) sde_debugfs_core_irq_init(sde_kms, debugfs_root);

	rc = sde_core_perf_debugfs_init(&sde_kms->perf, debugfs_root);
	if (rc) {
		SDE_ERROR("failed to init perf %d\n", rc);
		return rc;
	}

	if (sde_kms->catalog->qdss_count)
		debugfs_create_u32("qdss", 0600, debugfs_root,
				(u32 *)&sde_kms->qdss_enabled);

	return 0;
}

static void _sde_debugfs_destroy(struct sde_kms *sde_kms)
{
	/* don't need to NULL check debugfs_root */
	if (sde_kms) {
		sde_debugfs_vbif_destroy(sde_kms);
		sde_debugfs_core_irq_destroy(sde_kms);
	}
}
#else
static int _sde_debugfs_init(struct sde_kms *sde_kms)
{
	return 0;
}

static void _sde_debugfs_destroy(struct sde_kms *sde_kms)
{
}
#endif

static int sde_kms_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	int ret = 0;

	SDE_ATRACE_BEGIN("sde_kms_enable_vblank");
	ret = sde_crtc_vblank(crtc, true);
	SDE_ATRACE_END("sde_kms_enable_vblank");

	return ret;
}

static void sde_kms_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	SDE_ATRACE_BEGIN("sde_kms_disable_vblank");
	sde_crtc_vblank(crtc, false);
	SDE_ATRACE_END("sde_kms_disable_vblank");
}

static void sde_kms_wait_for_frame_transfer_complete(struct msm_kms *kms,
		struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	int ret;

	if (!kms || !crtc || !crtc->state || !crtc->dev) {
		SDE_ERROR("invalid params\n");
		return;
	}

	if (!crtc->state->enable) {
		SDE_DEBUG("[crtc:%d] not enable\n", crtc->base.id);
		return;
	}

	if (!crtc->state->active) {
		SDE_DEBUG("[crtc:%d] not active\n", crtc->base.id);
		return;
	}

	dev = crtc->dev;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		/*
		 * Video Mode - Wait for VSYNC
		 * Cmd Mode   - Wait for PP_DONE. Will be no-op if transfer is
		 *              complete
		 */
		SDE_EVT32_VERBOSE(DRMID(crtc));
		ret = sde_encoder_wait_for_event(encoder, MSM_ENC_TX_COMPLETE);
		if (ret && ret != -EWOULDBLOCK) {
			SDE_ERROR(
			"[crtc: %d][enc: %d] wait for commit done returned %d\n",
			crtc->base.id, encoder->base.id, ret);
			break;
		}
	}
}

static int _sde_kms_secure_ctrl_xin_clients(struct sde_kms *sde_kms,
			struct drm_crtc *crtc, bool enable)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_mdss_cfg *sde_cfg;
	struct drm_plane *plane;
	int i, ret;

	dev = sde_kms->dev;
	priv = dev->dev_private;
	sde_cfg = sde_kms->catalog;

	ret = sde_vbif_halt_xin_mask(sde_kms,
			sde_cfg->sui_block_xin_mask, enable);
	if (ret) {
		SDE_ERROR("failed to halt some xin-clients, ret:%d\n", ret);
		return ret;
	}

	if (enable) {
		for (i = 0; i < priv->num_planes; i++) {
			plane = priv->planes[i];
			sde_plane_secure_ctrl_xin_client(plane, crtc);
		}
	}

	return 0;
}

/**
 * _sde_kms_scm_call - makes secure channel call to switch the VMIDs
 * @sde_kms: Pointer to sde_kms struct
 * @vimd: switch the stage 2 translation to this VMID
 */
static int _sde_kms_scm_call(struct sde_kms *sde_kms, int vmid)
{
	struct scm_desc desc = {0};
	uint32_t num_sids;
	uint32_t *sec_sid;
	uint32_t mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_SWITCH;
	struct sde_mdss_cfg *sde_cfg = sde_kms->catalog;
	int ret = 0, i;
	struct qtee_shm shm;
	bool qtee_en = qtee_shmbridge_is_enabled();

	num_sids = sde_cfg->sec_sid_mask_count;
	if (!num_sids) {
		SDE_ERROR("secure SID masks not configured, vmid 0x%x\n", vmid);
		return -EINVAL;
	}

	if (qtee_en) {
		ret = qtee_shmbridge_allocate_shm(num_sids * sizeof(uint32_t),
			&shm);
		if (ret)
			return -ENOMEM;

		sec_sid = (uint32_t *) shm.vaddr;
		desc.args[1] = shm.paddr;
		/**
		 * SMMUSecureModeSwitch requires the size to be number of SID's
		 * but shm allocates size in pages. Modify the args as per
		 * client requirement.
		 */
		desc.args[2] = sizeof(uint32_t) * num_sids;
	} else {
		sec_sid = kcalloc(num_sids, sizeof(uint32_t), GFP_KERNEL);
		if (!sec_sid)
			return -ENOMEM;

		desc.args[1] = SCM_BUFFER_PHYS(sec_sid);
		desc.args[2] = sizeof(uint32_t) * num_sids;
	}

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
	desc.args[0] = MDP_DEVICE_ID;
	desc.args[3] =  vmid;

	for (i = 0; i < num_sids; i++) {
		sec_sid[i] = sde_cfg->sec_sid_mask[i];
		SDE_DEBUG("sid_mask[%d]: %d\n", i, sec_sid[i]);
	}
	dmac_flush_range(sec_sid, sec_sid + num_sids);

	SDE_DEBUG("calling scm_call for vmid 0x%x, num_sids %d, qtee_en %d",
				vmid, num_sids, qtee_en);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				mem_protect_sd_ctrl_id), &desc);
	if (ret)
		SDE_ERROR("Error:scm_call2, vmid %lld, ret%d\n",
				desc.args[3], ret);
	SDE_EVT32(mem_protect_sd_ctrl_id, desc.args[0], desc.args[2],
			desc.args[3], qtee_en, num_sids, ret);

	if (qtee_en)
		qtee_shmbridge_free_shm(&shm);
	else
		kfree(sec_sid);

	return ret;
}

static int _sde_kms_detach_all_cb(struct sde_kms *sde_kms, u32 vmid)
{
	u32 ret;

	if (atomic_inc_return(&sde_kms->detach_all_cb) > 1)
		return 0;

	/* detach_all_contexts */
	ret = sde_kms_mmu_detach(sde_kms, false);
	if (ret) {
		SDE_ERROR("failed to detach all cb ret:%d\n", ret);
		goto mmu_error;
	}

	ret = _sde_kms_scm_call(sde_kms, vmid);
	if (ret) {
		SDE_ERROR("scm call failed for vmid:%d\n", vmid);
		goto scm_error;
	}

	return 0;

scm_error:
	sde_kms_mmu_attach(sde_kms, false);
mmu_error:
	atomic_dec(&sde_kms->detach_all_cb);
	return ret;
}

static int _sde_kms_attach_all_cb(struct sde_kms *sde_kms, u32 vmid,
		u32 old_vmid)
{
	u32 ret;

	if (atomic_dec_return(&sde_kms->detach_all_cb) != 0)
		return 0;

	ret = _sde_kms_scm_call(sde_kms, vmid);
	if (ret) {
		SDE_ERROR("scm call failed for vmid:%d\n", vmid);
		goto scm_error;
	}

	/* attach_all_contexts */
	ret = sde_kms_mmu_attach(sde_kms, false);
	if (ret) {
		SDE_ERROR("failed to attach all cb ret:%d\n", ret);
		goto mmu_error;
	}

	return 0;

mmu_error:
	_sde_kms_scm_call(sde_kms, old_vmid);
scm_error:
	atomic_inc(&sde_kms->detach_all_cb);
	return ret;
}

static int _sde_kms_detach_sec_cb(struct sde_kms *sde_kms, int vmid)
{
	u32 ret;

	if (atomic_inc_return(&sde_kms->detach_sec_cb) > 1)
		return 0;

	/* detach secure_context */
	ret = sde_kms_mmu_detach(sde_kms, true);
	if (ret) {
		SDE_ERROR("failed to detach sec cb ret:%d\n", ret);
		goto mmu_error;
	}

	ret = _sde_kms_scm_call(sde_kms, vmid);
	if (ret) {
		SDE_ERROR("scm call failed for vmid:%d\n", vmid);
		goto scm_error;
	}

	return 0;

scm_error:
	sde_kms_mmu_attach(sde_kms, true);
mmu_error:
	atomic_dec(&sde_kms->detach_sec_cb);
	return ret;
}

static int _sde_kms_attach_sec_cb(struct sde_kms *sde_kms, u32 vmid,
		u32 old_vmid)
{
	u32 ret;

	if (atomic_dec_return(&sde_kms->detach_sec_cb) != 0)
		return 0;

	ret = _sde_kms_scm_call(sde_kms, vmid);
	if (ret) {
		goto scm_error;
		SDE_ERROR("scm call failed for vmid:%d\n", vmid);
	}

	ret = sde_kms_mmu_attach(sde_kms, true);
	if (ret) {
		SDE_ERROR("failed to attach sec cb ret:%d\n", ret);
		goto mmu_error;
	}

	return 0;

mmu_error:
	_sde_kms_scm_call(sde_kms, old_vmid);
scm_error:
	atomic_inc(&sde_kms->detach_sec_cb);
	return ret;
}

static int _sde_kms_sui_misr_ctrl(struct sde_kms *sde_kms,
		struct drm_crtc *crtc, bool enable)
{
	int ret;

	if (enable) {
		ret = pm_runtime_get_sync(sde_kms->dev->dev);
		if (ret < 0) {
			SDE_ERROR("failed to enable resource, ret:%d\n", ret);
			return ret;
		}

		sde_crtc_misr_setup(crtc, true, 1);

		ret = _sde_kms_secure_ctrl_xin_clients(sde_kms, crtc, true);
		if (ret) {
			sde_crtc_misr_setup(crtc, false, 0);
			pm_runtime_put_sync(sde_kms->dev->dev);
			return ret;
		}

	} else {
		_sde_kms_secure_ctrl_xin_clients(sde_kms, crtc, false);
		sde_crtc_misr_setup(crtc, false, 0);
		pm_runtime_put_sync(sde_kms->dev->dev);
	}

	return 0;
}

static int _sde_kms_secure_ctrl(struct sde_kms *sde_kms, struct drm_crtc *crtc,
		bool post_commit)
{
	struct sde_kms_smmu_state_data *smmu_state = &sde_kms->smmu_state;
	int old_smmu_state = smmu_state->state;
	int ret = 0;
	u32 vmid;

	if (!sde_kms || !crtc) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->transition_type,
			post_commit, smmu_state->sui_misr_state,
			smmu_state->secure_level, SDE_EVTLOG_FUNC_ENTRY);

	if ((!smmu_state->transition_type) ||
	    ((smmu_state->transition_type == POST_COMMIT) && !post_commit))
		/* Bail out */
		return 0;

	/* enable sui misr if requested, before the transition */
	if (smmu_state->sui_misr_state == SUI_MISR_ENABLE_REQ) {
		ret = _sde_kms_sui_misr_ctrl(sde_kms, crtc, true);
		if (ret) {
			smmu_state->sui_misr_state == NONE;
			goto end;
		}
	}

	mutex_lock(&sde_kms->secure_transition_lock);
	switch (smmu_state->state) {
	case DETACH_ALL_REQ:
		ret = _sde_kms_detach_all_cb(sde_kms, VMID_CP_SEC_DISPLAY);
		if (!ret)
			smmu_state->state = DETACHED;
		break;

	case ATTACH_ALL_REQ:
		ret = _sde_kms_attach_all_cb(sde_kms, VMID_CP_PIXEL,
				VMID_CP_SEC_DISPLAY);
		if (!ret) {
			smmu_state->state = ATTACHED;
			smmu_state->secure_level = SDE_DRM_SEC_NON_SEC;
		}
		break;

	case DETACH_SEC_REQ:
		vmid = (smmu_state->secure_level == SDE_DRM_SEC_ONLY) ?
				VMID_CP_SEC_DISPLAY : VMID_CP_CAMERA_PREVIEW;

		ret = _sde_kms_detach_sec_cb(sde_kms, vmid);
		if (!ret)
			smmu_state->state = DETACHED_SEC;
		break;

	case ATTACH_SEC_REQ:
		vmid = (smmu_state->secure_level == SDE_DRM_SEC_ONLY) ?
				VMID_CP_SEC_DISPLAY : VMID_CP_CAMERA_PREVIEW;
		ret = _sde_kms_attach_sec_cb(sde_kms, VMID_CP_PIXEL, vmid);
		if (!ret) {
			smmu_state->state = ATTACHED;
			smmu_state->secure_level = SDE_DRM_SEC_NON_SEC;
		}
		break;

	default:
		SDE_ERROR("crtc%d: invalid smmu state %d transition type %d\n",
			DRMID(crtc), smmu_state->state,
			smmu_state->transition_type);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&sde_kms->secure_transition_lock);

	/* disable sui misr if requested, after the transition */
	if (!ret && (smmu_state->sui_misr_state == SUI_MISR_DISABLE_REQ)) {
		ret = _sde_kms_sui_misr_ctrl(sde_kms, crtc, false);
		if (ret)
			goto end;
	}

end:
	smmu_state->transition_error = false;

	if (ret) {
		smmu_state->transition_error = true;
		SDE_ERROR(
		  "crtc%d: req_state %d, new_state %d, sec_lvl %d, ret %d\n",
			DRMID(crtc), old_smmu_state, smmu_state->state,
			smmu_state->secure_level, ret);

		smmu_state->state = smmu_state->prev_state;
		smmu_state->secure_level = smmu_state->prev_secure_level;

		if (smmu_state->sui_misr_state == SUI_MISR_ENABLE_REQ)
			_sde_kms_sui_misr_ctrl(sde_kms, crtc, false);
	}

	SDE_DEBUG("crtc %d: req_state %d, new_state %d, sec_lvl %d, ret %d\n",
			DRMID(crtc), old_smmu_state, smmu_state->state,
			smmu_state->secure_level, ret);
	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->prev_state,
			smmu_state->transition_type,
			smmu_state->transition_error,
			smmu_state->secure_level, smmu_state->prev_secure_level,
			smmu_state->sui_misr_state, ret, SDE_EVTLOG_FUNC_EXIT);

	smmu_state->sui_misr_state = NONE;
	smmu_state->transition_type = NONE;

	return ret;
}

static int sde_kms_prepare_secure_transition(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_plane_state *old_plane_state, *new_plane_state;

	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	int i, ops = 0, ret = 0;
	bool old_valid_fb = false;
	struct sde_kms_smmu_state_data *smmu_state = &sde_kms->smmu_state;

	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		if (!crtc->state || !crtc->state->active)
			continue;
		/*
		 * It is safe to assume only one active crtc,
		 * and compatible translation modes on the
		 * planes staged on this crtc.
		 * otherwise validation would have failed.
		 * For this CRTC,
		 */

		/*
		 * 1. Check if old state on the CRTC has planes
		 * staged with valid fbs
		 */
		for_each_old_plane_in_state(state, plane, plane_state, i) {
			if (!plane_state->crtc)
				continue;
			if (plane_state->fb) {
				old_valid_fb = true;
				break;
			}
		}

		/*
		 * 2.Get the operations needed to be performed before
		 * secure transition can be initiated.
		 */
		ops = sde_crtc_get_secure_transition_ops(crtc,
				old_crtc_state, old_valid_fb);
		if (ops < 0) {
			SDE_ERROR("invalid secure operations %x\n", ops);
			return ops;
		}

		if (!ops) {
			smmu_state->transition_error = false;
			goto no_ops;
		}

		SDE_DEBUG("%d:secure operations(%x) started on state:%pK\n",
				crtc->base.id, ops, crtc->state);
		SDE_EVT32(DRMID(crtc), ops, crtc->state, old_valid_fb);

		/* 3. Perform operations needed for secure transition */
		if  (ops & SDE_KMS_OPS_WAIT_FOR_TX_DONE) {
			SDE_DEBUG("wait_for_transfer_done\n");
			sde_kms_wait_for_frame_transfer_complete(kms, crtc);
		}
		if (ops & SDE_KMS_OPS_CLEANUP_PLANE_FB) {
			SDE_DEBUG("cleanup planes\n");
			drm_atomic_helper_cleanup_planes(dev, state);
			for_each_oldnew_plane_in_state(state, plane,
					old_plane_state, new_plane_state, i)
				sde_plane_destroy_fb(old_plane_state);
		}
		if (ops & SDE_KMS_OPS_SECURE_STATE_CHANGE) {
			SDE_DEBUG("secure ctrl\n");
			_sde_kms_secure_ctrl(sde_kms, crtc, false);
		}
		if (ops & SDE_KMS_OPS_PREPARE_PLANE_FB) {
			SDE_DEBUG("prepare planes %d",
					crtc->state->plane_mask);
			drm_atomic_crtc_for_each_plane(plane,
					crtc) {
				const struct drm_plane_helper_funcs *funcs;

				plane_state = plane->state;
				funcs = plane->helper_private;

				SDE_DEBUG("psde:%d FB[%u]\n",
						plane->base.id,
						plane->fb->base.id);
				if (!funcs)
					continue;

				if (funcs->prepare_fb(plane, plane_state)) {
					ret = funcs->prepare_fb(plane,
							plane_state);
					if (ret)
						return ret;
				}
			}
		}
		SDE_EVT32(DRMID(crtc), SDE_EVTLOG_FUNC_EXIT);
		SDE_DEBUG("secure operations completed\n");
	}

no_ops:
	return 0;
}

static void _sde_clear_boot_config(struct sde_boot_config *boot_cfg)
{
	if (!boot_cfg)
		return;

	SDE_IMEM_WRITE(&boot_cfg->header, 0x0);
	SDE_IMEM_WRITE(&boot_cfg->addr1, 0x0);
	SDE_IMEM_WRITE(&boot_cfg->addr2, 0x0);
}

static int _sde_kms_release_splash_buffer(struct sde_kms *sde_kms,
					unsigned int mem_addr,
					unsigned int splash_buffer_size,
					unsigned int ramdump_base,
					unsigned int ramdump_buffer_size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;
	int ret = 0;
	struct sde_boot_config *boot_cfg = sde_kms->imem;

	if (!mem_addr || !splash_buffer_size) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	/* leave ramdump memory only if base address matches */
	if (ramdump_base == mem_addr &&
			ramdump_buffer_size <= splash_buffer_size) {
		mem_addr +=  ramdump_buffer_size;
		splash_buffer_size -= ramdump_buffer_size;
	}

	if (!ramdump_base)
		_sde_clear_boot_config(boot_cfg);

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + splash_buffer_size) >> PAGE_SHIFT;

	ret = memblock_free(mem_addr, splash_buffer_size);
	if (ret) {
		SDE_ERROR("continuous splash memory free failed:%d\n", ret);
		return ret;
	}
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));

	return ret;

}

static int _sde_kms_splash_mem_get(struct sde_kms *sde_kms,
		struct sde_splash_mem *splash)
{
	struct msm_mmu *mmu = NULL;
	int ret = 0;

	if (!sde_kms->aspace[0]) {
		SDE_ERROR("aspace not found for sde kms node\n");
		return -EINVAL;
	}

	mmu = sde_kms->aspace[0]->mmu;
	if (!mmu) {
		SDE_ERROR("mmu not found for aspace\n");
		return -EINVAL;
	}

	if (!splash || !mmu->funcs || !mmu->funcs->one_to_one_map) {
		SDE_ERROR("invalid input params for map\n");
		return -EINVAL;
	}

	if (!splash->ref_cnt) {
		ret = mmu->funcs->one_to_one_map(mmu, splash->splash_buf_base,
				splash->splash_buf_base,
				splash->splash_buf_size,
				IOMMU_READ | IOMMU_NOEXEC);
		if (ret)
			SDE_ERROR("splash memory smmu map failed:%d\n", ret);
	}

	splash->ref_cnt++;
	SDE_DEBUG("one2one mapping done for base:%lx size:%x ref_cnt:%d\n",
				splash->splash_buf_base,
				splash->splash_buf_size,
				splash->ref_cnt);

	return ret;
}

static int _sde_kms_map_all_splash_regions(struct sde_kms *sde_kms)
{
	int i = 0;
	int ret = 0;

	if (!sde_kms)
		return -EINVAL;

	for (i = 0; i < sde_kms->splash_data.num_splash_displays; i++) {
		ret = _sde_kms_splash_mem_get(sde_kms,
				sde_kms->splash_data.splash_display[i].splash);
		if (ret)
			return ret;
	}

	return ret;
}

static int _sde_kms_splash_mem_put(struct sde_kms *sde_kms,
		struct sde_splash_mem *splash)
{
	struct msm_mmu *mmu = NULL;
	int rc = 0;

	if (!sde_kms || !sde_kms->aspace[0] || !sde_kms->aspace[0]->mmu) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	mmu = sde_kms->aspace[0]->mmu;

	if (!splash || !splash->ref_cnt ||
			!mmu || !mmu->funcs || !mmu->funcs->one_to_one_unmap)
		return -EINVAL;

	splash->ref_cnt--;

	SDE_DEBUG("splash base:%lx refcnt:%d\n",
			splash->splash_buf_base, splash->ref_cnt);

	if (!splash->ref_cnt) {
		mmu->funcs->one_to_one_unmap(mmu, splash->splash_buf_base,
				splash->splash_buf_size);
		rc = _sde_kms_release_splash_buffer(sde_kms,
			splash->splash_buf_base, splash->splash_buf_size,
			splash->ramdump_base, splash->ramdump_size);
		splash->splash_buf_base = 0;
		splash->splash_buf_size = 0;
	}

	return rc;
}

static int _sde_kms_unmap_all_splash_regions(struct sde_kms *sde_kms)
{
	int i = 0;
	int ret = 0;

	if (!sde_kms)
		return -EINVAL;

	for (i = 0; i < sde_kms->splash_data.num_splash_displays; i++) {
		ret = _sde_kms_splash_mem_put(sde_kms,
				sde_kms->splash_data.splash_display[i].splash);
		if (ret)
			return ret;
	}

	return ret;
}

static int _sde_kms_get_blank(struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	int lp_mode, blank;

	if (crtc_state->active)
		lp_mode = sde_connector_get_property(conn_state,
							CONNECTOR_PROP_LP);
	else
		lp_mode = SDE_MODE_DPMS_OFF;

	switch (lp_mode) {
	case SDE_MODE_DPMS_ON:
		blank = DRM_PANEL_BLANK_UNBLANK;
		break;
	case SDE_MODE_DPMS_LP1:
	case SDE_MODE_DPMS_LP2:
		blank = DRM_PANEL_BLANK_LP;
		break;
	case SDE_MODE_DPMS_OFF:
	default:
		blank = DRM_PANEL_BLANK_POWERDOWN;
		break;
	}

	return blank;
}

static void _sde_kms_drm_check_dpms(struct drm_atomic_state *old_state,
			unsigned long event)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc *crtc;
	int i, old_mode, new_mode, old_fps, new_fps;

	for_each_old_connector_in_state(old_state, connector,
			old_conn_state, i) {
		crtc = connector->state->crtc ? connector->state->crtc :
			old_conn_state->crtc;
		if (!crtc)
			continue;

		new_fps = crtc->state->mode.vrefresh;
		new_mode = _sde_kms_get_blank(crtc->state, connector->state);
		if (old_conn_state->crtc) {
			old_crtc_state = drm_atomic_get_existing_crtc_state(
					old_state, old_conn_state->crtc);

			old_fps = old_crtc_state->mode.vrefresh;
			old_mode = _sde_kms_get_blank(old_crtc_state,
							old_conn_state);
		} else {
			old_fps = 0;
			old_mode = DRM_PANEL_BLANK_POWERDOWN;
		}

		if ((old_mode != new_mode) || (old_fps != new_fps)) {
			struct drm_panel_notifier notifier_data;

			SDE_EVT32(old_mode, new_mode, old_fps, new_fps,
				connector->panel, crtc->state->active,
				old_conn_state->crtc, event);
			pr_debug("change detected (power mode %d->%d, fps %d->%d)\n",
				old_mode, new_mode, old_fps, new_fps);

			/* If suspend resume and fps change are happening
			 * at the same time, give preference to power mode
			 * changes rather than fps change.
			 */

			if ((old_mode == new_mode) && (old_fps != new_fps))
				new_mode = DRM_PANEL_BLANK_FPS_CHANGE;

			notifier_data.data = &new_mode;
			notifier_data.refresh_rate = new_fps;
			notifier_data.id = connector->base.id;

			if (connector->panel)
				drm_panel_notifier_call_chain(connector->panel,
							event, &notifier_data);
		}
	}
}

static void sde_kms_prepare_commit(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms;
	struct msm_drm_private *priv;
	struct drm_device *dev;
	struct drm_encoder *encoder;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i, rc;

	if (!kms)
		return;
	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	if (!dev || !dev->dev_private)
		return;
	priv = dev->dev_private;

	SDE_ATRACE_BEGIN("prepare_commit");
	rc = pm_runtime_get_sync(sde_kms->dev->dev);
	if (rc < 0) {
		SDE_ERROR("failed to enable power resources %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		goto end;
	}

	if (sde_kms->first_kickoff) {
		sde_power_scale_reg_bus(&priv->phandle, VOTE_INDEX_HIGH, false);
		sde_kms->first_kickoff = false;
	}

	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		list_for_each_entry(encoder, &dev->mode_config.encoder_list,
				head) {
			if (encoder->crtc != crtc)
				continue;

			if (sde_encoder_prepare_commit(encoder) == -ETIMEDOUT) {
				SDE_ERROR("crtc:%d, initiating hw reset\n",
						DRMID(crtc));
				sde_encoder_needs_hw_reset(encoder);
				sde_crtc_set_needs_hw_reset(crtc);
			}
		}
	}

	/*
	 * NOTE: for secure use cases we want to apply the new HW
	 * configuration only after completing preparation for secure
	 * transitions prepare below if any transtions is required.
	 */
	sde_kms_prepare_secure_transition(kms, state);

	_sde_kms_drm_check_dpms(state, DRM_PANEL_EARLY_EVENT_BLANK);
end:
	SDE_ATRACE_END("prepare_commit");
}

static void sde_kms_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct sde_kms *sde_kms;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	if (!kms || !old_state)
		return;
	sde_kms = to_sde_kms(kms);

	if (!sde_kms_power_resource_is_enabled(sde_kms->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	SDE_ATRACE_BEGIN("sde_kms_commit");
	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active) {
			SDE_EVT32(DRMID(crtc));
			sde_crtc_commit_kickoff(crtc, old_crtc_state);
		}
	}

	SDE_ATRACE_END("sde_kms_commit");
}

static void _sde_kms_free_splash_region(struct sde_kms *sde_kms,
		struct sde_splash_display *splash_display)
{
	if (!sde_kms || !splash_display ||
			!sde_kms->splash_data.num_splash_displays)
		return;

	_sde_kms_splash_mem_put(sde_kms, splash_display->splash);
	sde_kms->splash_data.num_splash_displays--;
	SDE_DEBUG("cont_splash handoff done, remaining:%d\n",
				sde_kms->splash_data.num_splash_displays);
	memset(splash_display, 0x0, sizeof(struct sde_splash_display));
}

static void _sde_kms_release_splash_resource(struct sde_kms *sde_kms,
		struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;
	struct sde_splash_display *splash_display;
	int i;

	if (!sde_kms || !crtc)
		return;

	priv = sde_kms->dev->dev_private;

	if (!crtc->state->active || !sde_kms->splash_data.num_splash_displays)
		return;

	SDE_EVT32(DRMID(crtc), crtc->state->active,
			sde_kms->splash_data.num_splash_displays);

	for (i = 0; i < MAX_DSI_DISPLAYS; i++) {
		splash_display = &sde_kms->splash_data.splash_display[i];
		if (splash_display->encoder &&
				crtc == splash_display->encoder->crtc)
			break;
	}

	if (i >= MAX_DSI_DISPLAYS)
		return;

	if (splash_display->cont_splash_enabled) {
		sde_encoder_update_caps_for_cont_splash(splash_display->encoder,
				splash_display, false);
		_sde_kms_free_splash_region(sde_kms, splash_display);
	}

	/* remove the votes if all displays are done with splash */
	if (!sde_kms->splash_data.num_splash_displays) {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
			sde_power_data_bus_set_quota(&priv->phandle, i,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);

		pm_runtime_put_sync(sde_kms->dev->dev);
	}
}

static void sde_kms_check_for_ext_vote(struct sde_kms *sde_kms,
		struct sde_power_handle *phandle)
{
	struct sde_crtc *sde_crtc;
	struct drm_crtc *crtc;
	struct drm_device *dev;
	bool crtc_enabled = false;

	if (!sde_kms->catalog->allow_gdsc_toggle)
		return;

	dev = sde_kms->dev;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		sde_crtc = to_sde_crtc(crtc);
		if (sde_crtc->enabled)
			crtc_enabled = true;
	}

	mutex_lock(&phandle->ext_client_lock);

	/* In some targets, a gdsc toggle is needed after crtc is disabled.
	 * There are some scenarios where presence of an external vote like
	 * secure vote which can prevent this from happening. In those
	 * cases, allow the target to go through a gdsc toggle after
	 * crtc is disabled.
	 */
	if (!crtc_enabled && phandle->is_ext_vote_en) {
		pm_runtime_put_sync(sde_kms->dev->dev);
		SDE_EVT32(phandle->is_ext_vote_en);
		pm_runtime_get_sync(sde_kms->dev->dev);
	}

	mutex_unlock(&phandle->ext_client_lock);
}

static void sde_kms_complete_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct sde_kms *sde_kms;
	struct msm_drm_private *priv;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct msm_display_conn_params params;
	int i, rc = 0;

	if (!kms || !old_state)
		return;
	sde_kms = to_sde_kms(kms);

	if (!sde_kms->dev || !sde_kms->dev->dev_private)
		return;
	priv = sde_kms->dev->dev_private;

	if (sde_kms_power_resource_is_enabled(sde_kms->dev) < 0) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	SDE_ATRACE_BEGIN("sde_kms_complete_commit");

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		sde_crtc_complete_commit(crtc, old_crtc_state);

		/* complete secure transitions if any */
		if (sde_kms->smmu_state.transition_type == POST_COMMIT)
			_sde_kms_secure_ctrl(sde_kms, crtc, true);
	}

	for_each_old_connector_in_state(old_state, connector,
			old_conn_state, i) {
		struct sde_connector *c_conn;

		c_conn = to_sde_connector(connector);
		if (!c_conn->ops.post_kickoff)
			continue;

		memset(&params, 0, sizeof(params));

		sde_connector_complete_qsync_commit(connector, &params);

		rc = c_conn->ops.post_kickoff(connector, &params);
		if (rc) {
			pr_err("Connector Post kickoff failed rc=%d\n",
					 rc);
		}

		sde_connector_fod_notify(connector);
	}

	_sde_kms_drm_check_dpms(old_state, DRM_PANEL_EVENT_BLANK);

	pm_runtime_put_sync(sde_kms->dev->dev);

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i)
		_sde_kms_release_splash_resource(sde_kms, crtc);

	sde_kms_check_for_ext_vote(sde_kms, &priv->phandle);

	SDE_EVT32_VERBOSE(SDE_EVTLOG_FUNC_EXIT);
	SDE_ATRACE_END("sde_kms_complete_commit");
}

static void sde_kms_wait_for_commit_done(struct msm_kms *kms,
		struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	int ret;

	if (!kms || !crtc || !crtc->state) {
		SDE_ERROR("invalid params\n");
		return;
	}

	dev = crtc->dev;

	if (!crtc->state->enable) {
		SDE_DEBUG("[crtc:%d] not enable\n", crtc->base.id);
		return;
	}

	if (!crtc->state->active) {
		SDE_DEBUG("[crtc:%d] not active\n", crtc->base.id);
		return;
	}

	if (!sde_kms_power_resource_is_enabled(crtc->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	SDE_ATRACE_BEGIN("sde_kms_wait_for_commit_done");
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		/*
		 * Wait for post-flush if necessary to delay before
		 * plane_cleanup. For example, wait for vsync in case of video
		 * mode panels. This may be a no-op for command mode panels.
		 */
		SDE_EVT32_VERBOSE(DRMID(crtc));
		ret = sde_encoder_wait_for_event(encoder, MSM_ENC_COMMIT_DONE);
		if (ret && ret != -EWOULDBLOCK) {
			SDE_ERROR("wait for commit done returned %d\n", ret);
			sde_crtc_request_frame_reset(crtc);
			break;
		}

		sde_crtc_complete_flip(crtc, NULL);
	}

	SDE_ATRACE_END("sde_ksm_wait_for_commit_done");
}

static void sde_kms_prepare_fence(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	if (!kms || !old_state || !old_state->dev || !old_state->acquire_ctx) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

	SDE_ATRACE_BEGIN("sde_kms_prepare_fence");

	/* old_state actually contains updated crtc pointers */
	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active || crtc->state->active_changed)
			sde_crtc_prepare_commit(crtc, old_crtc_state);
	}

	SDE_ATRACE_END("sde_kms_prepare_fence");
}

/**
 * _sde_kms_get_displays - query for underlying display handles and cache them
 * @sde_kms:    Pointer to sde kms structure
 * Returns:     Zero on success
 */
static int _sde_kms_get_displays(struct sde_kms *sde_kms)
{
	int rc = -ENOMEM;

	if (!sde_kms) {
		SDE_ERROR("invalid sde kms\n");
		return -EINVAL;
	}

	/* dsi */
	sde_kms->dsi_displays = NULL;
	sde_kms->dsi_display_count = dsi_display_get_num_of_displays();
	if (sde_kms->dsi_display_count) {
		sde_kms->dsi_displays = kcalloc(sde_kms->dsi_display_count,
				sizeof(void *),
				GFP_KERNEL);
		if (!sde_kms->dsi_displays) {
			SDE_ERROR("failed to allocate dsi displays\n");
			goto exit_deinit_dsi;
		}
		sde_kms->dsi_display_count =
			dsi_display_get_active_displays(sde_kms->dsi_displays,
					sde_kms->dsi_display_count);
	}

	/* wb */
	sde_kms->wb_displays = NULL;
	sde_kms->wb_display_count = sde_wb_get_num_of_displays();
	if (sde_kms->wb_display_count) {
		sde_kms->wb_displays = kcalloc(sde_kms->wb_display_count,
				sizeof(void *),
				GFP_KERNEL);
		if (!sde_kms->wb_displays) {
			SDE_ERROR("failed to allocate wb displays\n");
			goto exit_deinit_wb;
		}
		sde_kms->wb_display_count =
			wb_display_get_displays(sde_kms->wb_displays,
					sde_kms->wb_display_count);
	}

	/* dp */
	sde_kms->dp_displays = NULL;
	sde_kms->dp_display_count = dp_display_get_num_of_displays();
	if (sde_kms->dp_display_count) {
		sde_kms->dp_displays = kcalloc(sde_kms->dp_display_count,
				sizeof(void *), GFP_KERNEL);
		if (!sde_kms->dp_displays) {
			SDE_ERROR("failed to allocate dp displays\n");
			goto exit_deinit_dp;
		}
		sde_kms->dp_display_count =
			dp_display_get_displays(sde_kms->dp_displays,
					sde_kms->dp_display_count);

		sde_kms->dp_stream_count = dp_display_get_num_of_streams();
	}
	return 0;

exit_deinit_dp:
	kfree(sde_kms->dp_displays);
	sde_kms->dp_stream_count = 0;
	sde_kms->dp_display_count = 0;
	sde_kms->dp_displays = NULL;

exit_deinit_wb:
	kfree(sde_kms->wb_displays);
	sde_kms->wb_display_count = 0;
	sde_kms->wb_displays = NULL;

exit_deinit_dsi:
	kfree(sde_kms->dsi_displays);
	sde_kms->dsi_display_count = 0;
	sde_kms->dsi_displays = NULL;
	return rc;
}

/**
 * _sde_kms_release_displays - release cache of underlying display handles
 * @sde_kms:    Pointer to sde kms structure
 */
static void _sde_kms_release_displays(struct sde_kms *sde_kms)
{
	if (!sde_kms) {
		SDE_ERROR("invalid sde kms\n");
		return;
	}

	kfree(sde_kms->wb_displays);
	sde_kms->wb_displays = NULL;
	sde_kms->wb_display_count = 0;

	kfree(sde_kms->dsi_displays);
	sde_kms->dsi_displays = NULL;
	sde_kms->dsi_display_count = 0;
}

/**
 * _sde_kms_setup_displays - create encoders, bridges and connectors
 *                           for underlying displays
 * @dev:        Pointer to drm device structure
 * @priv:       Pointer to private drm device data
 * @sde_kms:    Pointer to sde kms structure
 * Returns:     Zero on success
 */
static int _sde_kms_setup_displays(struct drm_device *dev,
		struct msm_drm_private *priv,
		struct sde_kms *sde_kms)
{
	static const struct sde_connector_ops dsi_ops = {
		.set_info_blob = dsi_conn_set_info_blob,
		.detect =     dsi_conn_detect,
		.get_modes =  dsi_connector_get_modes,
		.pre_destroy =  dsi_connector_put_modes,
		.mode_valid = dsi_conn_mode_valid,
		.get_info =   dsi_display_get_info,
		.set_backlight = dsi_display_set_backlight,
		.soft_reset   = dsi_display_soft_reset,
		.pre_kickoff  = dsi_conn_pre_kickoff,
		.clk_ctrl = dsi_display_clk_ctrl,
		.set_power = dsi_display_set_power,
		.get_mode_info = dsi_conn_get_mode_info,
		.get_dst_format = dsi_display_get_dst_format,
		.post_kickoff = dsi_conn_post_kickoff,
		.check_status = dsi_display_check_status,
		.enable_event = dsi_conn_enable_event,
		.cmd_transfer = dsi_display_cmd_transfer,
		.cont_splash_config = dsi_display_cont_splash_config,
		.get_panel_vfp = dsi_display_get_panel_vfp,
		.get_default_lms = dsi_display_get_default_lms,
	};
	static const struct sde_connector_ops wb_ops = {
		.post_init =    sde_wb_connector_post_init,
		.set_info_blob = sde_wb_connector_set_info_blob,
		.detect =       sde_wb_connector_detect,
		.get_modes =    sde_wb_connector_get_modes,
		.set_property = sde_wb_connector_set_property,
		.get_info =     sde_wb_get_info,
		.soft_reset =   NULL,
		.get_mode_info = sde_wb_get_mode_info,
		.get_dst_format = NULL,
		.check_status = NULL,
		.cmd_transfer = NULL,
		.cont_splash_config = NULL,
		.get_panel_vfp = NULL,
	};
	static const struct sde_connector_ops dp_ops = {
		.post_init  = dp_connector_post_init,
		.detect     = dp_connector_detect,
		.get_modes  = dp_connector_get_modes,
		.atomic_check = dp_connector_atomic_check,
		.mode_valid = dp_connector_mode_valid,
		.get_info   = dp_connector_get_info,
		.get_mode_info  = dp_connector_get_mode_info,
		.post_open  = dp_connector_post_open,
		.check_status = NULL,
		.set_colorspace = dp_connector_set_colorspace,
		.config_hdr = dp_connector_config_hdr,
		.cmd_transfer = NULL,
		.cont_splash_config = NULL,
		.get_panel_vfp = NULL,
		.update_pps = dp_connector_update_pps,
	};
	struct msm_display_info info;
	struct drm_encoder *encoder;
	void *display, *connector;
	int i, max_encoders;
	int rc = 0;

	if (!dev || !priv || !sde_kms) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	max_encoders = sde_kms->dsi_display_count + sde_kms->wb_display_count +
				sde_kms->dp_display_count +
				sde_kms->dp_stream_count;
	if (max_encoders > ARRAY_SIZE(priv->encoders)) {
		max_encoders = ARRAY_SIZE(priv->encoders);
		SDE_ERROR("capping number of displays to %d", max_encoders);
	}

	/* dsi */
	for (i = 0; i < sde_kms->dsi_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->dsi_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = dsi_display_get_info(NULL, &info, display);
		if (rc) {
			SDE_ERROR("dsi get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("encoder init failed for dsi %d\n", i);
			continue;
		}

		rc = dsi_display_drm_bridge_init(display, encoder);
		if (rc) {
			SDE_ERROR("dsi bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
					encoder,
					dsi_display_get_drm_panel(display),
					display,
					&dsi_ops,
					DRM_CONNECTOR_POLL_HPD,
					DRM_MODE_CONNECTOR_DSI);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
			priv->connectors[priv->num_connectors++] = connector;
		} else {
			SDE_ERROR("dsi %d connector init failed\n", i);
			dsi_display_drm_bridge_deinit(display);
			sde_encoder_destroy(encoder);
			continue;
		}

		rc = dsi_display_drm_ext_bridge_init(display,
					encoder, connector);
		if (rc) {
			SDE_ERROR("dsi %d ext bridge init failed\n", rc);
			dsi_display_drm_bridge_deinit(display);
			sde_connector_destroy(connector);
			sde_encoder_destroy(encoder);
		}
	}


	/* wb */
	for (i = 0; i < sde_kms->wb_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->wb_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = sde_wb_get_info(NULL, &info, display);
		if (rc) {
			SDE_ERROR("wb get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("encoder init failed for wb %d\n", i);
			continue;
		}

		rc = sde_wb_drm_init(display, encoder);
		if (rc) {
			SDE_ERROR("wb bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
				encoder,
				0,
				display,
				&wb_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_VIRTUAL);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
			priv->connectors[priv->num_connectors++] = connector;
		} else {
			SDE_ERROR("wb %d connector init failed\n", i);
			sde_wb_drm_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}
	/* dp */
	for (i = 0; i < sde_kms->dp_display_count &&
			priv->num_encoders < max_encoders; ++i) {
		int idx;

		display = sde_kms->dp_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = dp_connector_get_info(NULL, &info, display);
		if (rc) {
			SDE_ERROR("dp get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("dp encoder init failed %d\n", i);
			continue;
		}

		rc = dp_drm_bridge_init(display, encoder);
		if (rc) {
			SDE_ERROR("dp bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
					encoder,
					NULL,
					display,
					&dp_ops,
					DRM_CONNECTOR_POLL_HPD,
					DRM_MODE_CONNECTOR_DisplayPort);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
			priv->connectors[priv->num_connectors++] = connector;
		} else {
			SDE_ERROR("dp %d connector init failed\n", i);
			dp_drm_bridge_deinit(display);
			sde_encoder_destroy(encoder);
		}

		/* update display cap to MST_MODE for DP MST encoders */
		info.capabilities |= MSM_DISPLAY_CAP_MST_MODE;
		sde_kms->dp_stream_count = dp_display_get_num_of_streams();
		for (idx = 0; idx < sde_kms->dp_stream_count &&
				priv->num_encoders < max_encoders; idx++) {
			info.h_tile_instance[0] = idx;
			encoder = sde_encoder_init(dev, &info);
			if (IS_ERR_OR_NULL(encoder)) {
				SDE_ERROR("dp mst encoder init failed %d\n", i);
				continue;
			}

			rc = dp_mst_drm_bridge_init(display, encoder);
			if (rc) {
				SDE_ERROR("dp mst bridge %d init failed, %d\n",
						i, rc);
				sde_encoder_destroy(encoder);
				continue;
			}
			priv->encoders[priv->num_encoders++] = encoder;
		}
	}

	return 0;
}

static void _sde_kms_drm_obj_destroy(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;
	int i;

	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	} else if (!sde_kms->dev) {
		SDE_ERROR("invalid dev\n");
		return;
	} else if (!sde_kms->dev->dev_private) {
		SDE_ERROR("invalid dev_private\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	for (i = 0; i < priv->num_crtcs; i++)
		priv->crtcs[i]->funcs->destroy(priv->crtcs[i]);
	priv->num_crtcs = 0;

	for (i = 0; i < priv->num_planes; i++)
		priv->planes[i]->funcs->destroy(priv->planes[i]);
	priv->num_planes = 0;

	for (i = 0; i < priv->num_connectors; i++)
		priv->connectors[i]->funcs->destroy(priv->connectors[i]);
	priv->num_connectors = 0;

	for (i = 0; i < priv->num_encoders; i++)
		priv->encoders[i]->funcs->destroy(priv->encoders[i]);
	priv->num_encoders = 0;

	_sde_kms_release_displays(sde_kms);
}

static int _sde_kms_drm_obj_init(struct sde_kms *sde_kms)
{
	struct drm_device *dev;
	struct drm_plane *primary_planes[MAX_PLANES], *plane;
	struct drm_crtc *crtc;

	struct msm_drm_private *priv;
	struct sde_mdss_cfg *catalog;

	int primary_planes_idx = 0, i, ret;
	int max_crtc_count;

	u32 sspp_id[MAX_PLANES];
	u32 master_plane_id[MAX_PLANES];
	u32 num_virt_planes = 0;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	dev = sde_kms->dev;
	priv = dev->dev_private;
	catalog = sde_kms->catalog;

	ret = sde_core_irq_domain_add(sde_kms);
	if (ret)
		goto fail_irq;
	/*
	 * Query for underlying display drivers, and create connectors,
	 * bridges and encoders for them.
	 */
	if (!_sde_kms_get_displays(sde_kms))
		(void)_sde_kms_setup_displays(dev, priv, sde_kms);

	max_crtc_count = min(catalog->mixer_count, priv->num_encoders);

	/* Create the planes */
	for (i = 0; i < catalog->sspp_count; i++) {
		bool primary = true;

		if (catalog->sspp[i].features & BIT(SDE_SSPP_CURSOR)
			|| primary_planes_idx >= max_crtc_count)
			primary = false;

		plane = sde_plane_init(dev, catalog->sspp[i].id, primary,
				(1UL << max_crtc_count) - 1, 0);
		if (IS_ERR(plane)) {
			SDE_ERROR("sde_plane_init failed\n");
			ret = PTR_ERR(plane);
			goto fail;
		}
		priv->planes[priv->num_planes++] = plane;

		if (primary)
			primary_planes[primary_planes_idx++] = plane;

		if (sde_hw_sspp_multirect_enabled(&catalog->sspp[i]) &&
			sde_is_custom_client()) {
			int priority =
				catalog->sspp[i].sblk->smart_dma_priority;
			sspp_id[priority - 1] = catalog->sspp[i].id;
			master_plane_id[priority - 1] = plane->base.id;
			num_virt_planes++;
		}
	}

	/* Initialize smart DMA virtual planes */
	for (i = 0; i < num_virt_planes; i++) {
		plane = sde_plane_init(dev, sspp_id[i], false,
			(1UL << max_crtc_count) - 1, master_plane_id[i]);
		if (IS_ERR(plane)) {
			SDE_ERROR("sde_plane for virtual SSPP init failed\n");
			ret = PTR_ERR(plane);
			goto fail;
		}
		priv->planes[priv->num_planes++] = plane;
	}

	max_crtc_count = min(max_crtc_count, primary_planes_idx);

	/* Create one CRTC per encoder */
	for (i = 0; i < max_crtc_count; i++) {
		crtc = sde_crtc_init(dev, primary_planes[i]);
		if (IS_ERR(crtc)) {
			ret = PTR_ERR(crtc);
			goto fail;
		}
		priv->crtcs[priv->num_crtcs++] = crtc;
	}

	if (sde_is_custom_client()) {
		/* All CRTCs are compatible with all planes */
		for (i = 0; i < priv->num_planes; i++)
			priv->planes[i]->possible_crtcs =
				(1 << priv->num_crtcs) - 1;
	}

	/* All CRTCs are compatible with all encoders */
	for (i = 0; i < priv->num_encoders; i++)
		priv->encoders[i]->possible_crtcs = (1 << priv->num_crtcs) - 1;

	return 0;
fail:
	_sde_kms_drm_obj_destroy(sde_kms);
fail_irq:
	sde_core_irq_domain_fini(sde_kms);
	return ret;
}

/**
 * sde_kms_timeline_status - provides current timeline status
 *    This API should be called without mode config lock.
 * @dev: Pointer to drm device
 */
void sde_kms_timeline_status(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;

	if (!dev) {
		SDE_ERROR("invalid drm device node\n");
		return;
	}

	drm_for_each_crtc(crtc, dev)
		sde_crtc_timeline_status(crtc);

	if (mutex_is_locked(&dev->mode_config.mutex)) {
		/*
		 *Probably locked from last close dumping status anyway
		 */
		SDE_ERROR("dumping conn_timeline without mode_config lock\n");
		drm_connector_list_iter_begin(dev, &conn_iter);
		drm_for_each_connector_iter(conn, &conn_iter)
			sde_conn_timeline_status(conn);
		drm_connector_list_iter_end(&conn_iter);
		return;
	}

	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter)
		sde_conn_timeline_status(conn);
	drm_connector_list_iter_end(&conn_iter);
	mutex_unlock(&dev->mode_config.mutex);
}

static int sde_kms_postinit(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev;
	struct drm_crtc *crtc;
	int rc;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	dev = sde_kms->dev;

	rc = _sde_debugfs_init(sde_kms);
	if (rc)
		SDE_ERROR("sde_debugfs init failed: %d\n", rc);

	drm_for_each_crtc(crtc, dev)
		sde_crtc_post_init(dev, crtc);

	return rc;
}

static long sde_kms_round_pixclk(struct msm_kms *kms, unsigned long rate,
		struct drm_encoder *encoder)
{
	return rate;
}

static void _sde_kms_hw_destroy(struct sde_kms *sde_kms,
		struct platform_device *pdev)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	int i;

	if (!sde_kms || !pdev)
		return;

	dev = sde_kms->dev;
	if (!dev)
		return;

	priv = dev->dev_private;
	if (!priv)
		return;

	if (sde_kms->genpd_init) {
		sde_kms->genpd_init = false;
		pm_genpd_remove(&sde_kms->genpd);
		of_genpd_del_provider(pdev->dev.of_node);
	}

	if (sde_kms->hw_intr)
		sde_hw_intr_destroy(sde_kms->hw_intr);
	sde_kms->hw_intr = NULL;

	if (sde_kms->power_event)
		sde_power_handle_unregister_event(
				&priv->phandle, sde_kms->power_event);

	_sde_kms_release_displays(sde_kms);

	_sde_kms_unmap_all_splash_regions(sde_kms);

	/* safe to call these more than once during shutdown */
	_sde_debugfs_destroy(sde_kms);
	_sde_kms_mmu_destroy(sde_kms);

	if (sde_kms->catalog) {
		for (i = 0; i < sde_kms->catalog->vbif_count; i++) {
			u32 vbif_idx = sde_kms->catalog->vbif[i].id;

			if ((vbif_idx < VBIF_MAX) && sde_kms->hw_vbif[vbif_idx])
				sde_hw_vbif_destroy(sde_kms->hw_vbif[vbif_idx]);
		}
	}

	if (sde_kms->rm_init)
		sde_rm_destroy(&sde_kms->rm);
	sde_kms->rm_init = false;

	if (sde_kms->catalog)
		sde_hw_catalog_deinit(sde_kms->catalog);
	sde_kms->catalog = NULL;

	if (sde_kms->sid)
		msm_iounmap(pdev, sde_kms->sid);
	sde_kms->sid = NULL;

	if (sde_kms->reg_dma)
		msm_iounmap(pdev, sde_kms->reg_dma);
	sde_kms->reg_dma = NULL;

	if (sde_kms->vbif[VBIF_NRT])
		msm_iounmap(pdev, sde_kms->vbif[VBIF_NRT]);
	sde_kms->vbif[VBIF_NRT] = NULL;

	if (sde_kms->vbif[VBIF_RT])
		msm_iounmap(pdev, sde_kms->vbif[VBIF_RT]);
	sde_kms->vbif[VBIF_RT] = NULL;

	if (sde_kms->mmio)
		msm_iounmap(pdev, sde_kms->mmio);
	sde_kms->mmio = NULL;

	sde_reg_dma_deinit();
}

int sde_kms_mmu_detach(struct sde_kms *sde_kms, bool secure_only)
{
	int i;

	if (!sde_kms)
		return -EINVAL;

	for (i = 0; i < MSM_SMMU_DOMAIN_MAX; i++) {
		struct msm_mmu *mmu;
		struct msm_gem_address_space *aspace = sde_kms->aspace[i];

		if (!aspace)
			continue;

		mmu = sde_kms->aspace[i]->mmu;

		if (secure_only &&
			!aspace->mmu->funcs->is_domain_secure(mmu))
			continue;

		/* cleanup aspace before detaching */
		msm_gem_aspace_domain_attach_detach_update(aspace, true);

		SDE_DEBUG("Detaching domain:%d\n", i);
		aspace->mmu->funcs->detach(mmu, (const char **)iommu_ports,
			ARRAY_SIZE(iommu_ports));

		aspace->domain_attached = false;
	}

	return 0;
}

int sde_kms_mmu_attach(struct sde_kms *sde_kms, bool secure_only)
{
	int i;

	if (!sde_kms)
		return -EINVAL;

	for (i = 0; i < MSM_SMMU_DOMAIN_MAX; i++) {
		struct msm_mmu *mmu;
		struct msm_gem_address_space *aspace = sde_kms->aspace[i];

		if (!aspace)
			continue;

		mmu = sde_kms->aspace[i]->mmu;

		if (secure_only &&
			!aspace->mmu->funcs->is_domain_secure(mmu))
			continue;

		SDE_DEBUG("Attaching domain:%d\n", i);
		aspace->mmu->funcs->attach(mmu, (const char **)iommu_ports,
			ARRAY_SIZE(iommu_ports));

		aspace->domain_attached = true;
		msm_gem_aspace_domain_attach_detach_update(aspace, false);
	}

	return 0;
}

static void sde_kms_destroy(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev || !dev->dev) {
		SDE_ERROR("invalid device\n");
		return;
	}

	_sde_kms_hw_destroy(sde_kms, to_platform_device(dev->dev));
	kfree(sde_kms);
}

static void _sde_kms_plane_force_remove(struct drm_plane *plane,
			struct drm_atomic_state *state)
{
	struct drm_plane_state *plane_state;
	int ret = 0;

	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		SDE_ERROR("error %d getting plane %d state\n",
				ret, plane->base.id);
		return;
	}

	plane->old_fb = plane->fb;

	SDE_DEBUG("disabling plane %d\n", plane->base.id);

	ret = __drm_atomic_helper_disable_plane(plane, plane_state);
	if (ret != 0)
		SDE_ERROR("error %d disabling plane %d\n", ret,
				plane->base.id);
}

static int _sde_kms_remove_fbs(struct sde_kms *sde_kms, struct drm_file *file,
		struct drm_atomic_state *state)
{
	struct drm_device *dev = sde_kms->dev;
	struct drm_framebuffer *fb, *tfb;
	struct list_head fbs;
	struct drm_plane *plane;
	int ret = 0;
	u32 plane_mask = 0;

	INIT_LIST_HEAD(&fbs);

	list_for_each_entry_safe(fb, tfb, &file->fbs, filp_head) {
		if (drm_framebuffer_read_refcount(fb) > 1) {
			list_move_tail(&fb->filp_head, &fbs);

			drm_for_each_plane(plane, dev) {
				if (plane->fb == fb) {
					plane_mask |=
						1 << drm_plane_index(plane);
					 _sde_kms_plane_force_remove(
								plane, state);
				}
			}
		} else {
			list_del_init(&fb->filp_head);
			drm_framebuffer_put(fb);
		}
	}

	if (list_empty(&fbs)) {
		SDE_DEBUG("skip commit as no fb(s)\n");
		drm_atomic_state_put(state);
		return 0;
	}

	SDE_DEBUG("committing after removing all the pipes\n");
	ret = drm_atomic_commit(state);

	if (ret) {
		/*
		 * move the fbs back to original list, so it would be
		 * handled during drm_release
		 */
		list_for_each_entry_safe(fb, tfb, &fbs, filp_head)
			list_move_tail(&fb->filp_head, &file->fbs);

		SDE_ERROR("atomic commit failed in preclose, ret:%d\n", ret);
		goto end;
	}

	while (!list_empty(&fbs)) {
		fb = list_first_entry(&fbs, typeof(*fb), filp_head);

		list_del_init(&fb->filp_head);
		drm_framebuffer_put(fb);
	}

end:
	return ret;
}

static void sde_kms_preclose(struct msm_kms *kms, struct drm_file *file)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;
	unsigned int i;
	struct drm_atomic_state *state = NULL;
	struct drm_modeset_acquire_ctx ctx;
	int ret = 0;

	/* cancel pending flip event */
	for (i = 0; i < priv->num_crtcs; i++)
		sde_crtc_complete_flip(priv->crtcs[i], file);

	drm_modeset_acquire_init(&ctx, 0);
retry:
	ret = drm_modeset_lock_all_ctx(dev, &ctx);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	} else if (WARN_ON(ret)) {
		goto end;
	}

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto end;
	}

	state->acquire_ctx = &ctx;

	for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
		ret = _sde_kms_remove_fbs(sde_kms, file, state);
		if (ret != -EDEADLK)
			break;
		drm_atomic_state_clear(state);
		drm_modeset_backoff(&ctx);
	}

end:
	if (state)
		drm_atomic_state_put(state);

	SDE_DEBUG("sde preclose done, ret:%d\n", ret);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static int _sde_kms_helper_reset_custom_properties(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
	struct drm_device *dev = sde_kms->dev;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_connector_list_iter conn_iter;
	int ret = 0;

	drm_for_each_plane(plane, dev) {
		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			SDE_ERROR("error %d getting plane %d state\n",
					ret, DRMID(plane));
			return ret;
		}

		ret = sde_plane_helper_reset_custom_properties(plane,
				plane_state);
		if (ret) {
			SDE_ERROR("error %d resetting plane props %d\n",
					ret, DRMID(plane));
			return ret;
		}
	}
	drm_for_each_crtc(crtc, dev) {
		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			ret = PTR_ERR(crtc_state);
			SDE_ERROR("error %d getting crtc %d state\n",
					ret, DRMID(crtc));
			return ret;
		}

		ret = sde_crtc_helper_reset_custom_properties(crtc, crtc_state);
		if (ret) {
			SDE_ERROR("error %d resetting crtc props %d\n",
					ret, DRMID(crtc));
			return ret;
		}
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		conn_state = drm_atomic_get_connector_state(state, conn);
		if (IS_ERR(conn_state)) {
			ret = PTR_ERR(conn_state);
			SDE_ERROR("error %d getting connector %d state\n",
					ret, DRMID(conn));
			return ret;
		}

		ret = sde_connector_helper_reset_custom_properties(conn,
				conn_state);
		if (ret) {
			SDE_ERROR("error %d resetting connector props %d\n",
					ret, DRMID(conn));
			return ret;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

static void sde_kms_lastclose(struct msm_kms *kms,
		struct drm_modeset_acquire_ctx *ctx)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct drm_atomic_state *state;
	int ret, i;

	if (!kms) {
		SDE_ERROR("invalid argument\n");
		return;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return;

	state->acquire_ctx = ctx;

	for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
		/* add reset of custom properties to the state */
		ret = _sde_kms_helper_reset_custom_properties(sde_kms, state);
		if (ret)
			break;

		ret = drm_atomic_commit(state);
		if (ret != -EDEADLK)
			break;

		drm_atomic_state_clear(state);
		drm_modeset_backoff(ctx);
		SDE_DEBUG("deadlock backoff on attempt %d\n", i);
	}

	if (ret)
		SDE_ERROR("failed to run last close: %d\n", ret);

	drm_atomic_state_put(state);
}

static int sde_kms_check_secure_transition(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct drm_crtc *crtc;
	struct drm_crtc *cur_crtc = NULL, *global_crtc = NULL;
	struct drm_crtc_state *crtc_state;
	int active_crtc_cnt = 0, global_active_crtc_cnt = 0;
	bool sec_session = false, global_sec_session = false;
	uint32_t fb_ns = 0, fb_sec = 0, fb_sec_dir = 0;
	int i;

	if (!kms || !state) {
		return -EINVAL;
		SDE_ERROR("invalid arguments\n");
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	/* iterate state object for active secure/non-secure crtc */
	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (!crtc_state->active)
			continue;

		active_crtc_cnt++;
		sde_crtc_state_find_plane_fb_modes(crtc_state, &fb_ns,
				&fb_sec, &fb_sec_dir);
		if (fb_sec_dir)
			sec_session = true;
		cur_crtc = crtc;
	}

	/* iterate global list for active and secure/non-secure crtc */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (!crtc->state->active)
			continue;

		global_active_crtc_cnt++;
		/* update only when crtc is not the same as current crtc */
		if (crtc != cur_crtc) {
			fb_ns = fb_sec = fb_sec_dir = 0;
			sde_crtc_find_plane_fb_modes(crtc, &fb_ns,
					&fb_sec, &fb_sec_dir);
			if (fb_sec_dir)
				global_sec_session = true;
			global_crtc = crtc;
		}
	}

	if (!global_sec_session && !sec_session)
		return 0;

	/*
	 * - fail crtc commit, if secure-camera/secure-ui session is
	 *   in-progress in any other display
	 * - fail secure-camera/secure-ui crtc commit, if any other display
	 *   session is in-progress
	 */
	if ((global_active_crtc_cnt > MAX_ALLOWED_CRTC_CNT_DURING_SECURE) ||
		    (active_crtc_cnt > MAX_ALLOWED_CRTC_CNT_DURING_SECURE)) {
		SDE_ERROR(
		    "crtc%d secure check failed global_active:%d active:%d\n",
				cur_crtc ? cur_crtc->base.id : -1,
				global_active_crtc_cnt, active_crtc_cnt);
		return -EPERM;

	/*
	 * As only one crtc is allowed during secure session, the crtc
	 * in this commit should match with the global crtc
	 */
	} else if (global_crtc && cur_crtc && (global_crtc != cur_crtc)) {
		SDE_ERROR("crtc%d-sec%d not allowed during crtc%d-sec%d\n",
				cur_crtc->base.id, sec_session,
				global_crtc->base.id, global_sec_session);
		return -EPERM;
	}

	return 0;
}

static int sde_kms_atomic_check(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	int ret;

	if (!kms || !state)
		return -EINVAL;

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	SDE_ATRACE_BEGIN("atomic_check");
	if (sde_kms_is_suspend_blocked(dev)) {
		SDE_DEBUG("suspended, skip atomic_check\n");
		ret = -EBUSY;
		goto end;
	}

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		goto end;
	/*
	 * Check if any secure transition(moving CRTC between secure and
	 * non-secure state and vice-versa) is allowed or not. when moving
	 * to secure state, planes with fb_mode set to dir_translated only can
	 * be staged on the CRTC, and only one CRTC can be active during
	 * Secure state
	 */
	ret = sde_kms_check_secure_transition(kms, state);
end:
	SDE_ATRACE_END("atomic_check");
	return ret;
}

static struct msm_gem_address_space*
_sde_kms_get_address_space(struct msm_kms *kms,
		unsigned int domain)
{
	struct sde_kms *sde_kms;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return  NULL;
	}

	sde_kms = to_sde_kms(kms);
	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return NULL;
	}

	if (domain >= MSM_SMMU_DOMAIN_MAX)
		return NULL;

	return (sde_kms->aspace[domain] &&
			sde_kms->aspace[domain]->domain_attached) ?
		sde_kms->aspace[domain] : NULL;
}

static struct device *_sde_kms_get_address_space_device(struct msm_kms *kms,
		unsigned int domain)
{
	struct msm_gem_address_space *aspace =
		_sde_kms_get_address_space(kms, domain);

	return (aspace && aspace->domain_attached) ?
			msm_gem_get_aspace_device(aspace) : NULL;
}

static void _sde_kms_post_open(struct msm_kms *kms, struct drm_file *file)
{
	struct drm_device *dev = NULL;
	struct sde_kms *sde_kms = NULL;
	struct drm_connector *connector = NULL;
	struct drm_connector_list_iter conn_iter;
	struct sde_connector *sde_conn = NULL;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	if (!dev) {
		SDE_ERROR("invalid device\n");
		return;
	}

	if (!dev->mode_config.poll_enabled)
		return;

	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		/* Only handle HPD capable connectors. */
		if (!(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		sde_conn = to_sde_connector(connector);

		if (sde_conn->ops.post_open)
			sde_conn->ops.post_open(&sde_conn->base,
					sde_conn->display);
	}
	drm_connector_list_iter_end(&conn_iter);
	mutex_unlock(&dev->mode_config.mutex);

}

static int _sde_kms_update_planes_for_cont_splash(struct sde_kms *sde_kms,
		struct sde_splash_display *splash_display,
		struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;
	struct drm_plane *plane;
	struct sde_splash_mem *splash;
	enum sde_sspp plane_id;
	bool is_virtual;
	int i, j;

	if (!sde_kms || !splash_display || !crtc) {
		SDE_ERROR("invalid input args\n");
		return -EINVAL;
	}

	priv = sde_kms->dev->dev_private;
	for (i = 0; i < priv->num_planes; i++) {
		plane = priv->planes[i];
		plane_id = sde_plane_pipe(plane);
		is_virtual = is_sde_plane_virtual(plane);
		splash = splash_display->splash;

		for (j = 0; j < splash_display->pipe_cnt; j++) {
			if ((plane_id != splash_display->pipes[j].sspp) ||
					(splash_display->pipes[j].is_virtual
					 != is_virtual))
				continue;

			if (splash && sde_plane_validate_src_addr(plane,
						splash->splash_buf_base,
						splash->splash_buf_size)) {
				SDE_ERROR("invalid adr on pipe:%d crtc:%d\n",
						plane_id, crtc->base.id);
			}

			SDE_DEBUG("set crtc:%d for plane:%d rect:%d\n",
					crtc->base.id, plane_id, is_virtual);
		}
	}

	return 0;
}

static int sde_kms_cont_splash_config(struct msm_kms *kms)
{
	void *display;
	struct dsi_display *dsi_display;
	struct msm_display_info info;
	struct drm_encoder *encoder = NULL;
	struct drm_crtc *crtc = NULL;
	int i, rc = 0;
	struct drm_display_mode *drm_mode = NULL;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector = NULL;
	struct sde_connector *sde_conn = NULL;
	struct sde_splash_display *splash_display;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev) {
		SDE_ERROR("invalid device\n");
		return -EINVAL;
	}

	if (!sde_kms->splash_data.num_splash_regions ||
			!sde_kms->splash_data.num_splash_displays) {
		DRM_INFO("cont_splash feature not enabled\n");
		return rc;
	}

	DRM_INFO("cont_splash enabled in %d of %d display(s)\n",
				sde_kms->splash_data.num_splash_displays,
				sde_kms->dsi_display_count);

	/* dsi */
	for (i = 0; i < sde_kms->dsi_display_count; ++i) {
		display = sde_kms->dsi_displays[i];
		dsi_display = (struct dsi_display *)display;
		splash_display = &sde_kms->splash_data.splash_display[i];

		if (!splash_display->cont_splash_enabled) {
			SDE_DEBUG("display->name = %s splash not enabled\n",
					dsi_display->name);
			continue;
		}

		SDE_DEBUG("display->name = %s\n", dsi_display->name);

		if (dsi_display->bridge->base.encoder) {
			encoder = dsi_display->bridge->base.encoder;
			SDE_DEBUG("encoder name = %s\n", encoder->name);
		}
		memset(&info, 0x0, sizeof(info));
		rc = dsi_display_get_info(NULL, &info, display);
		if (rc) {
			SDE_ERROR("dsi get_info %d failed\n", i);
			encoder = NULL;
			continue;
		}
		SDE_DEBUG("info.is_connected = %s, info.display_type = %d\n",
			((info.is_connected) ? "true" : "false"),
			info.display_type);

		if (!encoder) {
			SDE_ERROR("encoder not initialized\n");
			return -EINVAL;
		}

		priv = sde_kms->dev->dev_private;
		encoder->crtc = priv->crtcs[i];
		crtc = encoder->crtc;
		splash_display->encoder =  encoder;

		SDE_DEBUG("for dsi-display:%d crtc id = %d enc id =%d\n",
				i, crtc->base.id, encoder->base.id);

		mutex_lock(&dev->mode_config.mutex);
		drm_connector_list_iter_begin(dev, &conn_iter);
		drm_for_each_connector_iter(connector, &conn_iter) {
			/**
			 * SDE_KMS doesn't attach more than one encoder to
			 * a DSI connector. So it is safe to check only with
			 * the first encoder entry. Revisit this logic if we
			 * ever have to support continuous splash for
			 * external displays in MST configuration.
			 */
			if (connector->encoder_ids[0] == encoder->base.id)
				break;
		}
		drm_connector_list_iter_end(&conn_iter);

		if (!connector) {
			SDE_ERROR("connector not initialized\n");
			mutex_unlock(&dev->mode_config.mutex);
			return -EINVAL;
		}

		if (connector->funcs->fill_modes) {
			connector->funcs->fill_modes(connector,
					dev->mode_config.max_width,
					dev->mode_config.max_height);
		} else {
			SDE_ERROR("fill_modes api not defined\n");
			mutex_unlock(&dev->mode_config.mutex);
			return -EINVAL;
		}
		mutex_unlock(&dev->mode_config.mutex);

		crtc->state->encoder_mask = (1 << drm_encoder_index(encoder));

		/* currently consider modes[0] as the preferred mode */
		drm_mode = list_first_entry(&connector->modes,
				struct drm_display_mode, head);
		SDE_DEBUG("drm_mode->name = %s, id=%d, type=0x%x, flags=0x%x\n",
				drm_mode->name, drm_mode->base.id,
				drm_mode->type, drm_mode->flags);

		/* Update CRTC drm structure */
		crtc->state->active = true;
		rc = drm_atomic_set_mode_for_crtc(crtc->state, drm_mode);
		if (rc) {
			SDE_ERROR("Failed: set mode for crtc. rc = %d\n", rc);
			return rc;
		}
		drm_mode_copy(&crtc->state->adjusted_mode, drm_mode);
		drm_mode_copy(&crtc->mode, drm_mode);

		/* Update encoder structure */
		sde_encoder_update_caps_for_cont_splash(encoder,
				splash_display, true);

		sde_crtc_update_cont_splash_settings(crtc);

		sde_conn = to_sde_connector(connector);
		if (sde_conn && sde_conn->ops.cont_splash_config)
			sde_conn->ops.cont_splash_config(sde_conn->display);

		rc = _sde_kms_update_planes_for_cont_splash(sde_kms,
				splash_display, crtc);
		if (rc) {
			SDE_ERROR("Failed: updating plane status rc=%d\n", rc);
			return rc;
		}
	}

	return rc;
}

static bool sde_kms_check_for_splash(struct msm_kms *kms, struct drm_crtc *crtc)
{
	struct sde_kms *sde_kms;
	struct drm_encoder *encoder;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return false;
	}

	sde_kms = to_sde_kms(kms);

	if (!crtc || !sde_kms->splash_data.num_splash_displays)
		return !!sde_kms->splash_data.num_splash_displays;

	drm_for_each_encoder_mask(encoder, crtc->dev,
			crtc->state->encoder_mask) {
		if (sde_encoder_in_cont_splash(encoder))
			return true;
	}

	return false;

}

static int sde_kms_get_mixer_count(const struct msm_kms *kms,
		const struct drm_display_mode *mode,
		const struct msm_resource_caps_info *res, u32 *num_lm)
{
	struct sde_kms *sde_kms;
	s64 mode_clock_hz = 0;
	s64 max_mdp_clock_hz = 0;
	s64 mdp_fudge_factor = 0;
	s64 temp = 0;
	s64 htotal_fp = 0;
	s64 vtotal_fp = 0;
	s64 vrefresh_fp = 0;

	if (!num_lm) {
		SDE_ERROR("invalid num_lm pointer\n");
		return -EINVAL;
	}

	*num_lm = 1;
	if (!kms || !mode || !res) {
		SDE_ERROR("invalid input args\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);

	max_mdp_clock_hz = drm_fixp_from_fraction(
			sde_kms->perf.max_core_clk_rate, 1);
	mdp_fudge_factor = drm_fixp_from_fraction(105, 100); /* 1.05 */
	htotal_fp = drm_fixp_from_fraction(mode->htotal, 1);
	vtotal_fp = drm_fixp_from_fraction(mode->vtotal, 1);
	vrefresh_fp = drm_fixp_from_fraction(mode->vrefresh, 1);

	temp = drm_fixp_mul(htotal_fp, vtotal_fp);
	temp = drm_fixp_mul(temp, vrefresh_fp);
	mode_clock_hz = drm_fixp_mul(temp, mdp_fudge_factor);
	if (mode_clock_hz > max_mdp_clock_hz ||
			mode->hdisplay > res->max_mixer_width)
		*num_lm = 2;
	SDE_DEBUG("[%s] h=%d, v=%d, fps=%d, max_mdp_clk_hz=%llu, num_lm=%d\n",
			mode->name, mode->htotal, mode->vtotal, mode->vrefresh,
			sde_kms->perf.max_core_clk_rate, *num_lm);

	return 0;
}

static void _sde_kms_null_commit(struct drm_device *dev,
		struct drm_encoder *enc)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector *conn = NULL;
	struct drm_connector *tmp_conn = NULL;
	struct drm_connector_list_iter conn_iter;
	struct drm_atomic_state *state = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	struct drm_connector_state *conn_state = NULL;
	int retry_cnt = 0;
	int ret = 0;

	drm_modeset_acquire_init(&ctx, 0);

retry:
	ret = drm_modeset_lock_all_ctx(dev, &ctx);
	if (ret == -EDEADLK && retry_cnt < SDE_KMS_MODESET_LOCK_MAX_TRIALS) {
		drm_modeset_backoff(&ctx);
		retry_cnt++;
		udelay(SDE_KMS_MODESET_LOCK_TIMEOUT_US);
		goto retry;
	} else if (WARN_ON(ret)) {
		goto end;
	}

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		DRM_ERROR("failed to allocate atomic state, %d\n", ret);
		goto end;
	}

	state->acquire_ctx = &ctx;
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(tmp_conn, &conn_iter) {
		if (enc == tmp_conn->state->best_encoder) {
			conn = tmp_conn;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!conn) {
		SDE_ERROR("error in finding conn for enc:%d\n", DRMID(enc));
		goto end;
	}

	crtc_state = drm_atomic_get_crtc_state(state, enc->crtc);
	conn_state = drm_atomic_get_connector_state(state, conn);
	if (IS_ERR(conn_state)) {
		SDE_ERROR("error %d getting connector %d state\n",
				ret, DRMID(conn));
		goto end;
	}

	crtc_state->active = true;
	ret = drm_atomic_set_crtc_for_connector(conn_state, enc->crtc);
	if (ret)
		SDE_ERROR("error %d setting the crtc\n", ret);

	ret = drm_atomic_commit(state);
	if (ret)
		SDE_ERROR("Error %d doing the atomic commit\n", ret);

end:
	if (state)
		drm_atomic_state_put(state);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static void _sde_kms_pm_suspend_idle_helper(struct sde_kms *sde_kms,
	struct device *dev)
{
	int i, ret, crtc_id = 0;
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct msm_drm_private *priv = sde_kms->dev->dev_private;

	drm_connector_list_iter_begin(ddev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		uint64_t lp;

		lp = sde_connector_get_lp(conn);
		if (lp != SDE_MODE_DPMS_LP2)
			continue;

		if (sde_encoder_in_clone_mode(conn->encoder))
			continue;

		ret = sde_encoder_wait_for_event(conn->encoder,
						MSM_ENC_TX_COMPLETE);
		if (ret && ret != -EWOULDBLOCK) {
			SDE_ERROR(
				"[conn: %d] wait for commit done returned %d\n",
				conn->base.id, ret);
		} else if (!ret) {
			crtc_id = drm_crtc_index(conn->state->crtc);
			if (priv->event_thread[crtc_id].thread)
				kthread_flush_worker(
					&priv->event_thread[crtc_id].worker);
			sde_encoder_idle_request(conn->encoder);
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	for (i = 0; i < priv->num_crtcs; i++) {
		if (priv->disp_thread[i].thread)
			kthread_flush_worker(
				&priv->disp_thread[i].worker);
		if (priv->event_thread[i].thread)
			kthread_flush_worker(
				&priv->event_thread[i].worker);
	}
	kthread_flush_worker(&priv->pp_event_worker);
}

static int sde_kms_pm_suspend(struct device *dev)
{
	struct drm_device *ddev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector *conn;
	struct drm_encoder *enc;
	struct drm_connector_list_iter conn_iter;
	struct drm_atomic_state *state = NULL;
	struct sde_kms *sde_kms;
	int ret = 0, num_crtcs = 0;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev_to_msm_kms(ddev))
		return -EINVAL;

	sde_kms = to_sde_kms(ddev_to_msm_kms(ddev));
	SDE_EVT32(0);

	/* disable hot-plug polling */
	drm_kms_helper_poll_disable(ddev);

	/* if a display stuck in CS trigger a null commit to complete handoff */
	drm_for_each_encoder(enc, ddev) {
		if (sde_encoder_in_cont_splash(enc) && enc->crtc)
			_sde_kms_null_commit(ddev, enc);
	}

	/* acquire modeset lock(s) */
	drm_modeset_acquire_init(&ctx, 0);

retry:
	ret = drm_modeset_lock_all_ctx(ddev, &ctx);
	if (ret)
		goto unlock;

	/* save current state for resume */
	if (sde_kms->suspend_state)
		drm_atomic_state_put(sde_kms->suspend_state);
	sde_kms->suspend_state = drm_atomic_helper_duplicate_state(ddev, &ctx);
	if (IS_ERR_OR_NULL(sde_kms->suspend_state)) {
		ret = PTR_ERR(sde_kms->suspend_state);
		DRM_ERROR("failed to back up suspend state, %d\n", ret);
		sde_kms->suspend_state = NULL;
		goto unlock;
	}

	/* create atomic state to disable all CRTCs */
	state = drm_atomic_state_alloc(ddev);
	if (!state) {
		ret = -ENOMEM;
		DRM_ERROR("failed to allocate crtc disable state, %d\n", ret);
		goto unlock;
	}

	state->acquire_ctx = &ctx;
	drm_connector_list_iter_begin(ddev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		struct drm_crtc_state *crtc_state;
		uint64_t lp;

		if (!conn->state || !conn->state->crtc ||
			conn->dpms != DRM_MODE_DPMS_ON ||
			sde_encoder_in_clone_mode(conn->encoder))
			continue;

		lp = sde_connector_get_lp(conn);
		if (lp == SDE_MODE_DPMS_LP1) {
			/* transition LP1->LP2 on pm suspend */
			ret = sde_connector_set_property_for_commit(conn, state,
					CONNECTOR_PROP_LP, SDE_MODE_DPMS_LP2);
			if (ret) {
				DRM_ERROR("failed to set lp2 for conn %d\n",
						conn->base.id);
				drm_connector_list_iter_end(&conn_iter);
				goto unlock;
			}
		}

		if (lp != SDE_MODE_DPMS_LP2) {
			/* force CRTC to be inactive */
			crtc_state = drm_atomic_get_crtc_state(state,
					conn->state->crtc);
			if (IS_ERR_OR_NULL(crtc_state)) {
				DRM_ERROR("failed to get crtc %d state\n",
						conn->state->crtc->base.id);
				drm_connector_list_iter_end(&conn_iter);
				goto unlock;
			}

			if (lp != SDE_MODE_DPMS_LP1)
				crtc_state->active = false;
			++num_crtcs;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* check for nothing to do */
	if (num_crtcs == 0) {
		DRM_DEBUG("all crtcs are already in the off state\n");
		sde_kms->suspend_block = true;
		_sde_kms_pm_suspend_idle_helper(sde_kms, dev);
		goto unlock;
	}

	/* commit the "disable all" state */
	ret = drm_atomic_commit(state);
	if (ret < 0) {
		DRM_ERROR("failed to disable crtcs, %d\n", ret);
		goto unlock;
	}

	sde_kms->suspend_block = true;
	_sde_kms_pm_suspend_idle_helper(sde_kms, dev);

unlock:
	if (state) {
		drm_atomic_state_put(state);
		state = NULL;
	}

	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	}
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	/*
	 * pm runtime driver avoids multiple runtime_suspend API call by
	 * checking runtime_status. However, this call helps when there is a
	 * race condition between pm_suspend call and doze_suspend/power_off
	 * commit. It removes the extra vote from suspend and adds it back
	 * later to allow power collapse during pm_suspend call
	 */
	pm_runtime_put_sync(dev);
	pm_runtime_get_noresume(dev);

	return ret;
}

static int sde_kms_pm_resume(struct device *dev)
{
	struct drm_device *ddev;
	struct sde_kms *sde_kms;
	struct drm_modeset_acquire_ctx ctx;
	int ret, i;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev_to_msm_kms(ddev))
		return -EINVAL;

	sde_kms = to_sde_kms(ddev_to_msm_kms(ddev));

	SDE_EVT32(sde_kms->suspend_state != NULL);

	drm_mode_config_reset(ddev);

	drm_modeset_acquire_init(&ctx, 0);
retry:
	ret = drm_modeset_lock_all_ctx(ddev, &ctx);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	} else if (WARN_ON(ret)) {
		goto end;
	}

	sde_kms->suspend_block = false;

	if (sde_kms->suspend_state) {
		sde_kms->suspend_state->acquire_ctx = &ctx;
		for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
			ret = drm_atomic_helper_commit_duplicated_state(
					sde_kms->suspend_state, &ctx);
			if (ret != -EDEADLK)
				break;

			drm_modeset_backoff(&ctx);
		}

		if (ret < 0)
			DRM_ERROR("failed to restore state, %d\n", ret);

		drm_atomic_state_put(sde_kms->suspend_state);
		sde_kms->suspend_state = NULL;
	}

end:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	/* enable hot-plug polling */
	drm_kms_helper_poll_enable(ddev);

	return 0;
}

static const struct msm_kms_funcs kms_funcs = {
	.hw_init         = sde_kms_hw_init,
	.postinit        = sde_kms_postinit,
	.irq_preinstall  = sde_irq_preinstall,
	.irq_postinstall = sde_irq_postinstall,
	.irq_uninstall   = sde_irq_uninstall,
	.irq             = sde_irq,
	.preclose        = sde_kms_preclose,
	.lastclose       = sde_kms_lastclose,
	.prepare_fence   = sde_kms_prepare_fence,
	.prepare_commit  = sde_kms_prepare_commit,
	.commit          = sde_kms_commit,
	.complete_commit = sde_kms_complete_commit,
	.wait_for_crtc_commit_done = sde_kms_wait_for_commit_done,
	.wait_for_tx_complete = sde_kms_wait_for_frame_transfer_complete,
	.enable_vblank   = sde_kms_enable_vblank,
	.disable_vblank  = sde_kms_disable_vblank,
	.check_modified_format = sde_format_check_modified_format,
	.atomic_check = sde_kms_atomic_check,
	.get_format      = sde_get_msm_format,
	.round_pixclk    = sde_kms_round_pixclk,
	.pm_suspend      = sde_kms_pm_suspend,
	.pm_resume       = sde_kms_pm_resume,
	.destroy         = sde_kms_destroy,
	.cont_splash_config = sde_kms_cont_splash_config,
	.register_events = _sde_kms_register_events,
	.get_address_space = _sde_kms_get_address_space,
	.get_address_space_device = _sde_kms_get_address_space_device,
	.postopen = _sde_kms_post_open,
	.check_for_splash = sde_kms_check_for_splash,
	.get_mixer_count = sde_kms_get_mixer_count,
};

/* the caller api needs to turn on clock before calling it */
static inline void _sde_kms_core_hw_rev_init(struct sde_kms *sde_kms)
{
	sde_kms->core_rev = readl_relaxed(sde_kms->mmio + 0x0);
}

static int _sde_kms_mmu_destroy(struct sde_kms *sde_kms)
{
	int i;

	for (i = ARRAY_SIZE(sde_kms->aspace) - 1; i >= 0; i--) {
		if (!sde_kms->aspace[i])
			continue;

		msm_gem_address_space_put(sde_kms->aspace[i]);
		sde_kms->aspace[i] = NULL;
	}

	return 0;
}

static int _sde_kms_mmu_init(struct sde_kms *sde_kms)
{
	struct msm_mmu *mmu;
	int i, ret;
	int early_map = 0;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev)
		return -EINVAL;

	for (i = 0; i < MSM_SMMU_DOMAIN_MAX; i++) {
		struct msm_gem_address_space *aspace;

		mmu = msm_smmu_new(sde_kms->dev->dev, i);
		if (IS_ERR(mmu)) {
			ret = PTR_ERR(mmu);
			SDE_DEBUG("failed to init iommu id %d: rc:%d\n",
								i, ret);
			continue;
		}

		aspace = msm_gem_smmu_address_space_create(sde_kms->dev,
			mmu, "sde");
		if (IS_ERR(aspace)) {
			ret = PTR_ERR(aspace);
			mmu->funcs->destroy(mmu);
			goto fail;
		}

		sde_kms->aspace[i] = aspace;
		aspace->domain_attached = true;

		/* Mapping splash memory block */
		if ((i == MSM_SMMU_DOMAIN_UNSECURE) &&
				sde_kms->splash_data.num_splash_regions) {
			ret = _sde_kms_map_all_splash_regions(sde_kms);
			if (ret) {
				SDE_ERROR("failed to map ret:%d\n", ret);
				goto fail;
			}
		}

		/*
		 * disable early-map which would have been enabled during
		 * bootup by smmu through the device-tree hint for cont-spash
		 */
		ret = mmu->funcs->set_attribute(mmu, DOMAIN_ATTR_EARLY_MAP,
				 &early_map);
		if (ret) {
			SDE_ERROR("failed to set_att ret:%d, early_map:%d\n",
					ret, early_map);
			goto early_map_fail;
		}
	}

	sde_kms->base.aspace = sde_kms->aspace[0];

	return 0;

early_map_fail:
	_sde_kms_unmap_all_splash_regions(sde_kms);
fail:
	_sde_kms_mmu_destroy(sde_kms);

	return ret;
}

static void sde_kms_init_shared_hw(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->hw_mdp || !sde_kms->catalog)
		return;

	if (sde_kms->hw_mdp->ops.reset_ubwc)
		sde_kms->hw_mdp->ops.reset_ubwc(sde_kms->hw_mdp,
						sde_kms->catalog);

	sde_hw_sid_rotator_set(sde_kms->hw_sid);
}

static void _sde_kms_set_lutdma_vbif_remap(struct sde_kms *sde_kms)
{
	struct sde_vbif_set_qos_params qos_params;
	struct sde_mdss_cfg *catalog;

	if (!sde_kms->catalog)
		return;

	catalog = sde_kms->catalog;

	memset(&qos_params, 0, sizeof(qos_params));
	qos_params.vbif_idx = catalog->dma_cfg.vbif_idx;
	qos_params.xin_id = catalog->dma_cfg.xin_id;
	qos_params.clk_ctrl = catalog->dma_cfg.clk_ctrl;
	qos_params.client_type = VBIF_LUTDMA_CLIENT;

	sde_vbif_set_qos_remap(sde_kms, &qos_params);
}

void sde_kms_update_pm_qos_irq_request(struct sde_kms *sde_kms,
			 bool enable, bool skip_lock)
{
	struct msm_drm_private *priv;

	priv = sde_kms->dev->dev_private;

	if (!skip_lock)
		mutex_lock(&priv->phandle.phandle_lock);

	if (enable) {
		struct pm_qos_request *req;
		u32 cpu_irq_latency;

		req = &sde_kms->pm_qos_irq_req;
		req->type = PM_QOS_REQ_AFFINE_CORES;
		req->cpus_affine = sde_kms->irq_cpu_mask;
		cpu_irq_latency = sde_kms->catalog->perf.cpu_irq_latency;

		if (pm_qos_request_active(req))
			pm_qos_update_request(req, cpu_irq_latency);
		else if (!cpumask_empty(&req->cpus_affine)) {
			/** If request is not active yet and mask is not empty
			 *  then it needs to be added initially
			 */
			pm_qos_add_request(req, PM_QOS_CPU_DMA_LATENCY,
					cpu_irq_latency);
		}
	} else if (!enable && pm_qos_request_active(&sde_kms->pm_qos_irq_req)) {
		pm_qos_update_request(&sde_kms->pm_qos_irq_req,
				PM_QOS_DEFAULT_VALUE);
	}

	sde_kms->pm_qos_irq_req_en = enable;

	if (!skip_lock)
		mutex_unlock(&priv->phandle.phandle_lock);
}

static void sde_kms_irq_affinity_notify(
		struct irq_affinity_notify *affinity_notify,
		const cpumask_t *mask)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms = container_of(affinity_notify,
					struct sde_kms, affinity_notify);

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev_private)
		return;

	priv = sde_kms->dev->dev_private;

	mutex_lock(&priv->phandle.phandle_lock);

	// save irq cpu mask
	sde_kms->irq_cpu_mask = *mask;

	// request vote with updated irq cpu mask
	if (sde_kms->pm_qos_irq_req_en)
		sde_kms_update_pm_qos_irq_request(sde_kms, true, true);

	mutex_unlock(&priv->phandle.phandle_lock);
}

static void sde_kms_irq_affinity_release(struct kref *ref) {}

static void sde_kms_handle_power_event(u32 event_type, void *usr)
{
	struct sde_kms *sde_kms = usr;
	struct msm_kms *msm_kms;

	msm_kms = &sde_kms->base;
	if (!sde_kms)
		return;

	SDE_DEBUG("event_type:%d\n", event_type);
	SDE_EVT32_VERBOSE(event_type);

	if (event_type == SDE_POWER_EVENT_POST_ENABLE) {
		sde_irq_update(msm_kms, true);
		sde_vbif_init_memtypes(sde_kms);
		sde_kms_init_shared_hw(sde_kms);
		_sde_kms_set_lutdma_vbif_remap(sde_kms);
		sde_kms->first_kickoff = true;
		sde_kms_update_pm_qos_irq_request(sde_kms, true, true);
	} else if (event_type == SDE_POWER_EVENT_PRE_DISABLE) {
		sde_kms_update_pm_qos_irq_request(sde_kms, false, true);
		sde_irq_update(msm_kms, false);
		sde_kms->first_kickoff = false;
	}
}

#define genpd_to_sde_kms(domain) container_of(domain, struct sde_kms, genpd)

static int sde_kms_pd_enable(struct generic_pm_domain *genpd)
{
	struct sde_kms *sde_kms = genpd_to_sde_kms(genpd);
	int rc = -EINVAL;

	SDE_DEBUG("\n");

	rc = pm_runtime_get_sync(sde_kms->dev->dev);
	if (rc > 0)
		rc = 0;

	SDE_EVT32(rc, genpd->device_count);

	return rc;
}

static int sde_kms_pd_disable(struct generic_pm_domain *genpd)
{
	struct sde_kms *sde_kms = genpd_to_sde_kms(genpd);

	SDE_DEBUG("\n");

	pm_runtime_put_sync(sde_kms->dev->dev);

	SDE_EVT32(genpd->device_count);

	return 0;
}

static int _sde_kms_get_splash_data(struct sde_splash_data *data)
{
	int i = 0;
	int ret = 0;
	struct device_node *parent, *node, *node1;
	struct resource r, r1;
	const char *node_name = "cont_splash_region";
	struct sde_splash_mem *mem;
	bool share_splash_mem = false;
	int num_displays, num_regions;
	struct sde_splash_display *splash_display;

	if (!data)
		return -EINVAL;

	memset(data, 0, sizeof(*data));

	parent = of_find_node_by_path("/reserved-memory");
	if (!parent) {
		SDE_ERROR("failed to find reserved-memory node\n");
		return -EINVAL;
	}

	node = of_find_node_by_name(parent, node_name);
	if (!node) {
		SDE_DEBUG("failed to find node %s\n", node_name);
		return -EINVAL;
	}

	node1 = of_find_node_by_name(parent, "disp_rdump_region");
	if (!node1)
		SDE_DEBUG("failed to find disp ramdump memory reservation\n");

	/**
	 * Support sharing a single splash memory for all the built in displays
	 * and also independent splash region per displays. Incase of
	 * independent splash region for each connected display, dtsi node of
	 * cont_splash_region  should be collection of all memory regions
	 * Ex: <r1.start r1.end r2.start r2.end  ... rn.start, rn.end>
	 */
	num_displays = dsi_display_get_num_of_displays();
	num_regions = of_property_count_u64_elems(node, "reg") / 2;

	data->num_splash_displays = num_displays;

	SDE_DEBUG("splash mem num_regions:%d\n", num_regions);
	if (num_displays > num_regions) {
		share_splash_mem = true;
		pr_info(":%d displays share same splash buf\n", num_displays);
	}

	for (i = 0; i < num_displays; i++) {
		splash_display = &data->splash_display[i];
		if (!i || !share_splash_mem) {
			if (of_address_to_resource(node, i, &r)) {
				SDE_ERROR("invalid data for:%s\n", node_name);
				return -EINVAL;
			}

			mem =  &data->splash_mem[i];
			if (!node1 || of_address_to_resource(node1, i, &r1)) {
				SDE_DEBUG("failed to find ramdump memory\n");
				mem->ramdump_base = 0;
				mem->ramdump_size = 0;
			} else {
				mem->ramdump_base = (unsigned long)r1.start;
				mem->ramdump_size = (r1.end - r1.start) + 1;
			}

			mem->splash_buf_base = (unsigned long)r.start;
			mem->splash_buf_size = (r.end - r.start) + 1;
			mem->ref_cnt = 0;
			splash_display->splash = mem;
			data->num_splash_regions++;
		} else {
			data->splash_display[i].splash = &data->splash_mem[0];
		}

		SDE_DEBUG("splash mem for disp:%d add:%lx size:%x\n", (i + 1),
				splash_display->splash->splash_buf_base,
				splash_display->splash->splash_buf_size);
	}

	return ret;
}

static int _sde_kms_hw_init_ioremap(struct sde_kms *sde_kms,
	struct platform_device *platformdev)
{
	int rc = -EINVAL;

	sde_kms->mmio = msm_ioremap(platformdev, "mdp_phys", "mdp_phys");
	if (IS_ERR(sde_kms->mmio)) {
		rc = PTR_ERR(sde_kms->mmio);
		SDE_ERROR("mdp register memory map failed: %d\n", rc);
		sde_kms->mmio = NULL;
		goto error;
	}
	DRM_INFO("mapped mdp address space @%pK\n", sde_kms->mmio);
	sde_kms->mmio_len = msm_iomap_size(platformdev, "mdp_phys");

	rc = sde_dbg_reg_register_base(SDE_DBG_NAME, sde_kms->mmio,
			sde_kms->mmio_len);
	if (rc)
		SDE_ERROR("dbg base register kms failed: %d\n", rc);

	sde_kms->vbif[VBIF_RT] = msm_ioremap(platformdev, "vbif_phys",
								"vbif_phys");
	if (IS_ERR(sde_kms->vbif[VBIF_RT])) {
		rc = PTR_ERR(sde_kms->vbif[VBIF_RT]);
		SDE_ERROR("vbif register memory map failed: %d\n", rc);
		sde_kms->vbif[VBIF_RT] = NULL;
		goto error;
	}
	sde_kms->vbif_len[VBIF_RT] = msm_iomap_size(platformdev,
								"vbif_phys");
	rc = sde_dbg_reg_register_base("vbif_rt", sde_kms->vbif[VBIF_RT],
				sde_kms->vbif_len[VBIF_RT]);
	if (rc)
		SDE_ERROR("dbg base register vbif_rt failed: %d\n", rc);

	sde_kms->vbif[VBIF_NRT] = msm_ioremap(platformdev, "vbif_nrt_phys",
								"vbif_nrt_phys");
	if (IS_ERR(sde_kms->vbif[VBIF_NRT])) {
		sde_kms->vbif[VBIF_NRT] = NULL;
		SDE_DEBUG("VBIF NRT is not defined");
	} else {
		sde_kms->vbif_len[VBIF_NRT] = msm_iomap_size(platformdev,
							"vbif_nrt_phys");
		rc = sde_dbg_reg_register_base("vbif_nrt",
				sde_kms->vbif[VBIF_NRT],
				sde_kms->vbif_len[VBIF_NRT]);
		if (rc)
			SDE_ERROR("dbg base register vbif_nrt failed: %d\n",
					rc);
	}

	sde_kms->reg_dma = msm_ioremap(platformdev, "regdma_phys",
								"regdma_phys");
	if (IS_ERR(sde_kms->reg_dma)) {
		sde_kms->reg_dma = NULL;
		SDE_DEBUG("REG_DMA is not defined");
	} else {
		sde_kms->reg_dma_len = msm_iomap_size(platformdev,
								"regdma_phys");
		rc =  sde_dbg_reg_register_base("reg_dma",
				sde_kms->reg_dma,
				sde_kms->reg_dma_len);
		if (rc)
			SDE_ERROR("dbg base register reg_dma failed: %d\n",
					rc);
	}

	sde_kms->imem = msm_ioremap(platformdev, "sde_imem_phys",
							"sde_imem_phys");

	if (IS_ERR(sde_kms->imem)) {
		sde_kms->imem = NULL;
		sde_kms->imem_len = 0;
	} else {
		sde_kms->imem_len = msm_iomap_size(platformdev,
							"sde_imem_phys");
	}

	sde_kms->sid = msm_ioremap(platformdev, "sid_phys",
							"sid_phys");
	if (IS_ERR(sde_kms->sid)) {
		rc = PTR_ERR(sde_kms->sid);
		SDE_ERROR("sid register memory map failed: %d\n", rc);
		sde_kms->sid = NULL;
		goto error;
	}

	sde_kms->sid_len = msm_iomap_size(platformdev, "sid_phys");
	rc =  sde_dbg_reg_register_base("sid", sde_kms->sid, sde_kms->sid_len);
	if (rc)
		SDE_ERROR("dbg base register sid failed: %d\n", rc);

error:
	return rc;
}

static int _sde_kms_hw_init_power_helper(struct drm_device *dev,
			struct sde_kms *sde_kms)
{
	int rc = 0;

	if (of_find_property(dev->dev->of_node, "#power-domain-cells", NULL)) {
		sde_kms->genpd.name = dev->unique;
		sde_kms->genpd.power_off = sde_kms_pd_disable;
		sde_kms->genpd.power_on = sde_kms_pd_enable;

		rc = pm_genpd_init(&sde_kms->genpd, NULL, true);
		if (rc < 0) {
			SDE_ERROR("failed to init genpd provider %s: %d\n",
					sde_kms->genpd.name, rc);
			return rc;
		}

		rc = of_genpd_add_provider_simple(dev->dev->of_node,
				&sde_kms->genpd);
		if (rc < 0) {
			SDE_ERROR("failed to add genpd provider %s: %d\n",
					sde_kms->genpd.name, rc);
			pm_genpd_remove(&sde_kms->genpd);
			return rc;
		}

		sde_kms->genpd_init = true;
		SDE_DEBUG("added genpd provider %s\n", sde_kms->genpd.name);
	}

	return rc;
}

static void _sde_kms_update_tcsr_glitch_mask(struct sde_kms *sde_kms)
{
	u32 read_val, write_val;

	if (!sde_kms || !sde_kms->catalog ||
		!sde_kms->catalog->update_tcsr_disp_glitch)
		return;

	read_val = scm_io_read(TCSR_DISP_HF_SF_ARES_GLITCH_MASK);
	write_val = read_val | BIT(2);
	scm_io_write(TCSR_DISP_HF_SF_ARES_GLITCH_MASK, write_val);

	pr_info("tcsr glitch programmed read_val:%x write_val:%x\n",
						read_val, write_val);

}

static int _sde_kms_hw_init_blocks(struct sde_kms *sde_kms,
	struct drm_device *dev,
	struct msm_drm_private *priv)
{
	struct sde_rm *rm = NULL;
	int i, rc = -EINVAL;

	for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
		sde_power_data_bus_set_quota(&priv->phandle, i,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA);

	_sde_kms_core_hw_rev_init(sde_kms);

	pr_info("sde hardware revision:0x%x\n", sde_kms->core_rev);

	sde_kms->catalog = sde_hw_catalog_init(dev, sde_kms->core_rev);
	if (IS_ERR_OR_NULL(sde_kms->catalog)) {
		rc = PTR_ERR(sde_kms->catalog);
		if (!sde_kms->catalog)
			rc = -EINVAL;
		SDE_ERROR("catalog init failed: %d\n", rc);
		sde_kms->catalog = NULL;
		goto power_error;
	}

	/* mask glitch during gdsc power up */
	_sde_kms_update_tcsr_glitch_mask(sde_kms);

	/* initialize power domain if defined */
	rc = _sde_kms_hw_init_power_helper(dev, sde_kms);
	if (rc) {
		SDE_ERROR("_sde_kms_hw_init_power_helper failed: %d\n", rc);
		goto genpd_err;
	}

	rc = _sde_kms_mmu_init(sde_kms);
	if (rc) {
		SDE_ERROR("sde_kms_mmu_init failed: %d\n", rc);
		goto power_error;
	}

	/* Initialize reg dma block which is a singleton */
	rc = sde_reg_dma_init(sde_kms->reg_dma, sde_kms->catalog,
			sde_kms->dev);
	if (rc) {
		SDE_ERROR("failed: reg dma init failed\n");
		goto power_error;
	}

	sde_dbg_init_dbg_buses(sde_kms->core_rev);

	rm = &sde_kms->rm;
	rc = sde_rm_init(rm, sde_kms->catalog, sde_kms->mmio,
			sde_kms->dev);
	if (rc) {
		SDE_ERROR("rm init failed: %d\n", rc);
		goto power_error;
	}

	sde_kms->rm_init = true;

	sde_kms->hw_intr = sde_hw_intr_init(sde_kms->mmio, sde_kms->catalog);
	if (IS_ERR_OR_NULL(sde_kms->hw_intr)) {
		rc = PTR_ERR(sde_kms->hw_intr);
		SDE_ERROR("hw_intr init failed: %d\n", rc);
		sde_kms->hw_intr = NULL;
		goto hw_intr_init_err;
	}

	/*
	 * Attempt continuous splash handoff only if reserved
	 * splash memory is found & release resources on any error
	 * in finding display hw config in splash
	 */
	if (sde_kms->splash_data.num_splash_regions) {
		struct sde_splash_display *display;
		int ret, display_count =
			sde_kms->splash_data.num_splash_displays;

		ret = sde_rm_cont_splash_res_init(priv, &sde_kms->rm,
				&sde_kms->splash_data, sde_kms->catalog);

		for (i = 0; i < display_count; i++) {
			display = &sde_kms->splash_data.splash_display[i];
			/*
			 * free splash region on resource init failure and
			 * cont-splash disabled case
			 */
			if (!display->cont_splash_enabled || ret)
				_sde_kms_free_splash_region(sde_kms, display);
		}
	}

	sde_kms->hw_mdp = sde_rm_get_mdp(&sde_kms->rm);
	if (IS_ERR_OR_NULL(sde_kms->hw_mdp)) {
		rc = PTR_ERR(sde_kms->hw_mdp);
		if (!sde_kms->hw_mdp)
			rc = -EINVAL;
		SDE_ERROR("failed to get hw_mdp: %d\n", rc);
		sde_kms->hw_mdp = NULL;
		goto power_error;
	}

	for (i = 0; i < sde_kms->catalog->vbif_count; i++) {
		u32 vbif_idx = sde_kms->catalog->vbif[i].id;

		sde_kms->hw_vbif[i] = sde_hw_vbif_init(vbif_idx,
				sde_kms->vbif[vbif_idx], sde_kms->catalog);
		if (IS_ERR_OR_NULL(sde_kms->hw_vbif[vbif_idx])) {
			rc = PTR_ERR(sde_kms->hw_vbif[vbif_idx]);
			if (!sde_kms->hw_vbif[vbif_idx])
				rc = -EINVAL;
			SDE_ERROR("failed to init vbif %d: %d\n", vbif_idx, rc);
			sde_kms->hw_vbif[vbif_idx] = NULL;
			goto power_error;
		}
	}

	if (sde_kms->catalog->uidle_cfg.uidle_rev) {
		sde_kms->hw_uidle = sde_hw_uidle_init(UIDLE, sde_kms->mmio,
			sde_kms->mmio_len, sde_kms->catalog);
		if (IS_ERR_OR_NULL(sde_kms->hw_uidle)) {
			rc = PTR_ERR(sde_kms->hw_uidle);
			if (!sde_kms->hw_uidle)
				rc = -EINVAL;
			/* uidle is optional, so do not make it a fatal error */
			SDE_ERROR("failed to init uidle rc:%d\n", rc);
			sde_kms->hw_uidle = NULL;
			rc = 0;
		}
	} else {
		sde_kms->hw_uidle = NULL;
	}

	sde_kms->hw_sid = sde_hw_sid_init(sde_kms->sid,
				sde_kms->sid_len, sde_kms->catalog);
	if (IS_ERR(sde_kms->hw_sid)) {
		SDE_ERROR("failed to init sid %ld\n", PTR_ERR(sde_kms->hw_sid));
		sde_kms->hw_sid = NULL;
		goto power_error;
	}

	rc = sde_core_perf_init(&sde_kms->perf, dev, sde_kms->catalog,
			&priv->phandle, "core_clk");
	if (rc) {
		SDE_ERROR("failed to init perf %d\n", rc);
		goto perf_err;
	}

	/*
	 * _sde_kms_drm_obj_init should create the DRM related objects
	 * i.e. CRTCs, planes, encoders, connectors and so forth
	 */
	rc = _sde_kms_drm_obj_init(sde_kms);
	if (rc) {
		SDE_ERROR("modeset init failed: %d\n", rc);
		goto drm_obj_init_err;
	}

	return 0;

genpd_err:
drm_obj_init_err:
	sde_core_perf_destroy(&sde_kms->perf);
hw_intr_init_err:
perf_err:
power_error:
	return rc;
}

static int sde_kms_hw_init(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct platform_device *platformdev;
	int i, irq_num, rc = -EINVAL;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		goto end;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev || !dev->dev) {
		SDE_ERROR("invalid device\n");
		goto end;
	}

	platformdev = to_platform_device(dev->dev);
	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("invalid private data\n");
		goto end;
	}

	rc = _sde_kms_hw_init_ioremap(sde_kms, platformdev);
	if (rc)
		goto error;

	rc = _sde_kms_get_splash_data(&sde_kms->splash_data);
	if (rc)
		SDE_DEBUG("sde splash data fetch failed: %d\n", rc);

	rc = pm_runtime_get_sync(sde_kms->dev->dev);
	if (rc < 0) {
		SDE_ERROR("resource enable failed: %d\n", rc);
		goto error;
	}

	rc = _sde_kms_hw_init_blocks(sde_kms, dev, priv);
	if (rc)
		goto hw_init_err;

	dev->mode_config.min_width = sde_kms->catalog->min_display_width;
	dev->mode_config.min_height = sde_kms->catalog->min_display_height;
	dev->mode_config.max_width = sde_kms->catalog->max_display_width;
	dev->mode_config.max_height = sde_kms->catalog->max_display_height;

	mutex_init(&sde_kms->secure_transition_lock);
	mutex_init(&sde_kms->vblank_ctl_global_lock);

	atomic_set(&sde_kms->detach_sec_cb, 0);
	atomic_set(&sde_kms->detach_all_cb, 0);

	/*
	 * Support format modifiers for compression etc.
	 */
	dev->mode_config.allow_fb_modifiers = true;

	/*
	 * Handle (re)initializations during power enable
	 */
	sde_kms_handle_power_event(SDE_POWER_EVENT_POST_ENABLE, sde_kms);
	sde_kms->power_event = sde_power_handle_register_event(&priv->phandle,
			SDE_POWER_EVENT_POST_ENABLE |
			SDE_POWER_EVENT_PRE_DISABLE,
			sde_kms_handle_power_event, sde_kms, "kms");

	if (sde_kms->splash_data.num_splash_displays) {
		SDE_DEBUG("Skipping MDP Resources disable\n");
	} else {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
			sde_power_data_bus_set_quota(&priv->phandle, i,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);

		pm_runtime_put_sync(sde_kms->dev->dev);
	}

	sde_kms->affinity_notify.notify = sde_kms_irq_affinity_notify;
	sde_kms->affinity_notify.release = sde_kms_irq_affinity_release;

	irq_num = platform_get_irq(to_platform_device(sde_kms->dev->dev), 0);
	SDE_DEBUG("Registering for notification of irq_num: %d\n", irq_num);
	irq_set_affinity_notifier(irq_num, &sde_kms->affinity_notify);

	return 0;

hw_init_err:
	pm_runtime_put_sync(sde_kms->dev->dev);
error:
	_sde_kms_hw_destroy(sde_kms, platformdev);
end:
	return rc;
}

struct msm_kms *sde_kms_init(struct drm_device *dev)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!dev || !dev->dev_private) {
		SDE_ERROR("drm device node invalid\n");
		return ERR_PTR(-EINVAL);
	}

	priv = dev->dev_private;

	sde_kms = kzalloc(sizeof(*sde_kms), GFP_KERNEL);
	if (!sde_kms) {
		SDE_ERROR("failed to allocate sde kms\n");
		return ERR_PTR(-ENOMEM);
	}

	msm_kms_init(&sde_kms->base, &kms_funcs);
	sde_kms->dev = dev;

	return &sde_kms->base;
}

static int _sde_kms_register_events(struct msm_kms *kms,
		struct drm_mode_object *obj, u32 event, bool en)
{
	int ret = 0;
	struct drm_crtc *crtc = NULL;
	struct drm_connector *conn = NULL;
	struct sde_kms *sde_kms = NULL;

	if (!kms || !obj) {
		SDE_ERROR("invalid argument kms %pK obj %pK\n", kms, obj);
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);
	switch (obj->type) {
	case DRM_MODE_OBJECT_CRTC:
		crtc = obj_to_crtc(obj);
		ret = sde_crtc_register_custom_event(sde_kms, crtc, event, en);
		break;
	case DRM_MODE_OBJECT_CONNECTOR:
		conn = obj_to_connector(obj);
		ret = sde_connector_register_custom_event(sde_kms, conn, event,
				en);
		break;
	}

	return ret;
}

int sde_kms_handle_recovery(struct drm_encoder *encoder)
{
	SDE_EVT32(DRMID(encoder), MSM_ENC_ACTIVE_REGION);
	return sde_encoder_wait_for_event(encoder, MSM_ENC_ACTIVE_REGION);
}