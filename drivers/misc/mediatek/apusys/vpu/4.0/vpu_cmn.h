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

#include "apusys_device.h"
#include "vpu_drv.h"
#include "vpu_mem.h"

#include <aee.h>

// TODO: move power related defines elsewhere
enum VpuPowerOnType {
	/* power on previously by setPower */
	VPT_PRE_ON		= 1,
	/* power on by enque */
	VPT_ENQUE_ON	= 2,
	/* power on by enque, but want to immediately off(when exception) */
	VPT_IMT_OFF		= 3,
};

// TODO: remove vpu_user
struct vpu_user {
	int deprecated;
};

#ifdef CONFIG_MTK_AEE_FEATURE
#define vpu_aee(key, format, args...) \
	do { \
		pr_info(format, ##args); \
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
#define vpu_aee(key, format, args...)
#define vpu_aee_warn(key, format, args...)
#endif

/*
 * lock order
 * 1. driver lock
 * 2. device lock
 **/

// driver data
struct vpu_driver {
	// unsigned long vpu_syscfg_base;
	// unsigned long vpu_adlctrl_base;
	// unsigned long vpu_vcorecfg_base;
	// unsigned long smi_cmn_base;
	int cores;                 /* # of VPU cores */
	void *bin_va;
	unsigned long bin_pa;
	unsigned int bin_size;

	struct ion_client *ion;    /* ion client */
	struct m4u_client_t *m4u;  /* m4u client */

	struct class *class;

	/* memory, allocated by vpu_init_hw() */
	struct vpu_mem *share_data;

	/* shared algo */
	uint64_t mva_algo;

	/* list of devices */
	struct list_head devs;
	struct mutex lock;

	/* debugfs entry */
	struct dentry *droot;

	/* device references */
	struct kref ref;
};

enum vpu_state {
	VS_UNKNOWN = 0,
	VS_DISALBED,   // disabled by e-fuse
	VS_DOWN,       // power down
	VS_BOOT,       // booting
	VS_IDLE,       // power on, idle
	VS_CMD_ALG,
	VS_CMD_D2D,
	VS_REMOVING
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
	void __iomem *reg_base;  // IPU_BASE

	/* character device */
	dev_t devt;
	struct cdev cdev;
	struct device *ddev;

	/* debugfs entry */
	struct dentry *droot;

	/* algorithm */
	struct list_head algo;
	struct __vpu_algo *algo_curr;  // current active algorithm
	unsigned int algo_cnt;       // # of algorithms in vpu binary

	/* irq */
	unsigned int irq_num;
	unsigned int irq_level;

	/* command */
	struct mutex cmd_lock;
	wait_queue_head_t cmd_wait;
	bool cmd_done;               // command done

	/* memory, allocated by vpu_init_hw() */
	struct vpu_mem *work_buf;     /* working buffer */
	struct vpu_mem *kernel_lib;   /* execution kernel library */

	/* scatter-gather tables */
	struct sg_table sg_reset_vector;
	struct sg_table sg_main_program;
	struct sg_table sg_algo_binary_data;
	struct sg_table sg_iram_data;
	uint64_t mva_iram;

	/* trace */
	bool ftrace_avail;
};


extern struct vpu_driver *vpu_drv;

/***** vpu_hw.h *****/
/**
 * vpu_init_hw - init the procedure related to hw,
 *               include irq register and enque thread
 * @core:   core index of vpu_device.
 * @device: the pointer of vpu_device.
 */
int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *dev);
int vpu_init_drv_hw(void);


/**
 * vpu_uninit_hw - close resources related to hw module
 */
int vpu_exit_dev_hw(struct platform_device *pdev, struct vpu_device *dev);
int vpu_exit_drv_hw(void);

/**
 * vpu_get_name_of_algo - get the algo's name by its id
 * @core:	core index of vpu device
 * @id          the serial id
 * @name:       return the algo's name
 */
int vpu_get_name_of_algo(struct vpu_device *dev, int id, char **name);

/**
 * vpu_hw_load_algo - call vpu program to load algo, by specifying the
 *                    start address
 * @core:	core index of device.
 * @algo:       the pointer to struct algo, which has right binary-data info.
 */
int vpu_hw_load_algo(struct vpu_device *dev, struct __vpu_algo *algo);

/**
 * vpu_hw_get_algo_info - prepare a memory for vpu program to dump algo info
 * @core:	core index of device.
 * @algo:       the pointer to memory block for algo dump.
 *
 * Query properties value and port info from vpu algo(kernel).
 * Should create enough of memory
 * for properties dump, and assign the pointer to vpu_props_t's ptr.
 */
int vpu_hw_get_algo_info(struct vpu_device *dev, struct __vpu_algo *algo);

/**
 * vpu_get_entry_of_algo - get the address and length from binary data
 * @core:       core index of vpu device
 * @name:       the algo's name
 * @id          return the serial id
 * @mva:        return the mva of algo binary
 * @length:     return the length of algo binary
 */
int vpu_get_entry_of_algo(struct vpu_device *dev, char *name, int *id,
	unsigned int *mva, int *length);

int vpu_alloc_request(struct vpu_request **rreq);
int vpu_free_request(struct vpu_request *req);

int vpu_alloc_algo(struct __vpu_algo **ralgo);
int vpu_free_algo(struct __vpu_algo *algo);

int vpu_execute(struct vpu_device *dev, struct vpu_request *req);

#endif

