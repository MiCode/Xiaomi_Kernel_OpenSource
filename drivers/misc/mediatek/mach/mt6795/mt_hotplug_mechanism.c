/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*********************************
* include
**********************************/
#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/earlysuspend.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h> //seq_printf, single_open
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <mach/hotplug.h>
#include <mach/sync_write.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>



/*********************************
* macro
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
#define STATE_INIT                          0
#define STATE_ENTER_EARLY_SUSPEND           1
#define STATE_ENTER_LATE_RESUME             2
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

#define FORCE_CPU_OFF_DELAYED_WORK_TIME     3 //second
#define FORCE_CPU_OFF_WAKE_LOCK_TIME        5 //second



/*********************************
* glabal variable
**********************************/
static int g_enable = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
static int g_enable_cpu_rush_boost = 0;
static int g_prev_cpu_rush_boost_enable = 0;

static struct early_suspend mt_hotplug_mechanism_early_suspend_handler =
{
    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
    .suspend = NULL,
    .resume  = NULL,
};
static int g_cur_state = STATE_ENTER_LATE_RESUME;
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

static int g_enable_dynamic_cpu_hotplug_at_suspend = 0;
static int g_prev_dynamic_cpu_hotplug_enable = 0;
static int mt_hotplug_mechanism_probe(struct platform_device *pdev);
static int mt_hotplug_mechanism_suspend(struct platform_device *pdev, pm_message_t state);
static int mt_hotplug_mechanism_resume(struct platform_device *pdev);
static struct platform_driver mt_hotplug_mechanism_pdrv =
{
    .remove     = NULL,
    .shutdown   = NULL,
    .probe      = mt_hotplug_mechanism_probe,
    .suspend    = mt_hotplug_mechanism_suspend,
    .resume     = mt_hotplug_mechanism_resume,
    .driver     = {
        .name = "hotplug_mechanism",
    },
};

static int g_test0 = 0;
static int g_test1 = 0;
static int g_memory_debug = SPM_CA15_CPUTOP_PWR_CON;



/*********************************
* extern function
**********************************/
#ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
extern int hp_get_cpu_rush_boost_enable(void);
extern void hp_set_cpu_rush_boost_enable(int enable);
extern int hp_get_dynamic_cpu_hotplug_enable(void);
extern void hp_set_dynamic_cpu_hotplug_enable(int enable);
#endif



/*********************************
* early suspend callback function
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mt_hotplug_mechanism_early_suspend(struct early_suspend *h)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_early_suspend\n");

    if (!g_enable)
        goto early_suspend_end;
    
    if (!g_enable_cpu_rush_boost)
    {
    #ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
        g_prev_cpu_rush_boost_enable = hp_get_cpu_rush_boost_enable();
        hp_set_cpu_rush_boost_enable(0);
    #endif //#ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
    }
    
early_suspend_end:
    g_cur_state = STATE_ENTER_EARLY_SUSPEND;

    return;
}
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND



/*******************************
* late resume callback function
********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mt_hotplug_mechanism_late_resume(struct early_suspend *h)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_late_resume\n");

    if (!g_enable)
        goto late_resume_end;
    
    if (!g_enable_cpu_rush_boost)
    {
    #ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
        hp_set_cpu_rush_boost_enable(g_prev_cpu_rush_boost_enable);
    #endif //#ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
    }
    
late_resume_end:
    g_cur_state = STATE_ENTER_LATE_RESUME;

    return;
}
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND



/*******************************
* probe callback function
********************************/
static int mt_hotplug_mechanism_probe(struct platform_device *pdev)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_probe\n");
    
    return 0;
}



/*******************************
* suspend callback function
********************************/
static int mt_hotplug_mechanism_suspend(struct platform_device *pdev, pm_message_t state)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_suspend\n");
    
    if (!g_enable)
        return 0;
    
    if (!g_enable_dynamic_cpu_hotplug_at_suspend)
    {
    #ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
        g_prev_dynamic_cpu_hotplug_enable = hp_get_dynamic_cpu_hotplug_enable();
        hp_set_dynamic_cpu_hotplug_enable(0);
    #endif //#ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
    }
    
    return 0;
}



