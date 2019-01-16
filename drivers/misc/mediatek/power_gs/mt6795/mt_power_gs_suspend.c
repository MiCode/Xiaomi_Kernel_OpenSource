#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_power_gs.h>

extern unsigned int *AP_ANALOG_gs_suspend;
extern unsigned int AP_ANALOG_gs_suspend_len;

extern unsigned int *AP_CG_gs_suspend;
extern unsigned int AP_CG_gs_suspend_len;

extern unsigned int *AP_DCM_gs_suspend;
extern unsigned int AP_DCM_gs_suspend_len;

extern unsigned int *MT6331_PMIC_REG_gs_flightmode_suspend_mode;
extern unsigned int MT6331_PMIC_REG_gs_flightmode_suspend_mode_len;

extern unsigned int *MT6332_PMIC_REG_gs_flightmode_suspend_mode;
extern unsigned int MT6332_PMIC_REG_gs_flightmode_suspend_mode_len;

void mt_power_gs_dump_suspend(void)
{
    mt_power_gs_compare("Suspend",                                \
                        AP_ANALOG_gs_suspend, AP_ANALOG_gs_suspend_len,   \
                        AP_CG_gs_suspend, AP_CG_gs_suspend_len,   \
                        AP_DCM_gs_suspend, AP_DCM_gs_suspend_len, \
			MT6331_PMIC_REG_gs_flightmode_suspend_mode, MT6331_PMIC_REG_gs_flightmode_suspend_mode_len, \
			MT6332_PMIC_REG_gs_flightmode_suspend_mode, MT6332_PMIC_REG_gs_flightmode_suspend_mode_len);
}

//static int dump_suspend_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
static int dump_suspend_read(struct seq_file *m, void *v)
{
    seq_printf(m, "mt_power_gs : suspend\n");
    mt_power_gs_dump_suspend();

    return 0;
}

static void __exit mt_power_gs_suspend_exit(void)
{
    remove_proc_entry("dump_suspend", mt_power_gs_dir);
}

static int proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, dump_suspend_read, NULL);
}

static const struct file_operations proc_fops =
{
    .owner = THIS_MODULE,
    .open = proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init mt_power_gs_suspend_init(void)
{
    struct proc_dir_entry *mt_entry = NULL;

    if (!mt_power_gs_dir)
    {
        printk("[%s]: mkdir /proc/mt_power_gs failed\n", __FUNCTION__);
    }
    else
    {
        mt_entry = proc_create("dump_suspend", S_IRUGO | S_IWUSR | S_IWGRP, mt_power_gs_dir, &proc_fops);
        if (NULL == mt_entry)
        {
            return -ENOMEM;
        }
    }

    return 0;
}

module_init(mt_power_gs_suspend_init);
module_exit(mt_power_gs_suspend_exit);

MODULE_DESCRIPTION("MT Power Golden Setting - Suspend");
