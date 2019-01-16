/*
 * core.c
 *
 * VIA CBP driver for Linux
 *
 * Copyright (C) 2011 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/platform_device.h>

static struct kobject *viatel_kobj;

struct kobject *viatel_kobject_add(const char *name)
{
    struct kobject *kobj = NULL;
    if(viatel_kobj){
        kobj = kobject_create_and_add(name, viatel_kobj);
    }
    
    return kobj;
}

static int __init viatel_core_init(void)
{
    int ret = 0;
    viatel_kobj = kobject_create_and_add("viatel", NULL);
    if (!viatel_kobj){
        ret = -ENOMEM;
        goto err_create_kobj;
    }
err_create_kobj:
    return ret;
}

arch_initcall(viatel_core_init);
