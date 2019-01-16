#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>

#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>

#include <mach/mt_ccci_common.h>
//#include <mach/mtk_ccci_helper.h>
// extern unsigned long ccci_get_md_boot_count(int md_id);

extern struct proc_dir_entry * mtk_thermal_get_proc_drv_therm_dir_entry(void);

extern int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len);

#define mtk_cooler_mutt_dprintk_always(fmt, args...) \
    do { xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/mutt", fmt, ##args); } while(0)

#define mtk_cooler_mutt_dprintk(fmt, args...) \
    do { \
        if (1 == cl_mutt_klog_on) { \
            xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/mutt", fmt, ##args); \
        } \
    } while(0)

#define MAX_NUM_INSTANCE_MTK_COOLER_MUTT  4

#define MTK_CL_MUTT_GET_LIMIT(limit, state) \
    do { (limit) = (short) (((unsigned long) (state))>>16); } while(0)

#define MTK_CL_MUTT_SET_LIMIT(limit, state) \
    do { state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); } while(0)

#define MTK_CL_MUTT_GET_CURR_STATE(curr_state, state) \
    do { curr_state = (((unsigned long) (state))&0xFFFF); } while(0)

#define MTK_CL_MUTT_SET_CURR_STATE(curr_state, state) \
    do { if (0 == curr_state) \
           state &= ~0x1; \
         else \
           state |= 0x1; \
    } while(0)

static int cl_mutt_klog_on = 0;
static struct thermal_cooling_device *cl_mutt_dev[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = {0};
static unsigned int cl_mutt_param[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = {0};
static unsigned long cl_mutt_state[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = {0};
static unsigned int cl_mutt_cur_limit = 0;

static unsigned long last_md_boot_cnt = 0;

static void
mtk_cl_mutt_set_mutt_limit(void)
{
    // TODO: optimize
    int i = 0, ret = 0;
    int min_limit = 255;
    unsigned int min_param = 0;

    for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i++)
    {
        unsigned long curr_state;
        MTK_CL_MUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);

        if (1 == curr_state)
        {
            unsigned int active;
            unsigned int suspend;
            int limit = 0;

            active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
            suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

            // a cooler with 0 active or 0 suspend is not allowed
            if (active == 0 || suspend == 0)
                goto err_unreg;

            // compare the active/suspend ratio
            if (active >= suspend) limit = active/suspend;
            else limit = (0 - suspend)/active;

            if (limit <= min_limit)
            {
                min_limit = limit;
                min_param = cl_mutt_param[i];
            }
        }
    }

    if (min_param != cl_mutt_cur_limit)
    {
        cl_mutt_cur_limit = min_param;
        last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1);
        ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char*) &cl_mutt_cur_limit, 4);
        mtk_cooler_mutt_dprintk_always("[%s] ret %d param %x bcnt %ul\n", __func__, ret, cl_mutt_cur_limit, last_md_boot_cnt);
    }
    else if (min_param != 0)
    {
        unsigned long cur_md_bcnt = ccci_get_md_boot_count(MD_SYS1);
        if (last_md_boot_cnt != cur_md_bcnt)
        {
            last_md_boot_cnt = cur_md_bcnt;
            ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char*) &cl_mutt_cur_limit, 4);
            mtk_cooler_mutt_dprintk_always("[%s] mdrb ret %d param %x bcnt %ul\n", __func__, ret, cl_mutt_cur_limit, last_md_boot_cnt);
        }
    }

err_unreg:
    return;

}

static int
mtk_cl_mutt_get_max_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{
    *state = 1;
    mtk_cooler_mutt_dprintk("mtk_cl_mutt_get_max_state() %s %d\n", cdev->type, *state);
    return 0;
}

static int
mtk_cl_mutt_get_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{
    MTK_CL_MUTT_GET_CURR_STATE(*state, *((unsigned long*) cdev->devdata));
    mtk_cooler_mutt_dprintk("mtk_cl_mutt_get_cur_state() %s %d\n", cdev->type, *state);
    return 0;
}

static int
mtk_cl_mutt_set_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long state)
{
    mtk_cooler_mutt_dprintk("mtk_cl_mutt_set_cur_state() %s %d\n", cdev->type, state);
    MTK_CL_MUTT_SET_CURR_STATE(state, *((unsigned long*) cdev->devdata));
    mtk_cl_mutt_set_mutt_limit();

    return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mutt_ops = {
    .get_max_state = mtk_cl_mutt_get_max_state,
    .get_cur_state = mtk_cl_mutt_get_cur_state,
    .set_cur_state = mtk_cl_mutt_set_cur_state,
};

static int mtk_cooler_mutt_register_ltf(void)
{
    int i;
    mtk_cooler_mutt_dprintk("register ltf\n");

    for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0; )
    {
        char temp[20] = {0};
        sprintf(temp, "mtk-cl-mutt%02d", i);
        cl_mutt_dev[i] = mtk_thermal_cooling_device_register(temp,
                                                             (void*) &cl_mutt_state[i], // put mutt state to cooler devdata
                                                             &mtk_cl_mutt_ops);
    }

    return 0;
}

static void mtk_cooler_mutt_unregister_ltf(void)
{
    int i;
    mtk_cooler_mutt_dprintk("unregister ltf\n");

    for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0; )
    {
        if (cl_mutt_dev[i])
        {
            mtk_thermal_cooling_device_unregister(cl_mutt_dev[i]);
            cl_mutt_dev[i] = NULL;
            cl_mutt_state[i] = 0;
        }
    }
}

