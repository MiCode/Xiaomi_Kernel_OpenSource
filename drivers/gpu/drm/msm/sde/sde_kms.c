/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-buf.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>

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

#include <soc/qcom/scm.h>
#include "soc/qcom/secure_buffer.h"

#define CREATE_TRACE_POINTS
#include "sde_trace.h"

/* defines for secure channel call */
#define SEC_SID_CNT               2
#define SEC_SID_MASK_0            0x80881
#define SEC_SID_MASK_1            0x80C81
#define MEM_PROTECT_SD_CTRL_SWITCH 0x18
#define MDP_DEVICE_ID            0x1A

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

bool sde_kms_is_vbif_operation_allowed(struct sde_kms *sde_kms)
{
	struct drm_device *dev;
	struct drm_crtc *crtc;
	bool sui_enhancement = false;

	if (!sde_kms || !sde_kms->dev)
		return false;
	dev = sde_kms->dev;

	if (!sde_kms->catalog->sui_misr_supported)
		return true;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (!crtc->state || !crtc->state->active)
			continue;

		sui_enhancement |= sde_crtc_is_sui_enhancement_enabled(crtc);
	}

	if (!sui_enhancement)
		return true;

	return !sde_kms_is_secure_session_inprogress(sde_kms);
}

#ifdef CONFIG_DEBUG_FS
static int _sde_danger_signal_status(struct seq_file *s,
		bool danger_status)
{
	struct sde_kms *kms = (struct sde_kms *)s->private;
	struct msm_drm_private *priv;
	struct sde_danger_safe_status status;
	int i;
	int rc;

	if (!kms || !kms->dev || !kms->dev->dev_private || !kms->hw_mdp) {
		SDE_ERROR("invalid arg(s)\n");
		return 0;
	}

	priv = kms->dev->dev_private;
	memset(&status, 0, sizeof(struct sde_danger_safe_status));

	rc = sde_power_resource_enable(&priv->phandle, kms->core_client, true);
	if (rc) {
		SDE_ERROR("failed to enable power resource %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		return rc;
	}

	if (danger_status) {
		seq_puts(s, "\nDanger signal status:\n");
		if (kms->hw_mdp->ops.get_danger_status)
			kms->hw_mdp->ops.get_danger_status(kms->hw_mdp,
					&status);
	} else {
		seq_puts(s, "\nSafe signal status:\n");
		if (kms->hw_mdp->ops.get_danger_status)
			kms->hw_mdp->ops.get_danger_status(kms->hw_mdp,
					&status);
	}
	sde_power_resource_enable(&priv->phandle, kms->core_client, false);

	seq_printf(s, "MDP     :  0x%x\n", status.mdp);

	for (i = SSPP_VIG0; i < SSPP_MAX; i++)
		seq_printf(s, "SSPP%d   :  0x%x  \t", i - SSPP_VIG0,
				status.sspp[i]);
	seq_puts(s, "\n");

	for (i = WB_0; i < WB_MAX; i++)
		seq_printf(s, "WB%d     :  0x%x  \t", i - WB_0,
				status.wb[i]);
	seq_puts(s, "\n");

	return 0;
}

#define DEFINE_SDE_DEBUGFS_SEQ_FOPS(__prefix)				\
static int __prefix ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __prefix ## _show, inode->i_private);	\
}									\
static const struct file_operations __prefix ## _fops = {		\
	.owner = THIS_MODULE,						\
	.open = __prefix ## _open,					\
	.release = single_release,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
}

static int sde_debugfs_danger_stats_show(struct seq_file *s, void *v)
{
	return _sde_danger_signal_status(s, true);
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_debugfs_danger_stats);

static int sde_debugfs_safe_stats_show(struct seq_file *s, void *v)
{
	return _sde_danger_signal_status(s, false);
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_debugfs_safe_stats);

static void sde_debugfs_danger_destroy(struct sde_kms *sde_kms)
{
	debugfs_remove_recursive(sde_kms->debugfs_danger);
	sde_kms->debugfs_danger = NULL;
}

static int sde_debugfs_danger_init(struct sde_kms *sde_kms,
		struct dentry *parent)
{
	sde_kms->debugfs_danger = debugfs_create_dir("danger",
			parent);
	if (!sde_kms->debugfs_danger) {
		SDE_ERROR("failed to create danger debugfs\n");
		return -EINVAL;
	}

	debugfs_create_file("danger_status", 0600, sde_kms->debugfs_danger,
			sde_kms, &sde_debugfs_danger_stats_fops);
	debugfs_create_file("safe_status", 0600, sde_kms->debugfs_danger,
			sde_kms, &sde_debugfs_safe_stats_fops);

	return 0;
}

static int _sde_debugfs_show_regset32(struct seq_file *s, void *data)
{
	struct sde_debugfs_regset32 *regset;
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	void __iomem *base;
	uint32_t i, addr;

	if (!s || !s->private)
		return 0;

	regset = s->private;

	sde_kms = regset->sde_kms;
	if (!sde_kms || !sde_kms->mmio)
		return 0;

	dev = sde_kms->dev;
	if (!dev)
		return 0;

	priv = dev->dev_private;
	if (!priv)
		return 0;

	base = sde_kms->mmio + regset->offset;

	/* insert padding spaces, if needed */
	if (regset->offset & 0xF) {
		seq_printf(s, "[%x]", regset->offset & ~0xF);
		for (i = 0; i < (regset->offset & 0xF); i += 4)
			seq_puts(s, "         ");
	}

	if (sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, true)) {
		seq_puts(s, "failed to enable sde clocks\n");
		return 0;
	}

	/* main register output */
	for (i = 0; i < regset->blk_len; i += 4) {
		addr = regset->offset + i;
		if ((addr & 0xF) == 0x0)
			seq_printf(s, i ? "\n[%x]" : "[%x]", addr);
		seq_printf(s, " %08x", readl_relaxed(base + i));
	}
	seq_puts(s, "\n");
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	return 0;
}

static int sde_debugfs_open_regset32(struct inode *inode,
		struct file *file)
{
	return single_open(file, _sde_debugfs_show_regset32, inode->i_private);
}

static const struct file_operations sde_fops_regset32 = {
	.open =		sde_debugfs_open_regset32,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

void sde_debugfs_setup_regset32(struct sde_debugfs_regset32 *regset,
		uint32_t offset, uint32_t length, struct sde_kms *sde_kms)
{
	if (regset) {
		regset->offset = offset;
		regset->blk_len = length;
		regset->sde_kms = sde_kms;
	}
}

void *sde_debugfs_create_regset32(const char *name, umode_t mode,
		void *parent, struct sde_debugfs_regset32 *regset)
{
	if (!name || !regset || !regset->sde_kms || !regset->blk_len)
		return NULL;

	/* make sure offset is a multiple of 4 */
	regset->offset = round_down(regset->offset, 4);

	return debugfs_create_file(name, mode, parent,
			regset, &sde_fops_regset32);
}

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

	(void) sde_debugfs_danger_init(sde_kms, debugfs_root);
	(void) sde_debugfs_vbif_init(sde_kms, debugfs_root);
	(void) sde_debugfs_core_irq_init(sde_kms, debugfs_root);

	rc = sde_core_perf_debugfs_init(&sde_kms->perf, debugfs_root);
	if (rc) {
		SDE_ERROR("failed to init perf %d\n", rc);
		return rc;
	}

	return 0;
}

