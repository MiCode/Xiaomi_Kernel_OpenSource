/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_ROTATOR_DEBUG_H__
#define __SDE_ROTATOR_DEBUG_H__

#include <linux/types.h>
#include <linux/dcache.h>

#define SDE_ROT_DATA_LIMITER (-1)
#define SDE_ROT_EVTLOG_TOUT_DATA_LIMITER (NULL)

enum sde_rot_dbg_reg_dump_flag {
	SDE_ROT_DBG_DUMP_IN_LOG = BIT(0),
	SDE_ROT_DBG_DUMP_IN_MEM = BIT(1),
};

enum sde_rot_dbg_evtlog_flag {
	SDE_ROT_EVTLOG_DEFAULT = BIT(0),
	SDE_ROT_EVTLOG_IOMMU = BIT(1),
	SDE_ROT_EVTLOG_DBG = BIT(6),
	SDE_ROT_EVTLOG_ALL = BIT(7)
};

#define SDEROT_EVTLOG(...) sde_rot_evtlog(__func__, __LINE__, \
		SDE_ROT_EVTLOG_DEFAULT, ##__VA_ARGS__, SDE_ROT_DATA_LIMITER)

#define SDEROT_EVTLOG_TOUT_HANDLER(...)	\
	sde_rot_evtlog_tout_handler(false, __func__, ##__VA_ARGS__, \
		SDE_ROT_EVTLOG_TOUT_DATA_LIMITER)

void sde_rot_evtlog(const char *name, int line, int flag, ...);
void sde_rot_dump_panic(void);
void sde_rot_evtlog_tout_handler(bool queue, const char *name, ...);

struct sde_rotator_device;

struct sde_rotator_debug_base {
	char name[80];
	void __iomem *base;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	struct sde_rot_mgr *mgr;
	struct mutex buflock;
};

#if defined(CONFIG_DEBUG_FS)
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev);

void sde_rotator_destroy_debugfs(struct dentry *debugfs);
#else
static inline
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev)
{
	return NULL;
}

static inline
void sde_rotator_destroy_debugfs(struct dentry *debugfs)
{
}
#endif
#endif /* __SDE_ROTATOR_DEBUG_H__ */
