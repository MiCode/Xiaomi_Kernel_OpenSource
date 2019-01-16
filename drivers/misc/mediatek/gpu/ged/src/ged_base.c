/*
 * (C) Copyright 2010
 * MediaTek <www.MediaTek.com>
 *
 * MTK GPU Extension Device
 *
 */

#include "ged_base.h"
#include <asm/page.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#endif

#include <asm/uaccess.h>

unsigned long ged_copy_to_user(void __user *pvTo, const void *pvFrom, unsigned long ulBytes)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
    if (access_ok(VERIFY_WRITE, pvTo, ulBytes))
    {
    	return __copy_to_user(pvTo, pvFrom, ulBytes);
    }
    return ulBytes;
#else
    return copy_to_user(pvTo, pvFrom, ulBytes);
#endif
}

unsigned long ged_copy_from_user(void *pvTo, const void __user *pvFrom, unsigned long ulBytes)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
    if (access_ok(VERIFY_READ, pvFrom, ulBytes))
    {
	    return __copy_from_user(pvTo, pvFrom, ulBytes);
    }
    return ulBytes;
#else
    return copy_from_user(pvTo, pvFrom, ulBytes);
#endif
}

void* ged_alloc(int i32Size)
{
    void *pvBuf;

    if (i32Size <= PAGE_SIZE)
    {
        pvBuf = kmalloc(i32Size, GFP_KERNEL);
    }
    else
    {
        pvBuf = vmalloc(i32Size);
    }

    return pvBuf;
}

void ged_free(void* pvBuf, int i32Size)
{
    if (pvBuf)
    {
        if (i32Size <= PAGE_SIZE)
        {
            kfree(pvBuf);
        }
        else
        {
            vfree(pvBuf);
        }
    }
}

long ged_get_pid(void)
{
    if (in_interrupt())
    {
        return 0xffffffffL;
    }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    return (long)current->pgrp;
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    return (long)task_tgid_nr(current);
#else
    return (long)current->tgid;
#endif
#endif
}