static void _sde_debugfs_destroy(struct sde_kms *sde_kms)
{
	/* don't need to NULL check debugfs_root */
	if (sde_kms) {
		sde_debugfs_vbif_destroy(sde_kms);
		sde_debugfs_danger_destroy(sde_kms);
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
	return sde_crtc_vblank(crtc, true);
}

static void sde_kms_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	sde_crtc_vblank(crtc, false);
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
 * @vimd: switch the stage 2 translation to this VMID.
 */
static int _sde_kms_scm_call(int vmid)
{
	struct scm_desc desc = {0};
	uint32_t num_sids;
	uint32_t *sec_sid;
	uint32_t mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_SWITCH;
	int ret = 0;

	/* This info should be queried from catalog */
	num_sids = SEC_SID_CNT;
	sec_sid = kcalloc(num_sids, sizeof(uint32_t), GFP_KERNEL);
	if (!sec_sid)
		return -ENOMEM;

	/*
	 * derive this info from device tree/catalog, this is combination of
	 * smr mask and SID for secure
	 */
	sec_sid[0] = SEC_SID_MASK_0;
	sec_sid[1] = SEC_SID_MASK_1;
	dmac_flush_range(sec_sid, sec_sid + num_sids);

	SDE_DEBUG("calling scm_call for vmid %d", vmid);

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
	desc.args[0] = MDP_DEVICE_ID;
	desc.args[1] = SCM_BUFFER_PHYS(sec_sid);
	desc.args[2] = sizeof(uint32_t) * num_sids;
	desc.args[3] =  vmid;

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				mem_protect_sd_ctrl_id), &desc);
	if (ret)
		SDE_ERROR("Error:scm_call2, vmid (%lld): ret%d\n",
				desc.args[3], ret);
	SDE_EVT32(mem_protect_sd_ctrl_id,
			desc.args[0], desc.args[3], num_sids,
			sec_sid[0], sec_sid[1], ret);

	kfree(sec_sid);
	return ret;
}

static int _sde_kms_detach_all_cb(struct sde_kms *sde_kms)
{
	u32 ret = 0;

	if (atomic_inc_return(&sde_kms->detach_all_cb) > 1)
		goto end;

	/* detach_all_contexts */
	ret = sde_kms_mmu_detach(sde_kms, false);
	if (ret) {
		SDE_ERROR("failed to detach all cb ret:%d\n", ret);
		goto end;
	}

	ret = _sde_kms_scm_call(VMID_CP_SEC_DISPLAY);
	if (ret)
		goto end;

end:
	return ret;
}

static int _sde_kms_attach_all_cb(struct sde_kms *sde_kms)
{
	u32 ret = 0;

	if (atomic_dec_return(&sde_kms->detach_all_cb) != 0)
		goto end;

	ret = _sde_kms_scm_call(VMID_CP_PIXEL);
	if (ret)
		goto end;

	/* attach_all_contexts */
	ret = sde_kms_mmu_attach(sde_kms, false);
	if (ret) {
		SDE_ERROR("failed to attach all cb ret:%d\n", ret);
		goto end;
	}

end:
	return ret;
}

static int _sde_kms_detach_sec_cb(struct sde_kms *sde_kms)
{
	u32 ret = 0;

	if (atomic_inc_return(&sde_kms->detach_sec_cb) > 1)
		goto end;

	/* detach secure_context */
	ret = sde_kms_mmu_detach(sde_kms, true);
	if (ret) {
		SDE_ERROR("failed to detach sec cb ret:%d\n", ret);
		goto end;
	}

	ret = _sde_kms_scm_call(VMID_CP_CAMERA_PREVIEW);
	if (ret)
		goto end;

end:
	return ret;
}

static int _sde_kms_attach_sec_cb(struct sde_kms *sde_kms)
{
	u32 ret = 0;

	if (atomic_dec_return(&sde_kms->detach_sec_cb) != 0)
		goto end;

	ret = _sde_kms_scm_call(VMID_CP_PIXEL);
	if (ret)
		goto end;

	ret = sde_kms_mmu_attach(sde_kms, true);
	if (ret) {
		SDE_ERROR("failed to attach sec cb ret:%d\n", ret);
		goto end;
	}

end:
	return ret;
}

static int _sde_kms_sui_misr_ctrl(struct sde_kms *sde_kms,
		struct drm_crtc *crtc, bool enable)
{
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;
	int ret;

	if (enable) {
		ret = sde_power_resource_enable(&priv->phandle,
					sde_kms->core_client, true);
		if (ret) {
			SDE_ERROR("failed to enable resource, ret:%d\n", ret);
			return ret;
		}

		sde_crtc_misr_setup(crtc, true, 1);

		ret = _sde_kms_secure_ctrl_xin_clients(sde_kms, crtc, true);
		if (ret) {
			sde_power_resource_enable(&priv->phandle,
					sde_kms->core_client, false);
			return ret;
		}

	} else {
		_sde_kms_secure_ctrl_xin_clients(sde_kms, crtc, false);
		sde_crtc_misr_setup(crtc, false, 0);
		sde_power_resource_enable(&priv->phandle,
					sde_kms->core_client, false);
	}

	return 0;
}

static int _sde_kms_secure_ctrl(struct sde_kms *sde_kms, struct drm_crtc *crtc,
		bool post_commit)
{
	struct sde_kms_smmu_state_data *smmu_state = &sde_kms->smmu_state;
	int old_smmu_state = smmu_state->state;
	int ret = 0;

	if (!sde_kms || !crtc) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->transition_type,
			post_commit, smmu_state->sui_misr_state,
			SDE_EVTLOG_FUNC_ENTRY);

	if ((!smmu_state->transition_type) ||
	    ((smmu_state->transition_type == POST_COMMIT) && !post_commit))
		/* Bail out */
		return 0;

	/* enable sui misr if requested, before the transition */
	if (smmu_state->sui_misr_state == SUI_MISR_ENABLE_REQ) {
		ret = _sde_kms_sui_misr_ctrl(sde_kms, crtc, true);
		if (ret)
			goto end;
	}

	mutex_lock(&sde_kms->secure_transition_lock);
	switch (smmu_state->state) {
	/* Secure UI use case enable */
	case DETACH_ALL_REQ:
		ret = _sde_kms_detach_all_cb(sde_kms);
		if (!ret)
			smmu_state->state = DETACHED;
		break;

	/* Secure UI use case disable */
	case ATTACH_ALL_REQ:
		ret = _sde_kms_attach_all_cb(sde_kms);
		if (!ret)
			smmu_state->state = ATTACHED;
		break;

	/* Secure preview enable */
	case DETACH_SEC_REQ:
		ret = _sde_kms_detach_sec_cb(sde_kms);
		if (!ret)
			smmu_state->state = DETACHED_SEC;
		break;

	/* Secure preview disable */
	case ATTACH_SEC_REQ:
		ret = _sde_kms_attach_sec_cb(sde_kms);
		if (!ret)
			smmu_state->state = ATTACHED;
		break;

	default:
		SDE_ERROR("crtc:%d invalid smmu state:%d transition type:%d\n",
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
	smmu_state->sui_misr_state = NONE;
	smmu_state->transition_type = NONE;
	smmu_state->transition_error = ret ? true : false;

	SDE_DEBUG("crtc:%d, old_state %d new_state %d, ret %d\n",
			DRMID(crtc), old_smmu_state, smmu_state->state, ret);
	SDE_EVT32(DRMID(crtc), smmu_state->state, smmu_state->transition_type,
			smmu_state->transition_error, ret,
			SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

static int sde_kms_prepare_secure_transition(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;

	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	int i, ops = 0, ret = 0;
	bool old_valid_fb = false;

	for_each_crtc_in_state(state, crtc, old_crtc_state, i) {
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
		for_each_plane_in_state(state, plane, plane_state, i) {
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

		if (!ops)
			goto no_ops;

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

static int _sde_kms_release_splash_buffer(unsigned int mem_addr,
							unsigned int size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;
	int ret = 0;

	if (!mem_addr || !size)
		SDE_ERROR("invalid params\n");

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + size) >> PAGE_SHIFT;

	ret = memblock_free(mem_addr, size);
	if (ret) {
		SDE_ERROR("continuous splash memory free failed:%d\n", ret);
		return ret;
	}
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));

	return ret;

}

static int _sde_kms_splash_smmu_map(struct drm_device *dev, struct msm_mmu *mmu,
		struct sde_splash_data *data)
{
	int ret = 0;

	if (!mmu || !data)
		return -EINVAL;

	ret = mmu->funcs->one_to_one_map(mmu, data->splash_base,
				data->splash_base, data->splash_size,
				IOMMU_READ | IOMMU_NOEXEC);
	if (ret)
		SDE_ERROR("Splash smmu map failed: %d\n", ret);

	return ret;
}

static int _sde_kms_splash_smmu_unmap(struct sde_kms *sde_kms)
{
	struct sde_splash_data *data;
	struct msm_mmu *mmu;
	int rc = 0;

	if (!sde_kms)
		return -EINVAL;

	data = &sde_kms->splash_data;
	if (!data) {
		SDE_ERROR("Invalid splash data\n");
		return -EINVAL;
	}

	if (!sde_kms->aspace[0]) {
		SDE_ERROR("aspace not found for sde kms node\n");
		return -EINVAL;
	}

	mmu = sde_kms->aspace[0]->mmu;
	if (!mmu) {
		SDE_ERROR("mmu not found for aspace\n");
		return -EINVAL;
	}

	if (mmu->funcs && mmu->funcs->one_to_one_unmap)
		mmu->funcs->one_to_one_unmap(mmu, data->splash_base,
				data->splash_size);

	return rc;
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
	int i, rc = 0;

	if (!kms)
		return;
	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	if (!dev || !dev->dev_private)
		return;
	priv = dev->dev_private;

	rc = sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
			true);
	if (rc) {
		SDE_ERROR("failed to enable power resource %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		return;
	}

	if (sde_kms->first_kickoff) {
		sde_power_scale_reg_bus(&priv->phandle, sde_kms->core_client,
			VOTE_INDEX_HIGH, false);
		sde_kms->first_kickoff = false;
	}

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		list_for_each_entry(encoder, &dev->mode_config.encoder_list,
				head) {
			if (encoder->crtc != crtc)
				continue;

			sde_encoder_prepare_commit(encoder);
		}
	}

	/*
	 * NOTE: for secure use cases we want to apply the new HW
	 * configuration only after completing preparation for secure
	 * transitions prepare below if any transtions is required.
	 */
	sde_kms_prepare_secure_transition(kms, state);
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

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active) {
			SDE_EVT32(DRMID(crtc));
			sde_crtc_commit_kickoff(crtc, old_crtc_state);
		}
	}
}