/*******************************
* resume callback function
********************************/
static int mt_hotplug_mechanism_resume(struct platform_device *pdev)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_resume\n");
    
    if (!g_enable)
        return 0;
    
    if (!g_enable_dynamic_cpu_hotplug_at_suspend)
    {
    #ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
        hp_set_dynamic_cpu_hotplug_enable(g_prev_dynamic_cpu_hotplug_enable);
    #endif //#ifdef CONFIG_CPU_FREQ_GOV_HOTPLUG
    }
    
    return 0;
}



/**************************************************************
* mt hotplug mechanism control interface for procfs test0
***************************************************************/
static int mt_hotplug_mechanism_read_test0(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", g_test0);
    
    HOTPLUG_INFO("mt_hotplug_mechanism_read_test0, hotplug_cpu_count: %d\n", atomic_read(&hotplug_cpu_count));
    on_each_cpu((smp_call_func_t)dump_stack, NULL, 1);
    
    /*
    //TODO: add dump per core last pc for debug purpose
    mt_reg_sync_writel(8, MP0_DBG_CTRL);
    pr_emerg("CPU%u, MP0_DBG_CTRL: 0x%08x, MP0_DBG_FLAG: 0x%08x\n", 0, *(volatile u32 *)(MP0_DBG_CTRL), *(volatile u32 *)(MP0_DBG_FLAG));
    mt_reg_sync_writel(9, MP0_DBG_CTRL);
    pr_emerg("CPU%u, MP0_DBG_CTRL: 0x%08x, MP0_DBG_FLAG: 0x%08x\n", 1, *(volatile u32 *)(MP0_DBG_CTRL), *(volatile u32 *)(MP0_DBG_FLAG));
    mt_reg_sync_writel(10, MP0_DBG_CTRL);
    pr_emerg("CPU%u, MP0_DBG_CTRL: 0x%08x, MP0_DBG_FLAG: 0x%08x\n", 2, *(volatile u32 *)(MP0_DBG_CTRL), *(volatile u32 *)(MP0_DBG_FLAG));
    mt_reg_sync_writel(11, MP0_DBG_CTRL);
    pr_emerg("CPU%u, MP0_DBG_CTRL: 0x%08x, MP0_DBG_FLAG: 0x%08x\n", 3, *(volatile u32 *)(MP0_DBG_CTRL), *(volatile u32 *)(MP0_DBG_FLAG));
    
    mt_reg_sync_writel(8, MP1_DBG_CTRL);
    pr_emerg("CPU%u, MP1_DBG_CTRL: 0x%08x, MP1_DBG_FLAG: 0x%08x\n", 4, *(volatile u32 *)(MP1_DBG_CTRL), *(volatile u32 *)(MP1_DBG_FLAG));
    mt_reg_sync_writel(9, MP1_DBG_CTRL);
    pr_emerg("CPU%u, MP1_DBG_CTRL: 0x%08x, MP1_DBG_FLAG: 0x%08x\n", 5, *(volatile u32 *)(MP1_DBG_CTRL), *(volatile u32 *)(MP1_DBG_FLAG));
    mt_reg_sync_writel(10, MP1_DBG_CTRL);
    pr_emerg("CPU%u, MP1_DBG_CTRL: 0x%08x, MP1_DBG_FLAG: 0x%08x\n", 6, *(volatile u32 *)(MP1_DBG_CTRL), *(volatile u32 *)(MP1_DBG_FLAG));
    mt_reg_sync_writel(11, MP1_DBG_CTRL);
    pr_emerg("CPU%u, MP1_DBG_CTRL: 0x%08x, MP1_DBG_FLAG: 0x%08x\n", 7, *(volatile u32 *)(MP1_DBG_CTRL), *(volatile u32 *)(MP1_DBG_FLAG));
    */
    
    return 0;
}

static int mt_hotplug_mechanism_write_test0(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int len = 0, test0 = 0;
    char desc[32];
    char cmd1[16];
    int cmd2;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';
    
    if (sscanf(desc, "%s %d", cmd1, &cmd2) == 2)
    {
        if (strcmp(cmd1, "wakeup_debug") == 0)
        {
            extern int wakeup_debug;
            
            wakeup_debug = cmd2;
            HOTPLUG_INFO("wakeup_debug: %d\n", wakeup_debug);
        }
        return count;
    }
    else if (sscanf(desc, "%d", &test0) == 1)
    {
        g_test0 = test0;
        return count;
    }
    else
    {
        HOTPLUG_INFO("mt_hotplug_mechanism_write_test0, bad argument\n");
    }
    
    return -EINVAL;
}

