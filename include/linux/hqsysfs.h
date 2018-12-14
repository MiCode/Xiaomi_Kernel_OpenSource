/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#ifndef __HQ_SYSFS_HEAD__
#define __HQ_SYSFS_HEAD__
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <linux/atomic.h>




#define MAX_HW_DEVICE_NAME (64)

enum hardware_id{
	HWID_NONE = 0x00,
	HWID_DDR = 0x10,
	HWID_EMMC,
	HWID_NAND,

	HWID_LCM = 0x20,
	HWID_SUB_LCM,
	HWID_BIAS_IC,
	HWID_CTP,

	HWID_MAIN_CAM = 0x30,
	HWID_MAIN_CAM_2,
	HWID_SUB_CAM,
	HWID_SUB_CAM_2,
	HWID_MAIN_LENS,
	HWID_MAIN_LENS_2,
	HWID_SUB_LENS,
	HWID_SUB_LENS_2,
	HWID_MAIN_OTP,
	HWID_MAIN_OTP_2,
	HWID_SUB_OTP,
	HWID_SUB_OTP_2,
	HWID_FLASHLIGHT,
	HWID_FLaSHLIGHT_2,

	HWID_GSENSOR = 0x70,
	HWID_ALSPS,
	HWID_GYRO,
	HWID_MSENSOR,
	HWID_IRDA,
	HWID_BAROMETER,
	HWID_PEDOMETER,
	HWID_HUMIDITY,

	HWID_PCBA = 0x80,

	HWID_BATERY = 0xA0,
	HWID_FUEL_GAUGE_IC,

	HWID_NFC = 0xC0,
	HWID_FP,
	HWID_TEE,

	HWID_USB_TYPE_C = 0xE0,


	HWID_SUMMARY = 0xF0,
	HWID_VER,
	HWID_END
};


struct hw_info{
	enum hardware_id hw_id;
	struct attribute attr;
	unsigned int hw_exist;

	char *hw_device_name;
};


#define __INFO(_id, _hw_type_name) {				\
		.hw_id = _id,				\
		.attr = {.name = __stringify(_hw_type_name),				\
		 		.mode = VERIFY_OCTAL_PERMISSIONS(S_IWUSR|S_IRUGO) },		\
		.hw_exist	= 0,						\
		.hw_device_name	= NULL,						\
	}


#define HW_INFO(_id, _hw_type_name) \
	struct hw_info hw_info_##_hw_type_name = __INFO(_id, _hw_type_name)



#define HUAQIN_CLASS_NAME       "huaqin"
#define HUAIN_INTERFACE_NAME	"interface"
#define HUAQIN_HWID_NAME        "hw_info"
#define HUAQIN_VERSION_FILE		"hw_info_ver"



int hq_regiser_hw_info(enum hardware_id id,char *device_name);
int hq_deregister_hw_info(enum hardware_id id,char *device_name);
int register_kboj_under_hqsysfs(struct kobject *kobj, struct kobj_type *ktype, const char *fmt, ...);


#endif