static void _sde_kms_release_splash_resource(struct sde_kms *sde_kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	bool primary_crtc_active = false;
	struct msm_drm_private *priv;
	int i, rc = 0;

	priv = sde_kms->dev->dev_private;

	if (!sde_kms->splash_data.resource_handoff_pending)
		return;

	SDE_EVT32(SDE_EVTLOG_FUNC_CASE1);
	for_each_crtc_in_state(old_state, crtc, crtc_state, i) {
		if (crtc->state->active)
			primary_crtc_active = true;
		SDE_EVT32(crtc->base.id, crtc->state->active);
	}

	if (!primary_crtc_active) {
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE2);
		return;
	}

	sde_kms->splash_data.resource_handoff_pending = false;

	if (sde_kms->splash_data.cont_splash_en) {
		SDE_DEBUG("disabling cont_splash feature\n");
		sde_kms->splash_data.cont_splash_en = false;

		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
			sde_power_data_bus_set_quota(&priv->phandle,
				sde_kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT, i,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);

		sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
			false);
	}

	if (sde_kms->splash_data.splash_base) {
		_sde_kms_splash_smmu_unmap(sde_kms);

		rc = _sde_kms_release_splash_buffer(
			sde_kms->splash_data.splash_base,
			sde_kms->splash_data.splash_size);
		if (rc)
			pr_err("failed to release splash memory\n");
		sde_kms->splash_data.splash_base = 0;
		sde_kms->splash_data.splash_size = 0;
	}
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
	int i, rc = 0;

	if (!kms || !old_state)
		return;
	sde_kms = to_sde_kms(kms);

	if (!sde_kms->dev || !sde_kms->dev->dev_private)
		return;
	priv = sde_kms->dev->dev_private;

	if (!sde_kms_power_resource_is_enabled(sde_kms->dev)) {
		SDE_ERROR("power resource is not enabled\n");
		return;
	}

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		sde_crtc_complete_commit(crtc, old_crtc_state);

		/* complete secure transitions if any */
		if (sde_kms->smmu_state.transition_type == POST_COMMIT)
			_sde_kms_secure_ctrl(sde_kms, crtc, true);
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		struct sde_connector *c_conn;

		c_conn = to_sde_connector(connector);
		if (!c_conn->ops.post_kickoff)
			continue;
		rc = c_conn->ops.post_kickoff(connector);
		if (rc) {
			pr_err("Connector Post kickoff failed rc=%d\n",
					 rc);
		}
	}

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	_sde_kms_release_splash_resource(sde_kms, old_state);

	SDE_EVT32_VERBOSE(SDE_EVTLOG_FUNC_EXIT);
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
			break;
		}
	}
}

static void sde_kms_prepare_fence(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i, rc;

	if (!kms || !old_state || !old_state->dev || !old_state->acquire_ctx) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

retry:
	/* attempt to acquire ww mutex for connection */
	rc = drm_modeset_lock(&old_state->dev->mode_config.connection_mutex,
			       old_state->acquire_ctx);

	if (rc == -EDEADLK) {
		drm_modeset_backoff(old_state->acquire_ctx);
		goto retry;
	}

	/* old_state actually contains updated crtc pointers */
	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active)
			sde_crtc_prepare_commit(crtc, old_crtc_state);
	}
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
	}
	return 0;

