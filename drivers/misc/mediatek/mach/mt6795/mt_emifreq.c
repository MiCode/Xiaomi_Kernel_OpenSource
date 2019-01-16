/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/jiffies.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_freqhopping.h"
#include "mach/mt_emifreq.h"
#include "mach/upmu_common.h"

/***************************
* debug message
****************************/
#define dprintk(fmt, args...)                                           \
do {                                                                    \
    if (mt_emifreq_debug) {                                             \
        xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", fmt, ##args);   \
    }                                                                   \
} while(0)

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_emifreq_early_suspend_handler =
{
    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
    .suspend = NULL,
    .resume  = NULL,
};
#endif

/**************************
* Global variable
***************************/
static bool mt_emifreq_debug = false;
static bool mt_emifreq_pause = true;

/******************************
* Extern Function Declaration
*******************************/


/******************************
* show current EMI DFS stauts
*******************************/
static int mt_emifreq_state_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    if (!mt_emifreq_pause)
        p += sprintf(p, "EMI DFS enabled\n");
    else
        p += sprintf(p, "EMI DFS disabled\n");

    len = p - buf;
    return len;
}

/****************************************
* set EMI DFS stauts by sysfs interface
*****************************************/
static ssize_t mt_emifreq_state_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int enabled = 0;

    if (sscanf(buffer, "%d", &enabled) == 1)
    {
        if (enabled == 1)
        {
            mt_emifreq_pause = false;
        }
        else if (enabled == 0)
        {
            mt_emifreq_pause = true;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "bad argument!! argument should be \"1\" or \"0\"\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "bad argument!! argument should be \"1\" or \"0\"\n");
    }

    return count;
}

/***************************
* show current debug status
****************************/
static int mt_emifreq_debug_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    if (mt_emifreq_debug)
        p += sprintf(p, "emifreq debug enabled\n");
    else
        p += sprintf(p, "emifreq debug disabled\n");

    len = p - buf;
    return len;
}

/***********************
* enable debug message
************************/
static ssize_t mt_emifreq_debug_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int debug = 0;

    if (sscanf(buffer, "%d", &debug) == 1)
    {
        if (debug == 0) 
        {
            mt_emifreq_debug = 0;
            return count;
        }
        else if (debug == 1)
        {
            mt_emifreq_debug = 1;
            return count;
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
    }

    return -EINVAL;
}


/*********************************
* early suspend callback function
**********************************/
void mt_emifreq_early_suspend(struct early_suspend *h)
{
    if(mt_emifreq_pause == false)
    {
        mt_h2l_dvfs_mempll();
        xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "mt_emifreq_early_suspend\n");
    }
}

/*******************************
* late resume callback function
********************************/
void mt_emifreq_late_resume(struct early_suspend *h)
{
    if(mt_emifreq_pause == false)
    {
        mt_l2h_dvfs_mempll();
        xlog_printk(ANDROID_LOG_INFO, "Power/EMI_DFS", "mt_emifreq_late_resume\n");
    }
}

/**********************************
* mediatek emifreq initialization
***********************************/
static int __init mt_emifreq_init(void)
{
    struct proc_dir_entry *mt_entry = NULL;
    struct proc_dir_entry *mt_emifreq_dir = NULL;
	
    #ifdef CONFIG_HAS_EARLYSUSPEND
    mt_emifreq_early_suspend_handler.suspend = mt_emifreq_early_suspend;
    mt_emifreq_early_suspend_handler.resume = mt_emifreq_late_resume;
    register_early_suspend(&mt_emifreq_early_suspend_handler);
    #endif

        mt_emifreq_dir = proc_mkdir("emifreq", NULL);
        if (!mt_emifreq_dir)
        {
            pr_err("[%s]: mkdir /proc/emifreq failed\n", __FUNCTION__);
        }
        else
        {
            mt_entry = create_proc_entry("emifreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_emifreq_dir);
            if (mt_entry)
            {
                mt_entry->read_proc = mt_emifreq_debug_read;
                mt_entry->write_proc = mt_emifreq_debug_write;
            }

            mt_entry = create_proc_entry("emifreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_emifreq_dir);
            if (mt_entry)
            {
                mt_entry->read_proc = mt_emifreq_state_read;
                mt_entry->write_proc = mt_emifreq_state_write;
            }
        }
		
    return 0;
}

static void __exit mt_emifreq_exit(void)
{

}

module_init(mt_emifreq_init);
module_exit(mt_emifreq_exit);
MODULE_DESCRIPTION("MediaTek EMI Frequency Scaling driver");
MODULE_LICENSE("GPL");