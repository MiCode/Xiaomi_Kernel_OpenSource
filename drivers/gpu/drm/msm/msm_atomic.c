/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2014 Red Hat
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

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gem.h"
#include "sde_trace.h"

struct msm_commit {
	struct drm_device *dev;
	struct drm_atomic_state *state;
	uint32_t fence;
	struct msm_fence_cb fence_cb;
	uint32_t crtc_mask;
	struct kthread_work commit_work;
};

/* block until specified crtcs are no longer pending update, and
 * atomically mark them as pending update
 */
static int start_atomic(struct msm_drm_private *priv, uint32_t crtc_mask)
{
	int ret;

	spin_lock(&priv->pending_crtcs_event.lock);
	ret = wait_event_interruptible_locked(priv->pending_crtcs_event,
			!(priv->pending_crtcs & crtc_mask));
	if (ret == 0) {
		DBG("start: %08x", crtc_mask);
		priv->pending_crtcs |= crtc_mask;
	}
	spin_unlock(&priv->pending_crtcs_event.lock);

	return ret;
}

/* clear specified crtcs (no longer pending update)
 */
static void end_atomic(struct msm_drm_private *priv, uint32_t crtc_mask)
{
	spin_lock(&priv->pending_crtcs_event.lock);
	DBG("end: %08x", crtc_mask);
	priv->pending_crtcs &= ~crtc_mask;
	wake_up_all_locked(&priv->pending_crtcs_event);
	spin_unlock(&priv->pending_crtcs_event.lock);
}

static void commit_destroy(struct msm_commit *commit)
{
	end_atomic(commit->dev->dev_private, commit->crtc_mask);
	kfree(commit);
}

static void msm_atomic_wait_for_commit_done(
		struct drm_device *dev,
		struct drm_atomic_state *old_state,
		int modeset_flags)
{
	struct drm_crtc *crtc;
	struct msm_drm_private *priv = old_state->dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int ncrtcs = old_state->dev->mode_config.num_crtc;
	int i;

	for (i = 0; i < ncrtcs; i++) {
		int private_flags;

		crtc = old_state->crtcs[i];

		if (!crtc || !crtc->state || !crtc->state->enable)
			continue;

		/* If specified, only wait if requested flag is true */
		private_flags = crtc->state->adjusted_mode.private_flags;
		if (modeset_flags && !(modeset_flags & private_flags))
			continue;

		/* Legacy cursor ioctls are completely unsynced, and userspace
		 * relies on that (by doing tons of cursor updates). */
		if (old_state->legacy_cursor_update)
			continue;

		if (kms->funcs->wait_for_crtc_commit_done)
			kms->funcs->wait_for_crtc_commit_done(kms, crtc);
	}
}

static void
msm_disable_outputs(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	SDE_ATRACE_BEGIN("msm_disable");
	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_crtc_state *old_crtc_state;
		unsigned int crtc_idx;

		/*
		 * Shut down everything that's in the changeset and currently
		 * still on. So need to check the old, saved state.
		 */
		if (!old_conn_state->crtc)
			continue;

		crtc_idx = drm_crtc_index(old_conn_state->crtc);
		old_crtc_state = old_state->crtc_states[crtc_idx];

		if (!old_crtc_state->active ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state))
			continue;

		encoder = old_conn_state->best_encoder;

		/* We shouldn't get this far if we didn't previously have
		 * an encoder.. but WARN_ON() rather than explode.
		 */
		if (WARN_ON(!encoder))
			continue;

		if (msm_is_mode_seamless(
				&connector->encoder->crtc->state->mode))
			continue;

		funcs = encoder->helper_private;

		DRM_DEBUG_ATOMIC("disabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call disable hooks twice.
		 */
		drm_bridge_disable(encoder->bridge);

		/* Right function depends upon target state. */
		if (connector->state->crtc && funcs->prepare)
			funcs->prepare(encoder);
		else if (funcs->disable)
			funcs->disable(encoder);
		else
			funcs->dpms(encoder, DRM_MODE_DPMS_OFF);

		drm_bridge_post_disable(encoder->bridge);
	}

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* Shut down everything that needs a full modeset. */
		if (!drm_atomic_crtc_needs_modeset(crtc->state))
			continue;

		if (!old_crtc_state->active)
			continue;

		if (msm_is_mode_seamless(&crtc->state->mode))
			continue;

		funcs = crtc->helper_private;

		DRM_DEBUG_ATOMIC("disabling [CRTC:%d]\n",
				 crtc->base.id);

		/* Right function depends upon target state. */
		if (crtc->state->enable && funcs->prepare)
			funcs->prepare(crtc);
		else if (funcs->disable)
			funcs->disable(crtc);
		else
			funcs->dpms(crtc, DRM_MODE_DPMS_OFF);
	}
	SDE_ATRACE_END("msm_disable");
}