exit_deinit_dp:
	kfree(sde_kms->dp_displays);
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
		.put_modes =  dsi_connector_put_modes,
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
		.mode_valid = dp_connector_mode_valid,
		.get_info   = dp_connector_get_info,
		.get_mode_info  = dp_connector_get_mode_info,
		.post_open  = dp_connector_post_open,
		.check_status = NULL,
		.config_hdr = dp_connector_config_hdr,
		.cmd_transfer = NULL,
		.cont_splash_config = NULL,
		.get_panel_vfp = NULL,
	};
	static const struct sde_connector_ops ext_bridge_ops = {
		.set_info_blob = dsi_conn_set_info_blob,
		.mode_valid = dsi_conn_mode_valid,
		.get_info = dsi_display_ext_bridge_get_info,
		.soft_reset = dsi_display_soft_reset,
		.clk_ctrl = dsi_display_clk_ctrl,
		.get_mode_info = dsi_conn_ext_bridge_get_mode_info,
		.get_dst_format = dsi_display_get_dst_format,
		.enable_event = dsi_conn_enable_event,
		.cmd_transfer = NULL,
		.cont_splash_config = NULL,
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
				sde_kms->dp_display_count;
	if (max_encoders > ARRAY_SIZE(priv->encoders)) {
		max_encoders = ARRAY_SIZE(priv->encoders);
		SDE_ERROR("capping number of displays to %d", max_encoders);
	}

	/* dsi */
	for (i = 0; i < sde_kms->dsi_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->dsi_displays[i];
		encoder = NULL;

		if (!dsi_display_has_ext_bridge(display)) {
			memset(&info, 0x0, sizeof(info));
			rc = dsi_display_get_info(&info, display);
			if (rc) {
				SDE_ERROR("dsi get_info %d failed\n", i);
				continue;
			}

			encoder = sde_encoder_init(dev, &info);
			if (IS_ERR_OR_NULL(encoder)) {
				SDE_ERROR("encoder init failed for dsi %d\n",
					i);
				continue;
			}

			rc = dsi_display_drm_bridge_init(display, encoder);
			if (rc) {
				SDE_ERROR("dsi bridge %d init failed, %d\n",
					i, rc);
				sde_encoder_destroy(encoder);
				continue;
			}

			connector = sde_connector_init(dev,
						encoder,
						NULL,
						display,
						&dsi_ops,
						DRM_CONNECTOR_POLL_HPD,
						DRM_MODE_CONNECTOR_DSI);
			if (connector) {
				priv->encoders[priv->num_encoders++] = encoder;
			} else {
				SDE_ERROR("dsi %d connector init failed\n", i);
				dsi_display_drm_bridge_deinit(display);
				sde_encoder_destroy(encoder);
			}
		} else {
			memset(&info, 0x0, sizeof(info));
			rc = dsi_display_ext_bridge_get_info(&info, display);
			if (rc) {
				SDE_ERROR("ext get_info %d failed\n", i);
				continue;
			}

			encoder = sde_encoder_init(dev, &info);
			if (IS_ERR_OR_NULL(encoder)) {
				SDE_ERROR("encoder init failed for ext %d\n",
					i);
				continue;
			}

			rc = dsi_display_drm_bridge_init(display, encoder);
			if (rc) {
				SDE_ERROR("dsi bridge %d init failed for ext\n",
					i);
				sde_encoder_destroy(encoder);
				continue;
			}

			connector = sde_connector_init(dev,
						encoder,
						NULL,
						display,
						&ext_bridge_ops,
						DRM_CONNECTOR_POLL_HPD,
						DRM_MODE_CONNECTOR_DSI);
			if (connector) {
				priv->encoders[priv->num_encoders++] = encoder;
			} else {
				SDE_ERROR("connector init %d failed for ext\n",
					i);
				dsi_display_drm_bridge_deinit(display);
				sde_encoder_destroy(encoder);
				continue;
			}

			rc = dsi_display_drm_ext_bridge_init(display,
				encoder, connector);
			if (rc) {
				struct drm_connector *conn = connector;

				SDE_ERROR("ext bridge %d init failed, %d\n",
					i, rc);
				conn->funcs->destroy(connector);
				dsi_display_drm_bridge_deinit(display);
				sde_encoder_destroy(encoder);
				continue;
			}
		}
	}

	/* wb */
	for (i = 0; i < sde_kms->wb_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->wb_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = sde_wb_get_info(&info, display);
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
		} else {
			SDE_ERROR("wb %d connector init failed\n", i);
			sde_wb_drm_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}
	/* dp */
	for (i = 0; i < sde_kms->dp_display_count &&
			priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->dp_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = dp_connector_get_info(&info, display);
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
		} else {
			SDE_ERROR("dp %d connector init failed\n", i);
			dp_drm_bridge_deinit(display);
			sde_encoder_destroy(encoder);
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
		drm_for_each_connector(conn, dev)
			sde_conn_timeline_status(conn);
		return;
	}

	mutex_lock(&dev->mode_config.mutex);
	drm_for_each_connector(conn, dev)
		sde_conn_timeline_status(conn);
	mutex_unlock(&dev->mode_config.mutex);
}

/**
 * struct sde_kms_fbo_fb - framebuffer creation list
 * @list: list of framebuffer attached to framebuffer object
 * @fb: Pointer to framebuffer attached to framebuffer object
 */
struct sde_kms_fbo_fb {
	struct list_head list;
	struct drm_framebuffer *fb;
};

struct drm_framebuffer *sde_kms_fbo_create_fb(struct drm_device *dev,
		struct sde_kms_fbo *fbo)
{
	struct drm_framebuffer *fb = NULL;
	struct sde_kms_fbo_fb *fbo_fb;
	struct drm_mode_fb_cmd2 mode_cmd = {0};
	u32 base_offset = 0;
	int i, ret;

	if (!dev) {
		SDE_ERROR("invalid drm device node\n");
		return NULL;
	}

	fbo_fb = kzalloc(sizeof(struct sde_kms_fbo_fb), GFP_KERNEL);
	if (!fbo_fb)
		return NULL;

	mode_cmd.pixel_format = fbo->pixel_format;
	mode_cmd.width = fbo->width;
	mode_cmd.height = fbo->height;
	mode_cmd.flags = fbo->flags;

	for (i = 0; i < fbo->nplane; i++) {
		mode_cmd.offsets[i] = base_offset;
		mode_cmd.pitches[i] = fbo->layout.plane_pitch[i];
		mode_cmd.modifier[i] = fbo->modifier[i];
		base_offset += fbo->layout.plane_size[i];
		SDE_DEBUG("offset[%d]:%x\n", i, mode_cmd.offsets[i]);
	}

	fb = msm_framebuffer_init(dev, &mode_cmd, fbo->bo);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		fb = NULL;
		SDE_ERROR("failed to allocate fb %d\n", ret);
		goto fail;
	}

	/* need to take one reference for gem object */
	for (i = 0; i < fbo->nplane; i++)
		drm_gem_object_reference(fbo->bo[i]);

	SDE_DEBUG("register private fb:%d\n", fb->base.id);

	INIT_LIST_HEAD(&fbo_fb->list);
	fbo_fb->fb = fb;
	drm_framebuffer_reference(fbo_fb->fb);
	list_add_tail(&fbo_fb->list, &fbo->fb_list);

	return fb;

fail:
	kfree(fbo_fb);
	return NULL;
}

static void sde_kms_fbo_destroy(struct sde_kms_fbo *fbo)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct sde_kms_fbo_fb *curr, *next;
	int i;

	if (!fbo) {
		SDE_ERROR("invalid drm device node\n");
		return;
	}
	dev = fbo->dev;

	if (!dev || !dev->dev_private) {
		SDE_ERROR("invalid drm device node\n");
		return;
	}
	priv = dev->dev_private;

	if (!priv->kms) {
		SDE_ERROR("invalid kms handle\n");
		return;
	}
	sde_kms = to_sde_kms(priv->kms);

	SDE_DEBUG("%dx%d@%c%c%c%c/%llx/%x\n", fbo->width, fbo->height,
			fbo->pixel_format >> 0, fbo->pixel_format >> 8,
			fbo->pixel_format >> 16, fbo->pixel_format >> 24,
			fbo->modifier[0], fbo->flags);

	list_for_each_entry_safe(curr, next, &fbo->fb_list, list) {
		SDE_DEBUG("unregister private fb:%d\n", curr->fb->base.id);
		drm_framebuffer_unregister_private(curr->fb);
		drm_framebuffer_unreference(curr->fb);
		list_del(&curr->list);
		kfree(curr);
	}

	for (i = 0; i < fbo->layout.num_planes; i++) {
		if (fbo->bo[i]) {
			mutex_lock(&dev->struct_mutex);
			drm_gem_object_unreference(fbo->bo[i]);
			mutex_unlock(&dev->struct_mutex);
			fbo->bo[i] = NULL;
		}
	}

	if (fbo->dma_buf) {
		dma_buf_put(fbo->dma_buf);
		fbo->dma_buf = NULL;
	}

	if (sde_kms->iclient && fbo->ihandle) {
		ion_free(sde_kms->iclient, fbo->ihandle);
		fbo->ihandle = NULL;
	}
}

static void sde_kms_set_gem_flags(struct msm_gem_object *msm_obj,
		uint32_t flags)
{
	if (msm_obj)
		msm_obj->flags |= flags;
}

