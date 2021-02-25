/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_KMS_H__
#define __SDE_KMS_H__

#include <linux/msm_ion.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_mmu.h"
#include "msm_gem.h"
#include "sde_dbg.h"
#include "sde_hw_catalog.h"
#include "sde_hw_ctl.h"
#include "sde_hw_lm.h"
#include "sde_hw_pingpong.h"
#include "sde_hw_interrupts.h"
#include "sde_hw_wb.h"
#include "sde_hw_top.h"
#include "sde_hw_uidle.h"
#include "sde_rm.h"
#include "sde_power_handle.h"
#include "sde_irq.h"
#include "sde_core_perf.h"

#define DRMID(x) ((x) ? (x)->base.id : -1)

/**
 * SDE_DEBUG - macro for kms/plane/crtc/encoder/connector logs
 * @fmt: Pointer to format string
 */
#define SDE_DEBUG(fmt, ...)                                                \
	do {                                                               \
		if (unlikely(drm_debug & DRM_UT_KMS))                      \
			DRM_DEBUG(fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_debug(fmt, ##__VA_ARGS__);                      \
	} while (0)

/**
 * SDE_INFO - macro for kms/plane/crtc/encoder/connector logs
 * @fmt: Pointer to format string
 */
#define SDE_INFO(fmt, ...)                                                \
	do {                                                               \
		if (unlikely(drm_debug & DRM_UT_KMS))                      \
			DRM_INFO(fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_info(fmt, ##__VA_ARGS__);                      \
	} while (0)

/**
 * SDE_DEBUG_DRIVER - macro for hardware driver logging
 * @fmt: Pointer to format string
 */
#define SDE_DEBUG_DRIVER(fmt, ...)                                         \
	do {                                                               \
		if (unlikely(drm_debug & DRM_UT_DRIVER))                   \
			DRM_ERROR(fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_debug(fmt, ##__VA_ARGS__);                      \
	} while (0)

#define SDE_ERROR(fmt, ...) pr_err("[sde error]" fmt, ##__VA_ARGS__)

#define POPULATE_RECT(rect, a, b, c, d, Q16_flag) \
	do {						\
		(rect)->x = (Q16_flag) ? (a) >> 16 : (a);    \
		(rect)->y = (Q16_flag) ? (b) >> 16 : (b);    \
		(rect)->w = (Q16_flag) ? (c) >> 16 : (c);    \
		(rect)->h = (Q16_flag) ? (d) >> 16 : (d);    \
	} while (0)

#define CHECK_LAYER_BOUNDS(offset, size, max_size) \
	(((size) > (max_size)) || ((offset) > ((max_size) - (size))))

/**
 * ktime_compare_safe - compare two ktime structures
 *	This macro is similar to the standard ktime_compare() function, but
 *	attempts to also handle ktime overflows.
 * @A: First ktime value
 * @B: Second ktime value
 * Returns: -1 if A < B, 0 if A == B, 1 if A > B
 */
#define ktime_compare_safe(A, B) \
	ktime_compare(ktime_sub((A), (B)), ktime_set(0, 0))

#define SDE_NAME_SIZE  12

#define DEFAULT_FPS 60

/* timeout in frames waiting for frame done */
#define SDE_FRAME_DONE_TIMEOUT	60

/* max active secure client counts allowed */
#define MAX_ALLOWED_SECURE_CLIENT_CNT	1

/* max active crtc when secure client is active */
#define MAX_ALLOWED_CRTC_CNT_DURING_SECURE	1

/* max virtual encoders per secure crtc */
#define MAX_ALLOWED_ENCODER_CNT_PER_SECURE_CRTC	1

/* defines the operations required for secure state transition */
#define SDE_KMS_OPS_SECURE_STATE_CHANGE		BIT(0)
#define SDE_KMS_OPS_WAIT_FOR_TX_DONE		BIT(1)
#define SDE_KMS_OPS_CLEANUP_PLANE_FB		BIT(2)
#define SDE_KMS_OPS_PREPARE_PLANE_FB		BIT(3)

/* ESD status check interval in miliseconds */
#define STATUS_CHECK_INTERVAL_MS 5000

/**
 * enum sde_kms_smmu_state:	smmu state
 * @ATTACHED:	 all the context banks are attached.
 * @DETACHED:	 all the context banks are detached.
 * @DETACHED_SEC:	 secure context bank is detached.
 * @ATTACH_ALL_REQ:	 transient state of attaching context banks.
 * @DETACH_ALL_REQ:	 transient state of detaching context banks.
 * @DETACH_SEC_REQ:	 tranisent state of secure context bank is detached
 * @ATTACH_SEC_REQ:	 transient state of attaching secure context bank.
 */
enum sde_kms_smmu_state {
	ATTACHED = 0,
	DETACHED,
	DETACHED_SEC,
	ATTACH_ALL_REQ,
	DETACH_ALL_REQ,
	DETACH_SEC_REQ,
	ATTACH_SEC_REQ,
};

/**
 * enum sde_kms_smmu_state_transition_type: state transition type
 * @NONE: no pending state transitions
 * @PRE_COMMIT: state transitions should be done before processing the commit
 * @POST_COMMIT: state transitions to be done after processing the commit.
 */
enum sde_kms_smmu_state_transition_type {
	NONE,
	PRE_COMMIT,
	POST_COMMIT
};

/**
 * enum sde_kms_sui_misr_state: state request for enabling/disabling MISR
 * @NONE: no request
 * @ENABLE_SUI_MISR_REQ: request to enable sui MISR
 * @DISABLE_SUI_MISR_REQ: request to disable sui MISR
 */
enum sde_kms_sui_misr_state {
	SUI_MISR_NONE,
	SUI_MISR_ENABLE_REQ,
	SUI_MISR_DISABLE_REQ
};

/*
 * @FRAME_DONE_WAIT_DEFAULT:	waits for frame N pp_done interrupt before
 *                              triggering frame N+1.
 * @FRAME_DONE_WAIT_SERIALIZE:	serialize pp_done and ctl_start irq for frame
 *                              N without next frame trigger wait.
 * @FRAME_DONE_WAIT_POSTED_START: Do not wait for pp_done interrupt for any
 *                              frame. Wait will trigger only for error case.
 */
enum frame_trigger_mode_type {
	FRAME_DONE_WAIT_DEFAULT,
	FRAME_DONE_WAIT_SERIALIZE,
	FRAME_DONE_WAIT_POSTED_START,
};

/**
 * struct sde_kms_smmu_state_data: stores the smmu state and transition type
 * @state: current state of smmu context banks
 * @prev_state: previous state of smmu context banks
 * @secure_level: secure level cached from crtc
 * @prev_secure_level: previous secure level
 * @transition_type: transition request type
 * @transition_error: whether there is error while transitioning the state
 */
struct sde_kms_smmu_state_data {
	uint32_t state;
	uint32_t prev_state;
	uint32_t secure_level;
	uint32_t prev_secure_level;
	uint32_t transition_type;
	uint32_t transition_error;
	uint32_t sui_misr_state;
};

/*
 * struct sde_irq_callback - IRQ callback handlers
 * @list: list to callback
 * @func: intr handler
 * @arg: argument for the handler
 */
struct sde_irq_callback {
	struct list_head list;
	void (*func)(void *arg, int irq_idx);
	void *arg;
};

/**
 * struct sde_irq: IRQ structure contains callback registration info
 * @total_irq:    total number of irq_idx obtained from HW interrupts mapping
 * @irq_cb_tbl:   array of IRQ callbacks setting
 * @enable_counts array of IRQ enable counts
 * @cb_lock:      callback lock
 * @debugfs_file: debugfs file for irq statistics
 */
struct sde_irq {
	u32 total_irqs;
	struct list_head *irq_cb_tbl;
	atomic_t *enable_counts;
	atomic_t *irq_counts;
	spinlock_t cb_lock;
	struct dentry *debugfs_file;
};

/**
 * struct sde_kms_frame_event_cb_data : info of drm objects of a frame event
 * @crtc:       pointer to drm crtc object registered for frame event
 * @connector:  pointer to drm connector which is source of frame event
 */
struct sde_kms_frame_event_cb_data {
	struct drm_crtc *crtc;
	struct drm_connector *connector;
};

struct sde_kms {
	struct msm_kms base;
	struct drm_device *dev;
	uint32_t core_rev;
	struct sde_mdss_cfg *catalog;

	struct generic_pm_domain genpd;
	bool genpd_init;

	struct msm_gem_address_space *aspace[MSM_SMMU_DOMAIN_MAX];
	struct sde_power_event *power_event;

	/* directory entry for debugfs */
	struct dentry *debugfs_vbif;

	/* io/register spaces: */
	void __iomem *mmio, *vbif[VBIF_MAX], *reg_dma, *sid;
	unsigned long mmio_len, vbif_len[VBIF_MAX], reg_dma_len, sid_len;

	struct regulator *vdd;
	struct regulator *mmagic;
	struct regulator *venus;

	struct sde_irq_controller irq_controller;

	struct sde_hw_intr *hw_intr;
	struct sde_irq irq_obj;
	int irq_num;	/* mdss irq number */
	bool irq_enabled;

	struct sde_core_perf perf;

	/* saved atomic state during system suspend */
	struct drm_atomic_state *suspend_state;
	bool suspend_block;

	struct sde_rm rm;
	bool rm_init;
	struct sde_splash_data splash_data;
	struct sde_hw_vbif *hw_vbif[VBIF_MAX];
	struct sde_hw_mdp *hw_mdp;
	struct sde_hw_uidle *hw_uidle;
	struct sde_hw_sid *hw_sid;
	int dsi_display_count;
	void **dsi_displays;
	int wb_display_count;
	void **wb_displays;
	int dp_display_count;
	void **dp_displays;
	int dp_stream_count;

	bool has_danger_ctrl;

	struct sde_kms_smmu_state_data smmu_state;
	atomic_t detach_sec_cb;
	atomic_t detach_all_cb;
	struct mutex secure_transition_lock;

	bool first_kickoff;
	bool qdss_enabled;
	bool pm_suspend_clk_dump;

	cpumask_t irq_cpu_mask;
	atomic_t irq_vote_count;
	struct dev_pm_qos_request pm_qos_irq_req[NR_CPUS];
	struct irq_affinity_notify affinity_notify;

	struct sde_vm *vm;
};

struct vsync_info {
	u32 frame_count;
	u32 line_count;
};

#define to_sde_kms(x) container_of(x, struct sde_kms, base)

/**
 * sde_is_custom_client - whether or not to enable non-standard customizations
 *
 * Return: Whether or not the 'sdeclient' module parameter was set on boot up
 */
bool sde_is_custom_client(void);

/**
 * sde_kms_get_hw_version - get the hw revision - client is expected to
 *    enable the power resources before making this call
 * @dev: Pointer to drm device
 */
static inline u32 sde_kms_get_hw_version(struct drm_device *dev)
{
	struct sde_kms *sde_kms;

	if (!ddev_to_msm_kms(dev))
		return 0;

	sde_kms = to_sde_kms(ddev_to_msm_kms(dev));

	return readl_relaxed(sde_kms->mmio + 0x0);
}

/**
 * sde_kms_power_resource_is_enabled - whether or not power resource is enabled
 * @dev: Pointer to drm device
 * Return: true if power resource is enabled; false otherwise
 */
static inline bool sde_kms_power_resource_is_enabled(struct drm_device *dev)
{
	if (!dev)
		return false;

	return pm_runtime_enabled(dev->dev);
}

/**
 * sde_kms_is_suspend_state - whether or not the system is pm suspended
 * @dev: Pointer to drm device
 * Return: Suspend status
 */
static inline bool sde_kms_is_suspend_state(struct drm_device *dev)
{
	if (!ddev_to_msm_kms(dev))
		return false;

	return to_sde_kms(ddev_to_msm_kms(dev))->suspend_state != NULL;
}

/**
 * sde_kms_is_suspend_blocked - whether or not commits are blocked due to pm
 *				suspend status
 * @dev: Pointer to drm device
 * Return: True if commits should be rejected due to pm suspend
 */
static inline bool sde_kms_is_suspend_blocked(struct drm_device *dev)
{
	if (!sde_kms_is_suspend_state(dev))
		return false;

	return to_sde_kms(ddev_to_msm_kms(dev))->suspend_block;
}

/**
 * sde_kms_is_secure_session_inprogress - to indicate if secure-session is in
 * currently in-progress based on the current smmu_state
 *
 * @sde_kms: Pointer to sde_kms
 *
 * return: true if secure-session is in progress; false otherwise
 */
static inline bool sde_kms_is_secure_session_inprogress(struct sde_kms *sde_kms)
{
	bool ret = false;

	if (!sde_kms)
		return false;

	mutex_lock(&sde_kms->secure_transition_lock);
	if (((sde_kms->catalog->sui_ns_allowed) &&
		(sde_kms->smmu_state.secure_level == SDE_DRM_SEC_ONLY) &&
			((sde_kms->smmu_state.state == DETACHED_SEC) ||
				(sde_kms->smmu_state.state == DETACH_SEC_REQ) ||
				(sde_kms->smmu_state.state == ATTACH_SEC_REQ)))
		|| (((sde_kms->smmu_state.state == DETACHED) ||
			(sde_kms->smmu_state.state == DETACH_ALL_REQ) ||
			(sde_kms->smmu_state.state == ATTACH_ALL_REQ))))
		ret = true;
	mutex_unlock(&sde_kms->secure_transition_lock);

	return ret;
}

/**
 * sde_kms_is_vbif_operation_allowed - resticts the VBIF programming
 * during secure-ui, if the sec_ui_misr feature is enabled
 *
 * @sde_kms: Pointer to sde_kms
 *
 * return: false if secure-session is in progress; true otherwise
 */
static inline bool sde_kms_is_vbif_operation_allowed(struct sde_kms *sde_kms)
{
	if (!sde_kms)
		return false;

	if (!sde_kms->catalog->sui_misr_supported)
		return true;

	return !sde_kms_is_secure_session_inprogress(sde_kms);
}

/**
 * sde_kms_is_cp_operation_allowed - resticts the CP programming
 * during secure-ui, if the non-secure context banks are detached
 *
 * @sde_kms: Pointer to sde_kms
 */
static inline bool sde_kms_is_cp_operation_allowed(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->catalog)
		return false;

	if (sde_kms->catalog->sui_ns_allowed)
		return true;

	return !sde_kms_is_secure_session_inprogress(sde_kms);
}

/**
 * Debugfs functions - extra helper functions for debugfs support
 *
 * Main debugfs documentation is located at,
 *
 * Documentation/filesystems/debugfs.txt
 *
 * @sde_debugfs_get_root: Get root dentry for SDE_KMS's debugfs node
 */

/**
 * sde_debugfs_get_root - Return root directory entry for KMS's debugfs
 *
 * The return value should be passed as the 'parent' argument to subsequent
 * debugfs create calls.
 *
 * @sde_kms: Pointer to SDE's KMS structure
 *
 * Return: dentry pointer for SDE's debugfs location
 */
void *sde_debugfs_get_root(struct sde_kms *sde_kms);

/**
 * SDE info management functions
 * These functions/definitions allow for building up a 'sde_info' structure
 * containing one or more "key=value\n" entries.
 */
#define SDE_KMS_INFO_MAX_SIZE	4096

/**
 * struct sde_kms_info - connector information structure container
 * @data: Array of information character data
 * @len: Current length of information data
 * @staged_len: Temporary data buffer length, commit to
 *              len using sde_kms_info_stop
 * @start: Whether or not a partial data entry was just started
 */
struct sde_kms_info {
	char data[SDE_KMS_INFO_MAX_SIZE];
	uint32_t len;
	uint32_t staged_len;
	bool start;
};

/**
 * SDE_KMS_INFO_DATA - Macro for accessing sde_kms_info data bytes
 * @S: Pointer to sde_kms_info structure
 * Returns: Pointer to byte data
 */
#define SDE_KMS_INFO_DATA(S)    ((S) ? ((struct sde_kms_info *)(S))->data \
							: NULL)

/**
 * SDE_KMS_INFO_DATALEN - Macro for accessing sde_kms_info data length
 *			it adds an extra character length to count null.
 * @S: Pointer to sde_kms_info structure
 * Returns: Size of available byte data
 */
#define SDE_KMS_INFO_DATALEN(S) ((S) ? ((struct sde_kms_info *)(S))->len + 1 \
							: 0)

/**
 * sde_kms_info_reset - reset sde_kms_info structure
 * @info: Pointer to sde_kms_info structure
 */
void sde_kms_info_reset(struct sde_kms_info *info);

/**
 * sde_kms_info_add_keyint - add integer value to 'sde_kms_info'
 * @info: Pointer to sde_kms_info structure
 * @key: Pointer to key string
 * @value: Signed 64-bit integer value
 */
void sde_kms_info_add_keyint(struct sde_kms_info *info,
		const char *key,
		int64_t value);

/**
 * sde_kms_info_add_keystr - add string value to 'sde_kms_info'
 * @info: Pointer to sde_kms_info structure
 * @key: Pointer to key string
 * @value: Pointer to string value
 */
void sde_kms_info_add_keystr(struct sde_kms_info *info,
		const char *key,
		const char *value);

/**
 * sde_kms_info_start - begin adding key to 'sde_kms_info'
 * Usage:
 *      sde_kms_info_start(key)
 *      sde_kms_info_append(val_1)
 *      ...
 *      sde_kms_info_append(val_n)
 *      sde_kms_info_stop
 * @info: Pointer to sde_kms_info structure
 * @key: Pointer to key string
 */
void sde_kms_info_start(struct sde_kms_info *info,
		const char *key);

/**
 * sde_kms_info_append - append value string to 'sde_kms_info'
 * Usage:
 *      sde_kms_info_start(key)
 *      sde_kms_info_append(val_1)
 *      ...
 *      sde_kms_info_append(val_n)
 *      sde_kms_info_stop
 * @info: Pointer to sde_kms_info structure
 * @str: Pointer to partial value string
 */
void sde_kms_info_append(struct sde_kms_info *info,
		const char *str);

/**
 * sde_kms_info_append_format - append format code string to 'sde_kms_info'
 * Usage:
 *      sde_kms_info_start(key)
 *      sde_kms_info_append_format(fourcc, modifier)
 *      ...
 *      sde_kms_info_stop
 * @info: Pointer to sde_kms_info structure
 * @pixel_format: FOURCC format code
 * @modifier: 64-bit drm format modifier
 */
void sde_kms_info_append_format(struct sde_kms_info *info,
		uint32_t pixel_format,
		uint64_t modifier);

/**
 * sde_kms_info_stop - finish adding key to 'sde_kms_info'
 * Usage:
 *      sde_kms_info_start(key)
 *      sde_kms_info_append(val_1)
 *      ...
 *      sde_kms_info_append(val_n)
 *      sde_kms_info_stop
 * @info: Pointer to sde_kms_info structure
 */
void sde_kms_info_stop(struct sde_kms_info *info);

/**
 * sde_kms_rect_intersect - intersect two rectangles
 * @r1: first rectangle
 * @r2: scissor rectangle
 * @result: result rectangle, all 0's on no intersection found
 */
void sde_kms_rect_intersect(const struct sde_rect *r1,
		const struct sde_rect *r2,
		struct sde_rect *result);

/**
 * sde_kms_rect_merge_rectangles - merge a rectangle list into one rect
 * @rois: pointer to the list of rois
 * @result: output rectangle, all 0 on error
 */
void sde_kms_rect_merge_rectangles(const struct msm_roi_list *rois,
		struct sde_rect *result);

/**
 * sde_kms_rect_is_equal - compares two rects
 * @r1: rect value to compare
 * @r2: rect value to compare
 *
 * Returns 1 if the rects are same, 0 otherwise.
 */
static inline bool sde_kms_rect_is_equal(struct sde_rect *r1,
		struct sde_rect *r2)
{
	if ((!r1 && r2) || (r1 && !r2))
		return false;

	if (!r1 && !r2)
		return true;

	return r1->x == r2->x && r1->y == r2->y && r1->w == r2->w &&
			r1->h == r2->h;
}

/**
 * sde_kms_rect_is_null - returns true if the width or height of a rect is 0
 * @rect: rectangle to check for zero size
 * @Return: True if width or height of rectangle is 0
 */
static inline bool sde_kms_rect_is_null(const struct sde_rect *r)
{
	if (!r)
		return true;

	return (!r->w || !r->h);
}

/*
 * sde_in_trusted_vm - checks the executing VM
 * return: true, if the device driver is executing in the trusted VM
 *         false, if the device driver is executing in the primary VM
 */
static inline bool sde_in_trusted_vm(const struct sde_kms *sde_kms)
{
	if (sde_kms && sde_kms->catalog)
		return sde_kms->catalog->trusted_vm_env;

	return false;
}

/**
 * Vblank enable/disable functions
 */
int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
void sde_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);

/**
 * smmu attach/detach functions
 * @sde_kms: poiner to sde_kms structure
 * @secure_only: if true only secure contexts are attached/detached, else
 * all contexts are attached/detached/
 */
int sde_kms_mmu_attach(struct sde_kms *sde_kms, bool secure_only);
int sde_kms_mmu_detach(struct sde_kms *sde_kms, bool secure_only);

/**
 * sde_kms_timeline_status - provides current timeline status
 * @dev: Pointer to drm device
 */
void sde_kms_timeline_status(struct drm_device *dev);

/**
 * sde_kms_handle_recovery - handler function for FIFO overflow issue
 * @encoder: pointer to drm encoder structure
 * return: 0 on success; error code otherwise
 */
int sde_kms_handle_recovery(struct drm_encoder *encoder);

/**
 * sde_kms_cpu_vote_for_irq() - API to keep pm_qos latency vote on cpu
 * where mdss_irq is scheduled
 * @sde_kms: pointer to sde_kms structure
 * @enable: true if enable request, false otherwise.
 */
void sde_kms_cpu_vote_for_irq(struct sde_kms *sde_kms, bool enable);

/**
 * sde_kms_get_io_resources() - reads associated register range
 * @kms: pointer to sde_kms structure
 * @io_res: pointer to msm_io_res struct to populate the ranges
 * Return: error code.
 */
int sde_kms_get_io_resources(struct sde_kms *kms, struct msm_io_res *io_res);

/**
 * sde_kms_vm_trusted_resource_init - reserve/initialize the HW/SW resources
 * @sde_kms: poiner to sde_kms structure
 * @state: current update atomic commit state
 * return: 0 on success; error code otherwise
 */
int sde_kms_vm_trusted_resource_init(struct sde_kms *sde_kms,
		struct drm_atomic_state *state);

/**
 * sde_kms_vm_trusted_resource_deinit - release the HW/SW resources
 * @sde_kms: poiner to sde_kms structure
 */
void sde_kms_vm_trusted_resource_deinit(struct sde_kms *sde_kms);

/**
 * sde_kms_vm_trusted_post_commit - function to prepare the VM after the
 *				    last commit before releasing the HW
 *				    resources from trusted VM
 * @sde_kms: pointer to sde_kms
 * @state: current frames atomic commit state
 */
int sde_kms_vm_trusted_post_commit(struct sde_kms *sde_kms,
	struct drm_atomic_state *state);

/**
 * sde_kms_vm_primary_post_commit - function to prepare the VM after the
 *				    last commit before assign the HW
 *				    resources from primary VM
 * @sde_kms: pointer to sde_kms
 * @state: current frames atomic commit state
 */
int sde_kms_vm_primary_post_commit(struct sde_kms *sde_kms,
	struct drm_atomic_state *state);

/**
 * sde_kms_vm_trusted_prepare_commit - function to prepare the VM before the
 *				       the first commit after the accepting
 *				       the HW resources in trusted VM.
 * @sde_kms: pointer to sde_kms
 * @state: current frame's atomic commit state
 */
int sde_kms_vm_trusted_prepare_commit(struct sde_kms *sde_kms,
					   struct drm_atomic_state *state);
/**
 * sde_kms_vm_primary_prepare_commit - function to prepare the VM before the
 *				       the first commit after the reclaming
 *				       the HW resources in trusted VM.
 * @sde_kms: pointer to sde_kms
 * @state: current frame's atomic commit state
 */
int sde_kms_vm_primary_prepare_commit(struct sde_kms *sde_kms,
					   struct drm_atomic_state *state);
#endif /* __sde_kms_H__ */