static int _mtk_cl_mutt_proc_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-mutt<ID>> <active (ms)> <suspend (ms)> <param> <state>
     *  ..
     */
    {
        int i = 0;

        seq_printf(m, "klog %d\n", cl_mutt_klog_on);
        seq_printf(m, "curr_limit %x\n", cl_mutt_cur_limit);

        for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i++)
        {
            unsigned int active;
            unsigned int suspend;
            unsigned long curr_state;

            active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
            suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

            MTK_CL_MUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);

            seq_printf(m, "mtk-cl-mutt%02d %u %u %x, state %lu\n", i, active, suspend, cl_mutt_param[i], curr_state);
        }
    }

    return 0;
}

static ssize_t _mtk_cl_mutt_proc_write(struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
    int len = 0;
    char desc[128];
    int klog_on, mutt0_a, mutt0_s, mutt1_a, mutt1_s, mutt2_a, mutt2_s, mutt3_a, mutt3_s;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';

    /**
     * sscanf format <klog_on> <mtk-cl-mutt00 active (ms)> <mtk-cl-mutt00 suspended (ms)> <mtk-cl-mutt01 active (ms)> <mtk-cl-mutt01 suspended (ms)> <mtk-cl-mutt02 active (ms)> <mtk-cl-mutt02 suspended (ms)>...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-mutt* active/suspended (ms) > can only be positive integer or 0 to denote no limit
     */

    if (NULL == data)
    {
        mtk_cooler_mutt_dprintk("[%s] null data\n", __func__);
        return -EINVAL;
    }

    // WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3
#if (4 == MAX_NUM_INSTANCE_MTK_COOLER_MUTT)
    //cl_mutt_param[0] = 0;
    //cl_mutt_param[1] = 0;
    //cl_mutt_param[2] = 0;

    if (1 <= sscanf(desc, "%d %d %d %d %d %d %d %d %d",
                    &klog_on, &mutt0_a, &mutt0_s, &mutt1_a, &mutt1_s, &mutt2_a, &mutt2_s, &mutt3_a, &mutt3_s))
    {
        if (klog_on == 0 || klog_on == 1)
        {
            cl_mutt_klog_on = klog_on;
        }

        if (mutt0_a == 0)
            cl_mutt_param[0] = 0;
        else if (mutt0_a >= 100 && mutt0_a <= 25500 && mutt0_s >= 100 && mutt0_s <= 25500)
            cl_mutt_param[0] = ((mutt0_s/100) << 16) | ((mutt0_a/100) << 8) | 1;

        if (mutt1_a == 0)
            cl_mutt_param[1] = 0;
        else if (mutt1_a >= 100 && mutt1_a <= 25500 && mutt1_s >= 100 && mutt1_s <= 25500)
            cl_mutt_param[1] = ((mutt1_s/100) << 16) | ((mutt1_a/100) << 8) | 1;

        if (mutt2_a == 0)
            cl_mutt_param[2] = 0;
        else if (mutt2_a >= 100 && mutt2_a <= 25500 && mutt2_s >= 100 && mutt2_s <= 25500)
            cl_mutt_param[2] = ((mutt2_s/100) << 16) | ((mutt2_a/100) << 8) | 1;

        if (mutt3_a == 0)
            cl_mutt_param[3] = 0;
        else if (mutt3_a >= 100 && mutt3_a <= 25500 && mutt3_s >= 100 && mutt3_s <= 25500)
            cl_mutt_param[3] = ((mutt3_s/100) << 16) | ((mutt3_a/100) << 8) | 1;
        
        return count;
    }
    else
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_MUTT!"
#endif
    {
        mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);
    }

    return -EINVAL;
}

static int _mtk_cl_mutt_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, _mtk_cl_mutt_proc_read, NULL);
}

static const struct file_operations cl_mutt_fops = {
    .owner = THIS_MODULE,
    .open = _mtk_cl_mutt_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = _mtk_cl_mutt_proc_write,
    .release = single_release,
};

static int __init mtk_cooler_mutt_init(void)
{
    int err = 0;
    int i;

    for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0; )
    {
        cl_mutt_dev[i] = NULL;
        cl_mutt_state[i] = 0;
    }

    mtk_cooler_mutt_dprintk("init\n");

    err = mtk_cooler_mutt_register_ltf();
    if (err)
        goto err_unreg;

    /* create a proc file */
    {
        struct proc_dir_entry *entry = NULL;
        struct proc_dir_entry *dir_entry = NULL;

        dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
        if (!dir_entry) {
            mtk_cooler_mutt_dprintk_always("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
        }
        else
        {
            entry = proc_create("clmutt", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry, &cl_mutt_fops);
            if (entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
                proc_set_user(entry, 0, 1000);
#else
                entry->gid = 1000;
#endif
            }
        }
    }

    return 0;

err_unreg:
    mtk_cooler_mutt_unregister_ltf();
    return err;
}

static void __exit mtk_cooler_mutt_exit(void)
{
    mtk_cooler_mutt_dprintk("exit\n");

    /* remove the proc file */
    remove_proc_entry("clmutt", NULL);

    mtk_cooler_mutt_unregister_ltf();
}

module_init(mtk_cooler_mutt_init);
module_exit(mtk_cooler_mutt_exit);