struct sde_kms_fbo *sde_kms_fbo_alloc(struct drm_device *dev, u32 width,
		u32 height, u32 pixel_format, u64 modifier[4], u32 flags)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_kms_fbo *fbo;
	int i, ret;

	if (!dev || !dev->dev_private) {
		SDE_ERROR("invalid drm device node\n");
		return NULL;
	}
	priv = dev->dev_private;

	if (!priv->kms) {
		SDE_ERROR("invalid kms handle\n");
		return NULL;
	}
	sde_kms = to_sde_kms(priv->kms);

	SDE_DEBUG("%dx%d@%c%c%c%c/%llx/%x\n", width, height,
			pixel_format >> 0, pixel_format >> 8,
			pixel_format >> 16, pixel_format >> 24,
			modifier[0], flags);

	fbo = kzalloc(sizeof(struct sde_kms_fbo), GFP_KERNEL);
	if (!fbo)
		return NULL;

	atomic_set(&fbo->refcount, 0);
	INIT_LIST_HEAD(&fbo->fb_list);
	fbo->dev = dev;
	fbo->width = width;
	fbo->height = height;
	fbo->pixel_format = pixel_format;
	fbo->flags = flags;
	for (i = 0; i < ARRAY_SIZE(fbo->modifier); i++)
		fbo->modifier[i] = modifier[i];
	fbo->nplane = drm_format_num_planes(fbo->pixel_format);
	fbo->fmt = sde_get_sde_format_ext(fbo->pixel_format, fbo->modifier,
			fbo->nplane);
	if (!fbo->fmt) {
		ret = -EINVAL;
		SDE_ERROR("failed to find pixel format\n");
		goto done;
	}

	ret = sde_format_get_plane_sizes(fbo->fmt, fbo->width, fbo->height,
			&fbo->layout, fbo->layout.plane_pitch);
	if (ret) {
		SDE_ERROR("failed to get plane sizes\n");
		goto done;
	}

	/* allocate backing buffer object */
	if (sde_kms->iclient) {
		u32 heap_id = fbo->flags & DRM_MODE_FB_SECURE ?
				ION_HEAP(ION_SECURE_HEAP_ID) :
				ION_HEAP(ION_SYSTEM_HEAP_ID);
		u32 iflags = fbo->flags & DRM_MODE_FB_SECURE ?
				(ION_FLAG_SECURE | ION_FLAG_CP_PIXEL) : 0;

		fbo->ihandle = ion_alloc(sde_kms->iclient,
				fbo->layout.total_size, SZ_4K, heap_id, iflags);
		if (IS_ERR_OR_NULL(fbo->ihandle)) {
			SDE_ERROR("failed to alloc ion memory\n");
			ret = PTR_ERR(fbo->ihandle);
			fbo->ihandle = NULL;
			goto done;
		}

		fbo->dma_buf = ion_share_dma_buf(sde_kms->iclient,
				fbo->ihandle);
		if (IS_ERR(fbo->dma_buf)) {
			SDE_ERROR("failed to share ion memory\n");
			ret = -ENOMEM;
			fbo->dma_buf = NULL;
			goto done;
		}

		fbo->bo[0] = dev->driver->gem_prime_import(dev,
				fbo->dma_buf);
		if (IS_ERR(fbo->bo[0])) {
			SDE_ERROR("failed to import ion memory\n");
			ret = PTR_ERR(fbo->bo[0]);
			fbo->bo[0] = NULL;
			goto done;
		}

		/* insert extra bo flags */
		sde_kms_set_gem_flags(to_msm_bo(fbo->bo[0]), MSM_BO_KEEPATTRS);
	} else {
		mutex_lock(&dev->struct_mutex);
		fbo->bo[0] = msm_gem_new(dev, fbo->layout.total_size,
				MSM_BO_SCANOUT | MSM_BO_WC | MSM_BO_KEEPATTRS);
		if (IS_ERR(fbo->bo[0])) {
			mutex_unlock(&dev->struct_mutex);
			SDE_ERROR("failed to new gem buffer\n");
			ret = PTR_ERR(fbo->bo[0]);
			fbo->bo[0] = NULL;
			goto done;
		}
		mutex_unlock(&dev->struct_mutex);
	}

	mutex_lock(&dev->struct_mutex);
	for (i = 1; i < fbo->layout.num_planes; i++) {
		fbo->bo[i] = fbo->bo[0];
		drm_gem_object_reference(fbo->bo[i]);
	}
	mutex_unlock(&dev->struct_mutex);

done:
	if (ret) {
		sde_kms_fbo_destroy(fbo);
		kfree(fbo);
		fbo = NULL;
	} else {
		sde_kms_fbo_reference(fbo);
	}

	return fbo;
}

int sde_kms_fbo_reference(struct sde_kms_fbo *fbo)
{
	if (!fbo) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	SDE_DEBUG("%pS refcount:%d\n", __builtin_return_address(0),
			atomic_read(&fbo->refcount));

	atomic_inc(&fbo->refcount);

	return 0;
}

void sde_kms_fbo_unreference(struct sde_kms_fbo *fbo)
{
	if (!fbo) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	SDE_DEBUG("%pS refcount:%d\n", __builtin_return_address(0),
			atomic_read(&fbo->refcount));

	if (!atomic_read(&fbo->refcount)) {
		SDE_ERROR("invalid refcount\n");
		return;
	} else if (atomic_dec_return(&fbo->refcount) == 0) {
		sde_kms_fbo_destroy(fbo);
	}
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
	(void)_sde_kms_release_splash_buffer(
				sde_kms->splash_data.splash_base,
				sde_kms->splash_data.splash_size);

	/* safe to call these more than once during shutdown */
	_sde_debugfs_destroy(sde_kms);
	_sde_kms_mmu_destroy(sde_kms);

	if (sde_kms->iclient) {
		ion_client_destroy(sde_kms->iclient);
		sde_kms->iclient = NULL;
	}

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

	if (sde_kms->core_client)
		sde_power_client_destroy(&priv->phandle, sde_kms->core_client);
	sde_kms->core_client = NULL;

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
	if (!dev) {
		SDE_ERROR("invalid device\n");
		return;
	}

	_sde_kms_hw_destroy(sde_kms, dev->platformdev);
	kfree(sde_kms);
}

static void _sde_kms_plane_force_remove(struct drm_plane *plane,
			struct drm_atomic_state *state)
{
	struct drm_plane_state *plane_state;
	int ret = 0;

	if (!plane->crtc)
		return;

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
			drm_framebuffer_unreference(fb);
		}
	}

	if (list_empty(&fbs)) {
		SDE_DEBUG("skip commit as no fb(s)\n");
		drm_atomic_state_free(state);
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
		drm_framebuffer_unreference(fb);
	}

end:
	drm_atomic_clean_old_fb(dev, plane_mask, ret);

	return ret;
}

static void sde_kms_preclose(struct msm_kms *kms, struct drm_file *file)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;
	unsigned int i;
	struct drm_atomic_state *state = NULL;
	int ret = 0;

	for (i = 0; i < priv->num_crtcs; i++)
		sde_crtc_cancel_pending_flip(priv->crtcs[i], file);

	drm_modeset_lock_all(dev);
	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto end;
	}

	state->acquire_ctx = dev->mode_config.acquire_ctx;

	for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
		ret = _sde_kms_remove_fbs(sde_kms, file, state);
		if (ret != -EDEADLK)
			break;
		drm_atomic_state_clear(state);
		drm_atomic_legacy_backoff(state);
	}

end:
	if ((ret != 0) && state)
		drm_atomic_state_free(state);

	SDE_DEBUG("sde preclose done, ret:%d\n", ret);
	drm_modeset_unlock_all(dev);
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

	drm_for_each_connector(conn, dev) {
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

	return ret;
}