static void
msm_crtc_set_mode(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!crtc->state->mode_changed)
			continue;

		funcs = crtc->helper_private;

		if (crtc->state->enable && funcs->mode_set_nofb) {
			DRM_DEBUG_ATOMIC("modeset on [CRTC:%d]\n",
					 crtc->base.id);

			funcs->mode_set_nofb(crtc);
		}
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_crtc_state *new_crtc_state;
		struct drm_encoder *encoder;
		struct drm_display_mode *mode, *adjusted_mode;

		if (!connector->state->best_encoder)
			continue;

		encoder = connector->state->best_encoder;
		funcs = encoder->helper_private;
		new_crtc_state = connector->state->crtc->state;
		mode = &new_crtc_state->mode;
		adjusted_mode = &new_crtc_state->adjusted_mode;

		if (!new_crtc_state->mode_changed)
			continue;

		DRM_DEBUG_ATOMIC("modeset on [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call mode_set hooks twice.
		 */
		if (funcs->mode_set)
			funcs->mode_set(encoder, mode, adjusted_mode);

		drm_bridge_mode_set(encoder->bridge, mode, adjusted_mode);
	}
}

/**
 * msm_atomic_helper_commit_modeset_disables - modeset commit to disable outputs
 * @dev: DRM device
 * @old_state: atomic state object with old state structures
 *
 * This function shuts down all the outputs that need to be shut down and
 * prepares them (if required) with the new mode.
 *
 * For compatibility with legacy crtc helpers this should be called before
 * drm_atomic_helper_commit_planes(), which is what the default commit function
 * does. But drivers with different needs can group the modeset commits together
 * and do the plane commits at the end. This is useful for drivers doing runtime
 * PM since planes updates then only happen when the CRTC is actually enabled.
 */
static void msm_atomic_helper_commit_modeset_disables(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	msm_disable_outputs(dev, old_state);

	drm_atomic_helper_update_legacy_modeset_state(dev, old_state);

	msm_crtc_set_mode(dev, old_state);
}

/**
 * msm_atomic_helper_commit_modeset_enables - modeset commit to enable outputs
 * @dev: DRM device
 * @old_state: atomic state object with old state structures
 *
 * This function enables all the outputs with the new configuration which had to
 * be turned off for the update.
 *
 * For compatibility with legacy crtc helpers this should be called after
 * drm_atomic_helper_commit_planes(), which is what the default commit function
 * does. But drivers with different needs can group the modeset commits together
 * and do the plane commits at the end. This is useful for drivers doing runtime
 * PM since planes updates then only happen when the CRTC is actually enabled.
 */
static void msm_atomic_helper_commit_modeset_enables(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int bridge_enable_count = 0;
	int i;

	SDE_ATRACE_BEGIN("msm_enable");
	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* Need to filter out CRTCs where only planes change. */
		if (!drm_atomic_crtc_needs_modeset(crtc->state))
			continue;

		if (!crtc->state->active)
			continue;

		if (msm_is_mode_seamless(&crtc->state->mode))
			continue;

		funcs = crtc->helper_private;

		if (crtc->state->enable) {
			DRM_DEBUG_ATOMIC("enabling [CRTC:%d]\n",
					 crtc->base.id);

			if (funcs->enable)
				funcs->enable(crtc);
			else
				funcs->commit(crtc);
		}
	}

	/* ensure bridge/encoder updates happen on same vblank */
	msm_atomic_wait_for_commit_done(dev, old_state,
			MSM_MODE_FLAG_VBLANK_PRE_MODESET);

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;

		if (!connector->state->best_encoder)
			continue;

		if (!connector->state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(
				    connector->state->crtc->state))
			continue;

		encoder = connector->state->best_encoder;
		funcs = encoder->helper_private;

		DRM_DEBUG_ATOMIC("enabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call enable hooks twice.
		 */
		drm_bridge_pre_enable(encoder->bridge);
		++bridge_enable_count;

		if (funcs->enable)
			funcs->enable(encoder);
		else
			funcs->commit(encoder);
	}

	if (kms->funcs->commit) {
		DRM_DEBUG_ATOMIC("triggering commit\n");
		kms->funcs->commit(kms, old_state);
	}

	/* If no bridges were pre_enabled, skip iterating over them again */
	if (bridge_enable_count == 0) {
		SDE_ATRACE_END("msm_enable");
		return;
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		struct drm_encoder *encoder;

		if (!connector->state->best_encoder)
			continue;

		if (!connector->state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(
				    connector->state->crtc->state))
			continue;

		encoder = connector->state->best_encoder;

		DRM_DEBUG_ATOMIC("bridge enable enabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		drm_bridge_enable(encoder->bridge);
	}
	SDE_ATRACE_END("msm_enable");
}

