/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_IOCTL_H__
#define __PCIE_IOCTL_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define islower(c) ((c) >= 'a' && (c) <= 'z')
#define toupper(c) \
(((c) >= 'a' && (c) <= 'z') ? ((char)('A'+((c)-'a'))) : (c))
#define isxdigit(c) \
	(isdigit(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define cp2_test_addr1	0x500000
#define cp2_test_addr2	0x20000
#define cp2_test_addr3	0x50000
#define cp2_test_addr4	0x30000
#define MAGIC_VALUE	0x12345678

struct pcicmd {
	unsigned int cmd;
	unsigned int seq;
	unsigned long addr1;
	unsigned long addr2;
	unsigned long addr3;
	unsigned long addr4;
	unsigned int arg[10];
};

struct tlv {
	unsigned short t;
	unsigned short l;
	unsigned char v[0];
};

struct arg_t {
	unsigned int id;
	char *name;
	unsigned long def;
};

struct char_drv_info {
	int major;
	struct cdev testcdev;
	struct class *myclass;
	struct device *mydev;
	struct wcn_pcie_info *pcie_dev_info;
};

int ioctlcmd_deinit(struct wcn_pcie_info *bus);
int hexdump(char *name, char *buf, int len);
int lo_start(int mode);
int lo_stop(void);
int dbg_attach_bus(struct wcn_pcie_info *bus);
#endif
