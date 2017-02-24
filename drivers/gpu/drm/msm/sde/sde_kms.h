/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_mmu.h"
#include "msm_gem.h"
#include "sde_dbg.h"
#include "sde_hw_catalog.h"
#include "sde_hw_ctl.h"
#include "sde_hw_lm.h"
#include "sde_hw_interrupts.h"
#include "sde_hw_wb.h"
#include "sde_hw_top.h"
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
			drm_ut_debug_printk(__func__, fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_debug(fmt, ##__VA_ARGS__);                      \
	} while (0)

/**
 * SDE_DEBUG_DRIVER - macro for hardware driver logging
 * @fmt: Pointer to format string
 */
#define SDE_DEBUG_DRIVER(fmt, ...)                                         \
	do {                                                               \
		if (unlikely(drm_debug & DRM_UT_DRIVER))                   \
			drm_ut_debug_printk(__func__, fmt, ##__VA_ARGS__); \
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

struct sde_kms {
	struct msm_kms base;
	struct drm_device *dev;
	int core_rev;
	struct sde_mdss_cfg *catalog;

	struct msm_gem_address_space *aspace[MSM_SMMU_DOMAIN_MAX];
	struct sde_power_client *core_client;

	/* directory entry for debugfs */
	void *debugfs_root;
	struct dentry *debugfs_debug;
	struct dentry *debugfs_danger;
	struct dentry *debugfs_vbif;

	/* io/register spaces: */
	void __iomem *mmio, *vbif[VBIF_MAX];

	struct regulator *vdd;
	struct regulator *mmagic;
	struct regulator *venus;

	struct sde_irq_controller irq_controller;

	struct sde_hw_intr *hw_intr;
	struct sde_irq irq_obj;

	struct sde_core_perf perf;

	struct sde_rm rm;
	bool rm_init;

	struct sde_hw_vbif *hw_vbif[VBIF_MAX];
	struct sde_hw_mdp *hw_mdp;
	int dsi_display_count;
	void **dsi_displays;
	int wb_display_count;
	void **wb_displays;
	bool has_danger_ctrl;
	void **hdmi_displays;
	int hdmi_display_count;
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
 * Debugfs functions - extra helper functions for debugfs support
 *
 * Main debugfs documentation is located at,
 *
 * Documentation/filesystems/debugfs.txt
 *
 * @sde_debugfs_setup_regset32: Initialize data for sde_debugfs_create_regset32
 * @sde_debugfs_create_regset32: Create 32-bit register dump file
 * @sde_debugfs_get_root: Get root dentry for SDE_KMS's debugfs node
 */

/**
 * Companion structure for sde_debugfs_create_regset32. Do not initialize the
 * members of this structure explicitly; use sde_debugfs_setup_regset32 instead.
 */
struct sde_debugfs_regset32 {
	uint32_t offset;
	uint32_t blk_len;
	struct sde_kms *sde_kms;
};

/**
 * sde_debugfs_setup_regset32 - Initialize register block definition for debugfs
 * This function is meant to initialize sde_debugfs_regset32 structures for use
 * with sde_debugfs_create_regset32.
 * @regset: opaque register definition structure
 * @offset: sub-block offset
 * @length: sub-block length, in bytes
 * @sde_kms: pointer to sde kms structure
 */
void sde_debugfs_setup_regset32(struct sde_debugfs_regset32 *regset,
		uint32_t offset, uint32_t length, struct sde_kms *sde_kms);

/**
 * sde_debugfs_create_regset32 - Create register read back file for debugfs
 *
 * This function is almost identical to the standard debugfs_create_regset32()
 * function, with the main difference being that a list of register
 * names/offsets do not need to be provided. The 'read' function simply outputs
 * sequential register values over a specified range.
 *
 * Similar to the related debugfs_create_regset32 API, the structure pointed to
 * by regset needs to persist for the lifetime of the created file. The calling
 * code is responsible for initialization/management of this structure.
 *
 * The structure pointed to by regset is meant to be opaque. Please use
 * sde_debugfs_setup_regset32 to initialize it.
 *
 * @name:   File name within debugfs
 * @mode:   File mode within debugfs
 * @parent: Parent directory entry within debugfs, can be NULL
 * @regset: Pointer to persistent register block definition
 *
 * Return: dentry pointer for newly created file, use either debugfs_remove()
 *         or debugfs_remove_recursive() (on a parent directory) to remove the
 *         file
 */
void *sde_debugfs_create_regset32(const char *name, umode_t mode,
		void *parent, struct sde_debugfs_regset32 *regset);

/**
 * sde_debugfs_get_root - Return root directory entry for SDE's debugfs
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
#define SDE_KMS_INFO_DATA(S)    ((S) ? ((struct sde_kms_info *)(S))->data : 0)

/**
 * SDE_KMS_INFO_DATALEN - Macro for accessing sde_kms_info data length
 * @S: Pointer to sde_kms_info structure
 * Returns: Size of available byte data
 */
#define SDE_KMS_INFO_DATALEN(S) ((S) ? ((struct sde_kms_info *)(S))->len : 0)

/**
 * sde_kms_info_reset - reset sde_kms_info structure
 * @info: Pointer to sde_kms_info structure
 */
void sde_kms_info_reset(struct sde_kms_info *info);

/**
 * sde_kms_info_add_keyint - add integer value to 'sde_kms_info'
 * @info: Pointer to sde_kms_info structure
 * @key: Pointer to key string
 * @value: Signed 32-bit integer value
 */
void sde_kms_info_add_keyint(struct sde_kms_info *info,
		const char *key,
		int32_t value);

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
 * Vblank enable/disable functions
 */
int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
void sde_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);

#endif /* __sde_kms_H__ */
