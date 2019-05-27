/*
 *  driver interface to finger print sensor  for
 *	Copyright (c) 2015  ChipSailing Technology.
 *	Copyright (C) 2019 XiaoMi, Inc.
 *	All rights reserved.
***********************************************************/
#include <linux/buffer_head.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/device-mapper.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/key.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/vmalloc.h>


#include <asm/setup.h>


#define BAT_ID_LENGTH 20


static char  bat_id[BAT_ID_LENGTH];

char* Get_BatID(void)
{
    return bat_id;
}
EXPORT_SYMBOL(Get_BatID);

static int __init setup_batid(char *line)
{
	strlcpy(bat_id, line, sizeof(bat_id));
	printk(KERN_ERR "bat_id: %s\n", bat_id);
	return 1;
}

__setup("androidboot.battery=", setup_batid);