/* The (potentially) asynchronous part of the commit.  At this point
 * nothing can fail short of armageddon.
 */
static void complete_commit(struct msm_commit *commit)
{
	struct drm_atomic_state *state = commit->state;
	struct drm_device *dev = state->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;

	kms->funcs->prepare_commit(kms, state);

	msm_atomic_helper_commit_modeset_disables(dev, state);

	drm_atomic_helper_commit_planes(dev, state, false);

	msm_atomic_helper_commit_modeset_enables(dev, state);

	/* NOTE: _wait_for_vblanks() only waits for vblank on
	 * enabled CRTCs.  So we end up faulting when disabling
	 * due to (potentially) unref'ing the outgoing fb's
	 * before the vblank when the disable has latched.
	 *
	 * But if it did wait on disabled (or newly disabled)
	 * CRTCs, that would be racy (ie. we could have missed
	 * the irq.  We need some way to poll for pipe shut
	 * down.  Or just live with occasionally hitting the
	 * timeout in the CRTC disable path (which really should
	 * not be critical path)
	 */

	msm_atomic_wait_for_commit_done(dev, state, 0);

	drm_atomic_helper_cleanup_planes(dev, state);

	kms->funcs->complete_commit(kms, state);

	drm_atomic_state_free(state);

	commit_destroy(commit);
}

static int msm_atomic_commit_dispatch(struct drm_device *dev,
		struct drm_atomic_state *state, struct msm_commit *commit);

static void fence_cb(struct msm_fence_cb *cb)
{
	struct msm_commit *commit =
			container_of(cb, struct msm_commit, fence_cb);
	int ret = -EINVAL;

	ret = msm_atomic_commit_dispatch(commit->dev, commit->state, commit);
	if (ret) {
		DRM_ERROR("%s: atomic commit failed\n", __func__);
		drm_atomic_state_free(commit->state);
		commit_destroy(commit);
	}
}

static void _msm_drm_commit_work_cb(struct kthread_work *work)
{
	struct msm_commit *commit =  NULL;

	if (!work) {
		DRM_ERROR("%s: Invalid commit work data!\n", __func__);
		return;
	}

	commit = container_of(work, struct msm_commit, commit_work);

	SDE_ATRACE_BEGIN("complete_commit");
	complete_commit(commit);
	SDE_ATRACE_END("complete_commit");
}

static struct msm_commit *commit_init(struct drm_atomic_state *state)
{
	struct msm_commit *commit = kzalloc(sizeof(*commit), GFP_KERNEL);

	if (!commit) {
		DRM_ERROR("invalid commit\n");
		return ERR_PTR(-ENOMEM);
	}

	commit->dev = state->dev;
	commit->state = state;

	/* TODO we might need a way to indicate to run the cb on a
	 * different wq so wait_for_vblanks() doesn't block retiring
	 * bo's..
	 */
	INIT_FENCE_CB(&commit->fence_cb, fence_cb);
	init_kthread_work(&commit->commit_work, _msm_drm_commit_work_cb);

	return commit;
}

static void commit_set_fence(struct msm_commit *commit,
		struct drm_framebuffer *fb)
{
	struct drm_gem_object *obj = msm_framebuffer_bo(fb, 0);
	commit->fence = max(commit->fence,
			msm_gem_fence(to_msm_bo(obj), MSM_PREP_READ));
}

