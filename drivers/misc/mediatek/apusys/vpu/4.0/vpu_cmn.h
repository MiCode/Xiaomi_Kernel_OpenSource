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

#ifndef __VPU_CMN_H__
#define __VPU_CMN_H__

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>

#include "apusys_device.h"
#include "vpu_drv.h"
#include "vpu_mem.h"
#include "vpu_dump.h"

#include <aee.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#define vpu_aee_excp(vd, req, key, format, args...) \
	do { \
		pr_info(format, ##args); \
		vpu_dmp_create_locked(vd, req, format, ##args); \
		vpu_pwr_down_locked(vd); \
		aee_kernel_exception("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)

#define vpu_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define vpu_aee_excp(vd, req, key, format, args...)
#define vpu_aee_warn(key, format, args...)
#endif

/*
 * lock order
 * 1. driver lock
 * 2. device lock
 **/

struct vpu_met_hrt {
	spinlock_t lock;
	struct hrtimer t;
	struct kref ref;
	uint64_t latency;
};

// driver data
struct vpu_driver {
	void *bin_va;
	unsigned long bin_pa;
	unsigned int bin_size;

	/* power work queue */
	struct workqueue_struct *wq;

	/* iova settings */
	struct kref iova_ref;
	struct device *iova_dev;
	struct vpu_iova iova_algo;
	struct vpu_iova iova_share;

	/* shared */
	uint64_t mva_algo;
	uint64_t mva_share;

	/* list of devices */
	struct list_head devs;
	struct mutex lock;

	/* debugfs entry */
	struct dentry *droot;

	/* device references */
	struct kref ref;

	/* met */
	uint32_t ilog;
	uint32_t met;
	struct vpu_met_hrt met_hrt;
};

enum vpu_state {
	VS_UNKNOWN = 0,
	VS_DISALBED,   // disabled by e-fuse
	VS_DOWN,       // power down
	VS_UP,         // power up
	VS_BOOT,       // booting
	VS_IDLE,       // power on, idle
	VS_CMD_ALG,
	VS_CMD_D2D,
	VS_REMOVING
};

struct vpu_iomem {
	void __iomem *m;
	struct resource *res;
};

struct vpu_met_work {
	struct list_head list;
	spinlock_t lock;
	int pid;
	struct work_struct work;
};

#define VPU_MET_PM_MAX 8

struct vpu_met_pm {
	uint32_t val[VPU_MET_PM_MAX];
};

// device data
struct vpu_device {
	int id;
	char name[8];
	struct list_head list;   // link in vpu driver
	enum vpu_state state;
	struct mutex lock;
	struct apusys_device adev;
	struct device *dev;      // platform device
	struct rproc *rproc;

	/* iomem */
	struct vpu_iomem reg;
	struct vpu_iomem dmem;
	struct vpu_iomem imem;
	struct vpu_iomem dbg;

	/* power */
	struct kref pw_ref;
	struct delayed_work pw_off_work;
	wait_queue_head_t pw_wait;
	uint64_t pw_off_latency;   /* ms, 0 = always on */
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source pw_wake_lock;
#else
	struct wake_lock pw_wake_lock;
#endif

	/* iova settings */
	struct vpu_iova iova_reset;
	struct vpu_iova iova_main;
	struct vpu_iova iova_kernel;
	struct vpu_iova iova_iram;
	struct vpu_iova iova_work;

	/* character device */
	dev_t devt;
	struct cdev cdev;
	struct device *ddev;

	/* algorithm */
	spinlock_t	algo_lock;
	struct list_head algo;
	struct __vpu_algo *algo_curr;  /* current active algorithm */
	unsigned int algo_cnt;         /* # of algorithms in vpu binary */

	/* irq */
	unsigned int irq_num;

	/* command */
	struct mutex cmd_lock;
	wait_queue_head_t cmd_wait;
	bool cmd_done;
	uint64_t cmd_timeout;  /* ms */

	/* memory */
	uint64_t mva_iram;

	/* debug */
	struct dentry *droot;  /* debugfs entry */
	bool ftrace_avail;     /* trace */
	bool jtag_enabled;     /* jtag */
	struct vpu_dmp *dmp;   /* dump */

	/* MET */
	struct vpu_met_work met;
	struct vpu_met_pm pm;
};


extern struct vpu_driver *vpu_drv;

/***** vpu_hw.h *****/
/**
 * vpu_init_hw - init the procedure related to hw,
 *               include irq register and enque thread
 * @core:   core index of vpu_device.
 * @device: the pointer of vpu_device.
 */
int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *vd);
int vpu_init_drv_hw(void);
/**
 * vpu_is_disabled - return whether efuse enable/disable on vd->id
 * @vd: vpu device.
 */
bool vpu_is_disabled(struct vpu_device *vd);


/**
 * vpu_uninit_hw - close resources related to hw module
 */
int vpu_exit_dev_hw(struct platform_device *pdev, struct vpu_device *vd);
int vpu_exit_drv_hw(void);

/**
 * vpu_get_name_of_algo - get the algo's name by its id
 * @core:	core index of vpu device
 * @id          the serial id
 * @name:       return the algo's name
 */
int vpu_get_name_of_algo(struct vpu_device *vd, int id, char **name);

/**
 * vpu_hw_load_algo - call vpu program to load algo, by specifying the
 *                    start address
 * @core:	core index of device.
 * @algo:       the pointer to struct algo, which has right binary-data info.
 */
int vpu_hw_load_algo(struct vpu_device *vd, struct __vpu_algo *algo);

/**
 * vpu_hw_get_algo_info - prepare a memory for vpu program to dump algo info
 * @core:	core index of device.
 * @algo:       the pointer to memory block for algo dump.
 *
 * Query properties value and port info from vpu algo(kernel).
 * Should create enough of memory
 * for properties dump, and assign the pointer to vpu_props_t's ptr.
 */
int vpu_hw_get_algo_info(struct vpu_device *vd, struct __vpu_algo *algo);

/**
 * vpu_get_entry_of_algo - get the address and length from binary data
 * @core:       core index of vpu device
 * @name:       the algo's name
 * @id          return the serial id
 * @mva:        return the mva of algo binary
 * @length:     return the length of algo binary
 */
int vpu_get_entry_of_algo(struct vpu_device *vd, char *name, int *id,
	unsigned int *mva, int *length);

int vpu_alloc_request(struct vpu_request **rreq);
int vpu_free_request(struct vpu_request *req);

int vpu_alloc_algo(struct __vpu_algo **ralgo);
int vpu_free_algo(struct __vpu_algo *algo);

int vpu_execute(struct vpu_device *vd, struct vpu_request *req);

#endif