static void sde_kms_lastclose(struct msm_kms *kms)
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

	state->acquire_ctx = dev->mode_config.acquire_ctx;

	for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
		/* add reset of custom properties to the state */
		ret = _sde_kms_helper_reset_custom_properties(sde_kms, state);
		if (ret)
			break;

		ret = drm_atomic_commit(state);
		if (ret != -EDEADLK)
			break;

		drm_atomic_state_clear(state);
		drm_atomic_legacy_backoff(state);
		SDE_DEBUG("deadlock backoff on attempt %d\n", i);
	}

	if (ret) {
		/**
		 * on success, atomic state object ownership transfers to
		 * framework, otherwise, free it here
		 */
		drm_atomic_state_free(state);
		SDE_ERROR("failed to run last close: %d\n", ret);
	}
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
	int i;

	if (!kms || !state) {
		return -EINVAL;
		SDE_ERROR("invalid arguments\n");
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;

	/* iterate state object for active secure/non-secure crtc */
	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		if (!crtc_state->active)
			continue;

		active_crtc_cnt++;
		if (sde_crtc_get_secure_level(crtc, crtc_state) ==
				SDE_DRM_SEC_ONLY)
			sec_session = true;

		cur_crtc = crtc;
	}

	/* iterate global list for active and secure crtc */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (!crtc->state->active)
			continue;

		global_active_crtc_cnt++;
		if (sde_crtc_get_secure_level(crtc, crtc->state) ==
				SDE_DRM_SEC_ONLY)
			global_sec_session = true;

		global_crtc = crtc;
	}

	/*
	 * - fail secure crtc commit, if any other crtc session is already
	 *   in progress
	 * - fail non-secure crtc commit, if any secure crtc session is already
	 *   in progress
	 */
	if (global_sec_session || sec_session) {
		if ((global_active_crtc_cnt >
					MAX_ALLOWED_CRTC_CNT_DURING_SECURE) ||
		    (active_crtc_cnt > MAX_ALLOWED_CRTC_CNT_DURING_SECURE)) {
			SDE_ERROR(
			"Secure check failed global_active:%d active:%d\n",
				global_active_crtc_cnt, active_crtc_cnt);
			return -EPERM;

		/*
		 * As only one crtc is allowed during secure session, the crtc
		 * in this commit should match with the global crtc, if it
		 * exists
		 */
		} else if (global_crtc && (global_crtc != cur_crtc)) {
			SDE_ERROR(
			    "crtc%d-sec%d not allowed during crtc%d-sec%d\n",
				cur_crtc ? cur_crtc->base.id : -1, sec_session,
				global_crtc->base.id, global_sec_session);
			return -EPERM;
		}

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

	if (sde_kms_is_suspend_blocked(dev)) {
		SDE_DEBUG("suspended, skip atomic_check\n");
		return -EBUSY;
	}

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;
	/*
	 * Check if any secure transition(moving CRTC between secure and
	 * non-secure state and vice-versa) is allowed or not. when moving
	 * to secure state, planes with fb_mode set to dir_translated only can
	 * be staged on the CRTC, and only one CRTC can be active during
	 * Secure state
	 */
	return sde_kms_check_secure_transition(kms, state);
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

static void _sde_kms_post_open(struct msm_kms *kms, struct drm_file *file)
{
	struct drm_device *dev = NULL;
	struct sde_kms *sde_kms = NULL;
	struct drm_connector *connector = NULL;
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
	drm_for_each_connector(connector, dev) {
		/* Only handle HPD capable connectors. */
		if (!(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		sde_conn = to_sde_connector(connector);

		if (sde_conn->ops.post_open)
			sde_conn->ops.post_open(sde_conn->display);
	}
	mutex_unlock(&dev->mode_config.mutex);

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
	struct list_head *connector_list = NULL;
	struct drm_connector *conn_iter = NULL;
	struct drm_connector *connector = NULL;
	struct sde_connector *sde_conn = NULL;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev || !dev->platformdev) {
		SDE_ERROR("invalid device\n");
		return -EINVAL;
	}

	if (!sde_kms->splash_data.cont_splash_en) {
		DRM_INFO("cont_splash feature not enabled\n");
		return rc;
	}

	/* Currently, we only support one dsi display configuration */
	/* dsi */
	for (i = 0; i < sde_kms->dsi_display_count; ++i) {
		display = sde_kms->dsi_displays[i];
		dsi_display = (struct dsi_display *)display;
		SDE_DEBUG("display->name = %s\n", dsi_display->name);

		if (dsi_display->bridge->base.encoder) {
			encoder = dsi_display->bridge->base.encoder;
			SDE_DEBUG("encoder name = %s\n", encoder->name);
		}
		memset(&info, 0x0, sizeof(info));
		rc = dsi_display_get_info(&info, display);
		if (rc) {
			SDE_ERROR("dsi get_info %d failed\n", i);
			encoder = NULL;
			continue;
		}
		SDE_DEBUG("info.is_connected = %s, info.is_primary = %s\n",
			((info.is_connected) ? "true" : "false"),
			((info.is_primary) ? "true" : "false"));

		/**
		 * Since we are supporting one DSI for splash, use the display
		 * which is marked as primary.
		 */
		if (!info.is_primary)
			continue;
		else
			break;
	}

	if (!encoder) {
		SDE_ERROR("encoder not initialized\n");
		return -EINVAL;
	}

	priv = sde_kms->dev->dev_private;
	encoder->crtc = priv->crtcs[0];
	crtc = encoder->crtc;
	SDE_DEBUG("crtc id = %d\n", crtc->base.id);


	mutex_lock(&dev->mode_config.mutex);
	connector_list = &dev->mode_config.connector_list;
	if (connector_list) {
		list_for_each_entry(conn_iter, connector_list, head) {
			/**
			 * SDE_KMS doesn't attach more than one encoder to
			 * a DSI connector. So it is safe to check only with
			 * the first encoder entry. Revisit this logic if we
			 * ever have to support continuous splash for
			 * external displays in MST configuration.
			 */
			if (conn_iter &&
			  (conn_iter->encoder_ids[0] == encoder->base.id)) {
				connector = conn_iter;
				break;
			}
		}
	}
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
	sde_encoder_update_caps_for_cont_splash(encoder);

	sde_crtc_update_cont_splash_mixer_settings(crtc);

	sde_conn = to_sde_connector(connector);
	if (sde_conn && sde_conn->ops.cont_splash_config)
		sde_conn->ops.cont_splash_config(sde_conn->display);

	return rc;
}

static bool sde_kms_check_for_splash(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return false;
	}

	sde_kms = to_sde_kms(kms);
	return sde_kms->splash_data.cont_splash_en;
}

static int sde_kms_pm_suspend(struct device *dev)
{
	struct drm_device *ddev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector *conn;
	struct drm_atomic_state *state;
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

	/* acquire modeset lock(s) */
	drm_modeset_acquire_init(&ctx, 0);

retry:
	ret = drm_modeset_lock_all_ctx(ddev, &ctx);
	if (ret)
		goto unlock;

	/* save current state for resume */
	if (sde_kms->suspend_state)
		drm_atomic_state_free(sde_kms->suspend_state);
	sde_kms->suspend_state = drm_atomic_helper_duplicate_state(ddev, &ctx);
	if (IS_ERR_OR_NULL(sde_kms->suspend_state)) {
		DRM_ERROR("failed to back up suspend state\n");
		sde_kms->suspend_state = NULL;
		goto unlock;
	}

	/* create atomic state to disable all CRTCs */
	state = drm_atomic_state_alloc(ddev);
	if (IS_ERR_OR_NULL(state)) {
		DRM_ERROR("failed to allocate crtc disable state\n");
		goto unlock;
	}

	state->acquire_ctx = &ctx;
	drm_for_each_connector(conn, ddev) {
		struct drm_crtc_state *crtc_state;
		uint64_t lp;

		if (!conn->state || !conn->state->crtc ||
				conn->dpms != DRM_MODE_DPMS_ON)
			continue;

		lp = sde_connector_get_lp(conn);
		if (lp == SDE_MODE_DPMS_LP1) {
			/* transition LP1->LP2 on pm suspend */
			ret = sde_connector_set_property_for_commit(conn, state,
					CONNECTOR_PROP_LP, SDE_MODE_DPMS_LP2);
			if (ret) {
				DRM_ERROR("failed to set lp2 for conn %d\n",
						conn->base.id);
				drm_atomic_state_free(state);
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
				drm_atomic_state_free(state);
				goto unlock;
			}

			if (lp != SDE_MODE_DPMS_LP1)
				crtc_state->active = false;
			++num_crtcs;
		}
	}

	/* check for nothing to do */
	if (num_crtcs == 0) {
		DRM_DEBUG("all crtcs are already in the off state\n");
		drm_atomic_state_free(state);
		sde_kms->suspend_block = true;
		goto unlock;
	}

	/* commit the "disable all" state */
	ret = drm_atomic_commit(state);
	if (ret < 0) {
		DRM_ERROR("failed to disable crtcs, %d\n", ret);
		drm_atomic_state_free(state);
		goto unlock;
	}

	sde_kms->suspend_block = true;

	drm_for_each_connector(conn, ddev) {
		uint64_t lp;

		lp = sde_connector_get_lp(conn);
		if (lp != SDE_MODE_DPMS_LP2)
			continue;

		ret = sde_encoder_wait_for_event(conn->encoder,
						MSM_ENC_TX_COMPLETE);
		if (ret && ret != -EWOULDBLOCK)
			SDE_ERROR(
				"[enc: %d] wait for commit done returned %d\n",
				conn->encoder->base.id, ret);
		else if (!ret)
			sde_encoder_idle_request(conn->encoder);
	}
unlock:
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	}
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return 0;
}