/* Start display thread function */
static int msm_atomic_commit_dispatch(struct drm_device *dev,
		struct drm_atomic_state *state, struct msm_commit *commit)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int ret = -EINVAL, i = 0, j = 0;

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < priv->num_crtcs; j++) {
			if (priv->disp_thread[j].crtc_id ==
						crtc->base.id) {
				if (priv->disp_thread[j].thread) {
					queue_kthread_work(
						&priv->disp_thread[j].worker,
							&commit->commit_work);
					/* only return zero if work is
					 * queued successfully.
					 */
					ret = 0;
				} else {
					DRM_ERROR(" Error for crtc_id: %d\n",
						priv->disp_thread[j].crtc_id);
				}
				break;
			}
		}
		/*
		 * TODO: handle cases where there will be more than
		 * one crtc per commit cycle. Remove this check then.
		 * Current assumption is there will be only one crtc
		 * per commit cycle.
		 */
		if (j < priv->num_crtcs)
			break;
	}

	return ret;
}

/**
 * drm_atomic_helper_commit - commit validated state object
 * @dev: DRM device
 * @state: the driver state object
 * @async: asynchronous commit
 *
 * This function commits with drm_atomic_helper_check() pre-validated state
 * object. This can still fail when e.g. the framebuffer reservation fails.
 *
 * RETURNS
 * Zero for success or -errno.
 */
int msm_atomic_commit(struct drm_device *dev,
		struct drm_atomic_state *state, bool async)
{
	struct msm_drm_private *priv = dev->dev_private;
	int nplanes = dev->mode_config.num_total_plane;
	int ncrtcs = dev->mode_config.num_crtc;
	ktime_t timeout;
	struct msm_commit *commit;
	int i, ret;

	if (!priv || priv->shutdown_in_progress) {
		DRM_ERROR("priv is null or shutdown is in-progress\n");
		return -EINVAL;
	}

	SDE_ATRACE_BEGIN("atomic_commit");
	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret) {
		SDE_ATRACE_END("atomic_commit");
		return ret;
	}

	commit = commit_init(state);
	if (IS_ERR_OR_NULL(commit)) {
		ret = PTR_ERR(commit);
		DRM_ERROR("commit_init failed: %d\n", ret);
		goto error;
	}

	/*
	 * Figure out what crtcs we have:
	 */
	for (i = 0; i < ncrtcs; i++) {
		struct drm_crtc *crtc = state->crtcs[i];
		if (!crtc)
			continue;
		commit->crtc_mask |= (1 << drm_crtc_index(crtc));
	}

	/*
	 * Figure out what fence to wait for:
	 */
	for (i = 0; i < nplanes; i++) {
		struct drm_plane *plane = state->planes[i];
		struct drm_plane_state *new_state = state->plane_states[i];

		if (!plane)
			continue;

		if ((plane->state->fb != new_state->fb) && new_state->fb)
			commit_set_fence(commit, new_state->fb);
	}

	/*
	 * Wait for pending updates on any of the same crtc's and then
	 * mark our set of crtc's as busy:
	 */
	ret = start_atomic(dev->dev_private, commit->crtc_mask);
	if (ret) {
		DRM_ERROR("start_atomic failed: %d\n", ret);
		commit_destroy(commit);
		goto error;
	}

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */

	drm_atomic_helper_swap_state(dev, state);

	/*
	 * Provide the driver a chance to prepare for output fences. This is
	 * done after the point of no return, but before asynchronous commits
	 * are dispatched to work queues, so that the fence preparation is
	 * finished before the .atomic_commit returns.
	 */
	if (priv && priv->kms && priv->kms->funcs &&
			priv->kms->funcs->prepare_fence)
		priv->kms->funcs->prepare_fence(priv->kms, state);

	/*
	 * Everything below can be run asynchronously without the need to grab
	 * any modeset locks at all under one conditions: It must be guaranteed
	 * that the asynchronous work has either been cancelled (if the driver
	 * supports it, which at least requires that the framebuffers get
	 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
	 * before the new state gets committed on the software side with
	 * drm_atomic_helper_swap_state().
	 *
	 * This scheme allows new atomic state updates to be prepared and
	 * checked in parallel to the asynchronous completion of the previous
	 * update. Which is important since compositors need to figure out the
	 * composition of the next frame right after having submitted the
	 * current layout.
	 */

	if (async) {
		msm_queue_fence_cb(dev, &commit->fence_cb, commit->fence);
		SDE_ATRACE_END("atomic_commit");
		return 0;
	}

	timeout = ktime_add_ms(ktime_get(), 1000);

	/* uninterruptible wait */
	msm_wait_fence(dev, commit->fence, &timeout, false);

	complete_commit(commit);

	SDE_ATRACE_END("atomic_commit");
	return 0;

error:
	drm_atomic_helper_cleanup_planes(dev, state);
	SDE_ATRACE_END("atomic_commit");
	return ret;
}
