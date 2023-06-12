#ifndef __MI_MEMORY_SYSFS_H__
#define __MI_MEMORY_SYSFS_H__

#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/err.h>

#include "../../scsi/ufs/ufs.h"
#include "../../scsi/ufs/ufs_quirks.h"

#define MEMORYTPYE_DIS		0

#define	MI_MEMORY_MODULE 	"mi_memory_module"
#define	MI_MEMORY_DEVICE 	"mi_memory_device"
#define	MI_MEMORY_CLASS 	"mi_memory"

#define	MEMORYDEV_MAJOR 	0
#define	MEMORYDEV_MINOR 	1

/* Raw data of DDR manufacturer id(MR5) */
#define HWINFO_DDRID_SAMSUNG 	0x01
#define HWINFO_DDRID_HYNIX 		0x06
#define HWINFO_DDRID_ELPIDA		0x03
#define HWINFO_DDRID_MICRON		0xFF
#define HWINFO_DDRID_NANYA		0x05
#define HWINFO_DDRID_INTEL		0x0E

#define UFS_VENDOR_HYNIX 		0x01ad

struct memory_info {
	struct class *mem_class;
	struct device *mem_dev;
	int major;
};

extern const struct attribute_group ufshcd_sysfs_group;
extern const struct attribute_group dram_sysfs_group;

uint8_t get_ddr_id(void);
u8 get_ddr_size(void);
u16 get_ufs_id(void);

#endif