static int sde_kms_pm_resume(struct device *dev)
{
	struct drm_device *ddev;
	struct sde_kms *sde_kms;
	int ret;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev_to_msm_kms(ddev))
		return -EINVAL;

	sde_kms = to_sde_kms(ddev_to_msm_kms(ddev));

	SDE_EVT32(sde_kms->suspend_state != NULL);

	drm_mode_config_reset(ddev);

	drm_modeset_lock_all(ddev);

	sde_kms->suspend_block = false;

	if (sde_kms->suspend_state) {
		sde_kms->suspend_state->acquire_ctx =
			ddev->mode_config.acquire_ctx;
		ret = drm_atomic_commit(sde_kms->suspend_state);
		if (ret < 0) {
			DRM_ERROR("failed to restore state, %d\n", ret);
			drm_atomic_state_free(sde_kms->suspend_state);
		}
		sde_kms->suspend_state = NULL;
	}
	drm_modeset_unlock_all(ddev);

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
	.postopen = _sde_kms_post_open,
	.check_for_splash = sde_kms_check_for_splash,
};

/* the caller api needs to turn on clock before calling it */
static inline void _sde_kms_core_hw_rev_init(struct sde_kms *sde_kms)
{
	sde_kms->core_rev = readl_relaxed(sde_kms->mmio + 0x0);
}

static int _sde_kms_mmu_destroy(struct sde_kms *sde_kms)
{
	struct msm_mmu *mmu;
	int i;

	for (i = ARRAY_SIZE(sde_kms->aspace) - 1; i >= 0; i--) {
		if (!sde_kms->aspace[i])
			continue;

		mmu = sde_kms->aspace[i]->mmu;

		mmu->funcs->detach(mmu, (const char **)iommu_ports,
				ARRAY_SIZE(iommu_ports));
		msm_gem_address_space_destroy(sde_kms->aspace[i]);

		sde_kms->aspace[i] = NULL;
	}

	return 0;
}

static int _sde_kms_mmu_init(struct sde_kms *sde_kms)
{
	struct msm_mmu *mmu;
	int i, ret;
	int early_map = 1;

	for (i = 0; i < MSM_SMMU_DOMAIN_MAX; i++) {
		struct msm_gem_address_space *aspace;

		mmu = msm_smmu_new(sde_kms->dev->dev, i);
		if (IS_ERR(mmu)) {
			ret = PTR_ERR(mmu);
			SDE_DEBUG("failed to init iommu id %d: rc:%d\n",
								i, ret);
			continue;
		}

		/*
		 * Before attaching SMMU, we need to honor continuous splash
		 * use case where hardware tries to fetch buffer from physical
		 * address. To facilitate this requirement we need to have a
		 * one to one mapping on SMMU until we have our first frame.
		 */
		if (i == MSM_SMMU_DOMAIN_UNSECURE) {
			ret = mmu->funcs->set_attribute(mmu,
				DOMAIN_ATTR_EARLY_MAP,
				&early_map);
			if (ret) {
				SDE_ERROR("failed to set map att: %d\n", ret);
				goto fail;
			}
		}

		aspace = msm_gem_smmu_address_space_create(sde_kms->dev,
			mmu, "sde");
		if (IS_ERR(aspace)) {
			ret = PTR_ERR(aspace);
			mmu->funcs->destroy(mmu);
			goto fail;
		}

		sde_kms->aspace[i] = aspace;

		ret = mmu->funcs->attach(mmu, (const char **)iommu_ports,
				ARRAY_SIZE(iommu_ports));
		if (ret) {
			SDE_ERROR("failed to attach iommu %d: %d\n", i, ret);
			msm_gem_address_space_destroy(aspace);
			goto fail;
		}
		aspace->domain_attached = true;
		early_map = 0;

		/* Mapping splash memory block */
		if ((i == MSM_SMMU_DOMAIN_UNSECURE) &&
				sde_kms->splash_data.splash_base) {
			ret = _sde_kms_splash_smmu_map(sde_kms->dev, mmu,
					&sde_kms->splash_data);
			if (ret) {
				SDE_ERROR("failed to map ret:%d\n", ret);
				goto fail;
			}
		}

		/*
		 * Turning off early map after generating one to one
		 * mapping for splash address space.
		 */
		ret = mmu->funcs->set_attribute(mmu, DOMAIN_ATTR_EARLY_MAP,
			&early_map);
		if (ret) {
			SDE_ERROR("failed to set map att ret:%d\n", ret);
			goto early_map_fail;
		}
	}

	return 0;
early_map_fail:
	mmu->funcs->one_to_one_unmap(mmu, sde_kms->splash_data.splash_base,
					sde_kms->splash_data.splash_size);
fail:
	mmu->funcs->destroy(mmu);
	_sde_kms_mmu_destroy(sde_kms);

	return ret;
}

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
		sde_kms->first_kickoff = true;
	} else if (event_type == SDE_POWER_EVENT_PRE_DISABLE) {
		sde_irq_update(msm_kms, false);
		sde_kms->first_kickoff = false;
	}
}

#define genpd_to_sde_kms(domain) container_of(domain, struct sde_kms, genpd)

static int sde_kms_pd_enable(struct generic_pm_domain *genpd)
{
	struct sde_kms *sde_kms = genpd_to_sde_kms(genpd);
	struct drm_device *dev;
	struct msm_drm_private *priv;
	int rc;

	SDE_DEBUG("\n");

	dev = sde_kms->dev;
	if (!dev)
		return -EINVAL;

	priv = dev->dev_private;
	if (!priv)
		return -EINVAL;

	SDE_EVT32(genpd->device_count);

	rc = sde_power_resource_enable(&priv->phandle, priv->pclient, true);

	return rc;
}

static int sde_kms_pd_disable(struct generic_pm_domain *genpd)
{
	struct sde_kms *sde_kms = genpd_to_sde_kms(genpd);
	struct drm_device *dev;
	struct msm_drm_private *priv;
	int rc;

	SDE_DEBUG("\n");

	dev = sde_kms->dev;
	if (!dev)
		return -EINVAL;

	priv = dev->dev_private;
	if (!priv)
		return -EINVAL;

	SDE_EVT32(genpd->device_count);

	rc = sde_power_resource_enable(&priv->phandle, priv->pclient, false);

	return rc;
}

