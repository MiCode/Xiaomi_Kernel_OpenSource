#ifndef __MI_MEMORY_SYSFS_H__
#define __MI_MEMORY_SYSFS_H__

#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "dram_info.h"
#include "ufs_info.h"
#include "ufs.h"
#include "ufshcd.h"

#define MV_NAME				"mv"
#define	MI_MEMORY_MODULE 	"mi_memory_module"
#define	MI_MEMORY_DEVICE 	"mi_memory_device"
#define	MI_MEMORY_CLASS 	"mi_memory"

#define	MEMORYDEV_MAJOR 	0
#define	MEMORYDEV_MINOR 	1

/* Raw data of DDR manufacturer id(MR5) */
#define HWINFO_DDRID_SAMSUNG 	0x01
#define HWINFO_DDRID_SKHYNIX 	0x06
#define HWINFO_DDRID_ELPIDA		0x03
#define HWINFO_DDRID_MICRON		0xFF
#define HWINFO_DDRID_NANYA		0x05
#define HWINFO_DDRID_INTEL		0x0E
#define HWINFO_DDRID_CXMT		0x13

#define UFS_VENDOR_MICRON      0x12C
#define UFS_VENDOR_SAMSUNG     0x1CE
#define UFS_VENDOR_SKHYNIX     0x1AD

struct memory_info
{
	struct class *mem_class;
	struct device *mem_dev;
	int major;
	struct delayed_work meminfo_init_delay_wq;
	struct ufs_info_t *ufs_info;
	struct dram_info_t *ddr_info;
	int retry_times;
};

extern const struct attribute_group ufs_sysfs_group;
extern const struct attribute_group dram_sysfs_group;
#endif
