/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __VPU_ALGO_H__
#define __VPU_ALGO_H__

#include <linux/slab.h>
#include "apusys_device.h"
#include "vpu_mem.h"
#include "vpu_ioctl.h"

enum {
	VPU_PROP_RESERVED,
	VPU_NUM_PROPS
};

struct vpu_device;
struct vpu_algo_list;

extern const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES];

int vpu_init_algo(void);

/* vpu_algo.c */
struct __vpu_algo *vpu_alg_alloc(struct vpu_algo_list *al);
void vpu_alg_free(struct __vpu_algo *alg);

/* vpu_hw.c */
int vpu_init_dev_algo(struct platform_device *pdev, struct vpu_device *vd);
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *vd);
int vpu_hw_alg_init(struct vpu_algo_list *al, struct __vpu_algo *alg);

struct __vpu_algo {
	struct vpu_algo a;
	struct vpu_iova prog;   /* preloaded and dynamic loaded algo */
	struct vpu_iova iram;   /* preloaded algo iram */
	bool builtin;           /* from vpu binary */
	struct kref ref;        /* reference count */
	struct list_head list;  /* link to device algo list */
	struct vpu_algo_list *al;
};

struct vpu_algo_ops {
	/* driver controls */
	int (*load)(struct vpu_algo_list *al, const char *name,
		struct __vpu_algo *alg, int prio);
	void (*unload)(struct vpu_algo_list *al, int prio);
	struct __vpu_algo * (*get)(struct vpu_algo_list *al,
		const char *name, struct __vpu_algo *alg);
	void (*put)(struct __vpu_algo *alg);
	void (*release)(struct kref *ref);
	/* device controls */
	int (*hw_init)(struct vpu_algo_list *al, struct __vpu_algo *alg);
	/* external firmware */
	int (*add)(struct vpu_algo_list *al, struct apusys_firmware_hnd *hnd);
	int (*del)(struct vpu_algo_list *al, struct apusys_firmware_hnd *hnd);
};

extern struct vpu_algo_ops vpu_normal_aops;
extern struct vpu_algo_ops vpu_prelaod_aops;

struct vpu_algo_list {
	char name[ALGO_NAMELEN];
	spinlock_t lock;
	struct list_head a;
	unsigned int cnt;    /* # of algorithms */
	struct vpu_device *vd;
	struct vpu_algo_ops *ops;
};

static inline void
vpu_algo_list_init(struct vpu_device *vd, struct vpu_algo_list *al,
	struct vpu_algo_ops *ops, const char *name) {
	if (!vd || !al)
		return;
	strncpy(al->name, name, (ALGO_NAMELEN - 1));
	spin_lock_init(&al->lock);
	INIT_LIST_HEAD(&al->a);
	al->cnt = 0;
	al->vd = vd;
	al->ops = ops;
}

#endif