static int _sde_kms_get_splash_data(struct sde_splash_data *data)
{
	int ret = 0;
	struct device_node *parent, *node;
	struct resource r;

	if (!data)
		return -EINVAL;

	parent = of_find_node_by_path("/reserved-memory");
	if (!parent) {
		SDE_ERROR("failed to find reserved-memory node\n");
		return -EINVAL;
	}

	node = of_find_node_by_name(parent, "cont_splash_region");
	if (!node) {
		SDE_ERROR("failed to find splash memory reservation\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, &r)) {
		SDE_ERROR("failed to find data for  splash memory\n");
		return -EINVAL;
	}

	data->splash_base = (unsigned long)r.start;
	data->splash_size = (r.end - r.start) + 1;

	pr_info("found continuous splash base address:%lx size:%x\n",
						data->splash_base,
						data->splash_size);
	return ret;
}

static int sde_kms_hw_init(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_rm *rm = NULL;
	int i, rc = -EINVAL;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		goto end;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev || !dev->platformdev) {
		SDE_ERROR("invalid device\n");
		goto end;
	}

	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("invalid private data\n");
		goto end;
	}

	sde_kms->mmio = msm_ioremap(dev->platformdev, "mdp_phys", "mdp_phys");
	if (IS_ERR(sde_kms->mmio)) {
		rc = PTR_ERR(sde_kms->mmio);
		SDE_ERROR("mdp register memory map failed: %d\n", rc);
		sde_kms->mmio = NULL;
		goto error;
	}
	DRM_INFO("mapped mdp address space @%pK\n", sde_kms->mmio);
	sde_kms->mmio_len = msm_iomap_size(dev->platformdev, "mdp_phys");

	rc = sde_dbg_reg_register_base(SDE_DBG_NAME, sde_kms->mmio,
			sde_kms->mmio_len);
	if (rc)
		SDE_ERROR("dbg base register kms failed: %d\n", rc);

	sde_kms->vbif[VBIF_RT] = msm_ioremap(dev->platformdev, "vbif_phys",
								"vbif_phys");
	if (IS_ERR(sde_kms->vbif[VBIF_RT])) {
		rc = PTR_ERR(sde_kms->vbif[VBIF_RT]);
		SDE_ERROR("vbif register memory map failed: %d\n", rc);
		sde_kms->vbif[VBIF_RT] = NULL;
		goto error;
	}
	sde_kms->vbif_len[VBIF_RT] = msm_iomap_size(dev->platformdev,
								"vbif_phys");
	rc = sde_dbg_reg_register_base("vbif_rt", sde_kms->vbif[VBIF_RT],
				sde_kms->vbif_len[VBIF_RT]);
	if (rc)
		SDE_ERROR("dbg base register vbif_rt failed: %d\n", rc);

	sde_kms->vbif[VBIF_NRT] = msm_ioremap(dev->platformdev, "vbif_nrt_phys",
								"vbif_nrt_phys");
	if (IS_ERR(sde_kms->vbif[VBIF_NRT])) {
		sde_kms->vbif[VBIF_NRT] = NULL;
		SDE_DEBUG("VBIF NRT is not defined");
	} else {
		sde_kms->vbif_len[VBIF_NRT] = msm_iomap_size(dev->platformdev,
							"vbif_nrt_phys");
		rc = sde_dbg_reg_register_base("vbif_nrt",
				sde_kms->vbif[VBIF_NRT],
				sde_kms->vbif_len[VBIF_NRT]);
		if (rc)
			SDE_ERROR("dbg base register vbif_nrt failed: %d\n",
					rc);
	}

	sde_kms->reg_dma = msm_ioremap(dev->platformdev, "regdma_phys",
								"regdma_phys");
	if (IS_ERR(sde_kms->reg_dma)) {
		sde_kms->reg_dma = NULL;
		SDE_DEBUG("REG_DMA is not defined");
	} else {
		sde_kms->reg_dma_len = msm_iomap_size(dev->platformdev,
								"regdma_phys");
		rc =  sde_dbg_reg_register_base("reg_dma",
				sde_kms->reg_dma,
				sde_kms->reg_dma_len);
		if (rc)
			SDE_ERROR("dbg base register reg_dma failed: %d\n",
					rc);
	}

	sde_kms->core_client = sde_power_client_create(&priv->phandle, "core");
	if (IS_ERR_OR_NULL(sde_kms->core_client)) {
		rc = PTR_ERR(sde_kms->core_client);
		if (!sde_kms->core_client)
			rc = -EINVAL;
		SDE_ERROR("sde power client create failed: %d\n", rc);
		sde_kms->core_client = NULL;
		goto error;
	}

	rc = _sde_kms_get_splash_data(&sde_kms->splash_data);
	if (rc)
		SDE_DEBUG("sde splash data fetch failed: %d\n", rc);

	for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++) {
		priv->phandle.data_bus_handle[i].ab_rt =
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA;
		priv->phandle.data_bus_handle[i].ib_rt =
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA;
	}

	rc = sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
		true);
	if (rc) {
		SDE_ERROR("resource enable failed: %d\n", rc);
		goto error;
	}

	for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
		sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT, i,
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
	 * splash memory is found.
	 */
	if (sde_kms->splash_data.splash_base)
		sde_rm_cont_splash_res_init(priv, &sde_kms->rm,
					&sde_kms->splash_data,
					sde_kms->catalog);

	sde_kms->splash_data.resource_handoff_pending = true;

	/* Initialize reg dma block which is a singleton */
	rc = sde_reg_dma_init(sde_kms->reg_dma, sde_kms->catalog,
			sde_kms->dev);
	if (rc) {
		SDE_ERROR("failed: reg dma init failed\n");
		goto power_error;
	}

	rc = _sde_kms_mmu_init(sde_kms);
	if (rc) {
		SDE_ERROR("sde_kms_mmu_init failed: %d\n", rc);
		goto power_error;
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

	sde_kms->iclient = msm_ion_client_create(dev->unique);
	if (IS_ERR(sde_kms->iclient)) {
		rc = PTR_ERR(sde_kms->iclient);
		SDE_DEBUG("msm_ion_client not available: %d\n", rc);
		sde_kms->iclient = NULL;
	}


	rc = sde_core_perf_init(&sde_kms->perf, dev, sde_kms->catalog,
			&priv->phandle, priv->pclient, "core_clk");
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

	/* initialize power domain if defined */
	if (of_find_property(dev->dev->of_node, "#power-domain-cells", NULL)) {
		sde_kms->genpd.name = dev->unique;
		sde_kms->genpd.power_off = sde_kms_pd_disable;
		sde_kms->genpd.power_on = sde_kms_pd_enable;

		rc = pm_genpd_init(&sde_kms->genpd, NULL, true);
		if (rc < 0) {
			SDE_ERROR("failed to init genpd provider %s: %d\n",
					sde_kms->genpd.name, rc);
			goto genpd_err;
		}

		rc = of_genpd_add_provider_simple(dev->dev->of_node,
				&sde_kms->genpd);
		if (rc < 0) {
			SDE_ERROR("failed to add genpd provider %s: %d\n",
					sde_kms->genpd.name, rc);
			pm_genpd_remove(&sde_kms->genpd);
			goto genpd_err;
		}

		sde_kms->genpd_init = true;
		SDE_DEBUG("added genpd provider %s\n", sde_kms->genpd.name);
	}

	if (sde_kms->splash_data.cont_splash_en) {
		SDE_DEBUG("Skipping MDP Resources disable\n");
	} else {
		for (i = 0; i < SDE_POWER_HANDLE_DBUS_ID_MAX; i++)
			sde_power_data_bus_set_quota(&priv->phandle,
				sde_kms->core_client,
				SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT, i,
				SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA);

		sde_power_resource_enable(&priv->phandle,
						sde_kms->core_client, false);
	}
	return 0;

genpd_err:
drm_obj_init_err:
	sde_core_perf_destroy(&sde_kms->perf);
hw_intr_init_err:
perf_err:
power_error:
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);
error:
	_sde_kms_hw_destroy(sde_kms, dev->platformdev);
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