static int mt_hotplug_mechanism_open_test0(struct inode *inode, struct file *file)
{
    return single_open(file, mt_hotplug_mechanism_read_test0, NULL);
}

static const struct file_operations mt_hotplug_test0_fops = { 
    .owner = THIS_MODULE,
    .open  = mt_hotplug_mechanism_open_test0,
    .read  = seq_read,
    .write = mt_hotplug_mechanism_write_test0,
};



/**************************************************************
* mt hotplug mechanism control interface for procfs test1
***************************************************************/
static int mt_hotplug_mechanism_read_test1(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", g_test1);
    
    return 0;
}

static int mt_hotplug_mechanism_write_test1(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int len = 0, test1 = 0;
    char desc[32];
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';
    
    if (sscanf(desc, "%d", &test1) == 1)
    {
        g_test1 = test1;
        
        switch (g_test1)
        {
            case 0:
                spm_mtcmos_ctrl_dbg0(STA_POWER_ON);
                break;
            case 1:
                spm_mtcmos_ctrl_dbg0(STA_POWER_DOWN);
                break;
            case 2:
                //spm_mtcmos_ctrl_dbg1(STA_POWER_ON);
                pr_emerg("SPM_CA7_CPU0_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU0_PWR_CON));
                pr_emerg("SPM_CA7_CPU1_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU1_PWR_CON));
                pr_emerg("SPM_CA7_CPU2_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU2_PWR_CON));
                pr_emerg("SPM_CA7_CPU3_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU3_PWR_CON));
                pr_emerg("SPM_CA7_DBG_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_DBG_PWR_CON));
                pr_emerg("SPM_CA7_CPUTOP_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPUTOP_PWR_CON));
                pr_emerg("SPM_CA7_CPU0_L1_PDN: 0x%08x\n", REG_READ(SPM_CA7_CPU0_L1_PDN));
                pr_emerg("SPM_CA7_CPU1_L1_PDN: 0x%08x\n", REG_READ(SPM_CA7_CPU1_L1_PDN));
                pr_emerg("SPM_CA7_CPU2_L1_PDN: 0x%08x\n", REG_READ(SPM_CA7_CPU2_L1_PDN));
                pr_emerg("SPM_CA7_CPU3_L1_PDN: 0x%08x\n", REG_READ(SPM_CA7_CPU3_L1_PDN));
                pr_emerg("SPM_CA7_CPUTOP_L2_PDN: 0x%08x\n", REG_READ(SPM_CA7_CPUTOP_L2_PDN));
                pr_emerg("SPM_CA15_CPU0_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU0_PWR_CON));
                pr_emerg("SPM_CA15_CPU1_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU1_PWR_CON));
                pr_emerg("SPM_CA15_CPU2_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU2_PWR_CON));
                pr_emerg("SPM_CA15_CPU3_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU3_PWR_CON));
                pr_emerg("SPM_CA15_CPUTOP_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPUTOP_PWR_CON));
                pr_emerg("SPM_CA15_L1_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_L1_PWR_CON));
                pr_emerg("SPM_CA15_L2_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_L2_PWR_CON));
                break;
            case 3:
                //spm_mtcmos_ctrl_dbg1(STA_POWER_DOWN);
                break;
        }
        
        return count;
    }
    else
    {
        HOTPLUG_INFO("mt_hotplug_mechanism_write_test1, bad argument\n");
    }
    
    return -EINVAL;
}

static int mt_hotplug_mechanism_open_test1(struct inode *inode, struct file *file)
{
    return single_open(file, mt_hotplug_mechanism_read_test1, NULL);
}

static const struct file_operations mt_hotplug_test1_fops = { 
    .owner = THIS_MODULE,
    .open  = mt_hotplug_mechanism_open_test1,
    .read  = seq_read,
    .write = mt_hotplug_mechanism_write_test1,
};



/**************************************************************
* mt hotplug mechanism control interface for procfs memory_debug
***************************************************************/
static int mt_hotplug_mechanism_read_memory_debug(struct seq_file *m, void *v)
{
    seq_printf(m, "[0x%08x]=0x%08x\n", g_memory_debug, REG_READ(g_memory_debug));
    HOTPLUG_INFO("[0x%08x]=0x%08x\n", g_memory_debug, REG_READ(g_memory_debug));
    
    return 0;
}

static int mt_hotplug_mechanism_write_memory_debug(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    int len = 0;
    char desc[32];
    char cmd1[16];
    int cmd2, cmd3;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';
    
    if (sscanf(desc, "%s %x %x", cmd1, &cmd2, &cmd3) == 3)
    {
        if (strcmp(cmd1, "w") == 0)
        {
            HOTPLUG_INFO("write [0x%08x] to 0x%08x\n", cmd2, cmd3);
            REG_WRITE(cmd2, cmd3);
        }
        return count;
    }
    else if (sscanf(desc, "%s %x", cmd1, &cmd2) == 2)
    {
        if (strcmp(cmd1, "r") == 0)
        {
            HOTPLUG_INFO("read [0x%08x] as 0x%08x\n", cmd2, REG_READ(cmd2));
            g_memory_debug = cmd2;
        }
        return count;
    }
    else
    {
        HOTPLUG_INFO("mt_hotplug_mechanism_write_memory_debug, bad argument\n");
    }
    
    return -EINVAL;
}

static int mt_hotplug_mechanism_open_memory_debug(struct inode *inode, struct file *file)
{
    return single_open(file, mt_hotplug_mechanism_read_memory_debug, NULL);
}

static const struct file_operations mt_hotplug_memory_debug_fops = { 
    .owner = THIS_MODULE,
    .open  = mt_hotplug_mechanism_open_memory_debug,
    .read  = seq_read,
    .write = mt_hotplug_mechanism_write_memory_debug,
};



/*******************************
* kernel module init function
********************************/
static int __init mt_hotplug_mechanism_init(void)
{
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *mt_hotplug_dir = NULL;
    int r = 0;
    
    HOTPLUG_INFO("mt_hotplug_mechanism_init\n");
    
    mt_hotplug_dir = proc_mkdir("mt_hotplug", NULL);
    if (!mt_hotplug_dir)
    {
        HOTPLUG_INFO("mkdir /proc/mt_hotplug failed\n");
    }
    else
    {
        entry = proc_create("test0", S_IRUGO | S_IWUSR | S_IWGRP, mt_hotplug_dir, &mt_hotplug_test0_fops);
        entry = proc_create("test1", S_IRUGO | S_IWUSR | S_IWGRP, mt_hotplug_dir, &mt_hotplug_test1_fops);
        entry = proc_create("memory_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_hotplug_dir, &mt_hotplug_memory_debug_fops);
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    mt_hotplug_mechanism_early_suspend_handler.suspend = mt_hotplug_mechanism_early_suspend;
    mt_hotplug_mechanism_early_suspend_handler.resume = mt_hotplug_mechanism_late_resume;
    register_early_suspend(&mt_hotplug_mechanism_early_suspend_handler);
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

    r = platform_driver_register(&mt_hotplug_mechanism_pdrv);
    if (r)
        HOTPLUG_INFO("platform_driver_register failed (%d)\n", r);
    
    return r;
}
module_init(mt_hotplug_mechanism_init);



/*******************************
* kernel module exit function
********************************/
static void __exit mt_hotplug_mechanism_exit(void)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_exit\n");
}
module_exit(mt_hotplug_mechanism_exit);



/**************************************************************
* mt hotplug mechanism control interface for thermal protect
***************************************************************/
void mt_hotplug_mechanism_thermal_protect(int limited_cpus)
{
    HOTPLUG_INFO("mt_hotplug_mechanism_thermal_protect\n");

}
EXPORT_SYMBOL(mt_hotplug_mechanism_thermal_protect);



module_param(g_enable, int, 0644);
#ifdef CONFIG_HAS_EARLYSUSPEND
module_param(g_enable_cpu_rush_boost, int, 0644);
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND
module_param(g_enable_dynamic_cpu_hotplug_at_suspend, int, 0644);



MODULE_DESCRIPTION("MediaTek CPU Hotplug Mechanism");
MODULE_LICENSE("GPL");
