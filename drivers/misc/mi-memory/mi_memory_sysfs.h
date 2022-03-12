#ifndef __MI_MEMORY_SYSFS_H__
#define __MI_MEMORY_SYSFS_H__

#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "../../scsi/ufs/ufs.h"
#include "../../scsi/ufs/ufshcd.h"

#define	MI_MEMORY_MODULE 	"mi_memory_module"
#define	MI_MEMORY_DEVICE 	"mi_memory_device"
#define	MI_MEMORY_CLASS 	"mi_memory"

#define	MEMORYDEV_MAJOR 	0
#define	MEMORYDEV_MINOR 	1

struct memory_info
{
	struct class *mem_class;
	struct device *mem_dev;
	int major;
};

extern const struct attribute_group ufshcd_sysfs_group;
extern const struct attribute_group dram_sysfs_group;

struct ufs_hba *get_ufs_hba_data(void);

#endif
