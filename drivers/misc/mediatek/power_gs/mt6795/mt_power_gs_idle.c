#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_power_gs.h>

extern unsigned int *AP_ANALOG_gs_idle;
extern unsigned int AP_ANALOG_gs_idle_len;

extern unsigned int *AP_CG_gs_idle;
extern unsigned int AP_CG_gs_idle_len;

extern unsigned int *AP_DCM_gs_idle;
extern unsigned int AP_DCM_gs_idle_len;

extern unsigned int *MT6331_PMIC_REG_gs_early_suspend_idle;
extern unsigned int MT6331_PMIC_REG_gs_early_suspend_idle_len;

extern unsigned int *MT6332_PMIC_REG_gs_early_suspend_idle;
extern unsigned int MT6332_PMIC_REG_gs_early_suspend_idle_len;

void mt_power_gs_dump_idle(void)
{
    mt_power_gs_compare("Idle",                             \
                        AP_ANALOG_gs_idle, AP_ANALOG_gs_idle_len, \
                        AP_CG_gs_idle, AP_CG_gs_idle_len,   \
                        AP_DCM_gs_idle, AP_DCM_gs_idle_len, \
			MT6331_PMIC_REG_gs_early_suspend_idle, MT6331_PMIC_REG_gs_early_suspend_idle_len, \
			MT6332_PMIC_REG_gs_early_suspend_idle, MT6332_PMIC_REG_gs_early_suspend_idle_len);
}

static int dump_idle_read(struct seq_file *m, void *v)
{
    seq_printf(m, "mt_power_gs : idle\n");
    mt_power_gs_dump_idle();

    return 0;
}

static void __exit mt_power_gs_idle_exit(void)
{
    remove_proc_entry("dump_idle", mt_power_gs_dir);
}

static int proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, dump_idle_read, NULL);
}

static const struct file_operations proc_fops =
{
    .owner = THIS_MODULE,
    .open = proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init mt_power_gs_idle_init(void)
{
    struct proc_dir_entry *mt_entry = NULL;

    if (!mt_power_gs_dir)
    {
        printk("[%s]: mkdir /proc/mt_power_gs failed\n", __FUNCTION__);
    }
    else
    {
        mt_entry = proc_create("dump_idle", S_IRUGO | S_IWUSR | S_IWGRP, mt_power_gs_dir, &proc_fops);
        if (NULL == mt_entry)
        {
            return -ENOMEM;
        }
    }

    return 0;
}

module_init(mt_power_gs_idle_init);
module_exit(mt_power_gs_idle_exit);

MODULE_DESCRIPTION("MT Power Golden Setting - idle");
