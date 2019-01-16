#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_power_gs.h>

extern unsigned int *AP_ANALOG_gs_mp3_play;
extern unsigned int AP_ANALOG_gs_mp3_play_len;

extern unsigned int *AP_CG_gs_mp3_play;
extern unsigned int AP_CG_gs_mp3_play_len;

extern unsigned int *AP_DCM_gs_mp3_play;
extern unsigned int AP_DCM_gs_mp3_play_len;

extern unsigned int *MT6331_PMIC_REG_gs_mp3_play;
extern unsigned int MT6331_PMIC_REG_gs_mp3_play_len;

extern unsigned int *MT6332_PMIC_REG_gs_mp3_play;
extern unsigned int MT6332_PMIC_REG_gs_mp3_play_len;

void mt_power_gs_dump_audio_playback(void)
{
    mt_power_gs_compare("Audio Playback",                           \
                        AP_ANALOG_gs_mp3_play, AP_ANALOG_gs_mp3_play_len, \
                        AP_CG_gs_mp3_play, AP_CG_gs_mp3_play_len,   \
                        AP_DCM_gs_mp3_play, AP_DCM_gs_mp3_play_len, \
                        MT6331_PMIC_REG_gs_mp3_play, MT6331_PMIC_REG_gs_mp3_play_len, \
                        MT6332_PMIC_REG_gs_mp3_play, MT6332_PMIC_REG_gs_mp3_play_len);
}

static int dump_audio_playback_read(struct seq_file *m, void *v)
{
    seq_printf(m, "mt_power_gs : audio_playback\n");
    mt_power_gs_dump_audio_playback();

    return 0;
}

static void __exit mt_power_gs_audio_playback_exit(void)
{
    remove_proc_entry("dump_audio_playback", mt_power_gs_dir);
}

static int proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, dump_audio_playback_read, NULL);
}

static const struct file_operations proc_fops =
{
    .owner = THIS_MODULE,
    .open = proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init mt_power_gs_audio_playback_init(void)
{
    struct proc_dir_entry *mt_entry = NULL;

    if (!mt_power_gs_dir)
    {
        printk("[%s]: mkdir /proc/mt_power_gs failed\n", __FUNCTION__);
    }
    else
    {
        mt_entry = proc_create("dump_audio_playback", S_IRUGO | S_IWUSR | S_IWGRP, mt_power_gs_dir, &proc_fops);
        if (NULL == mt_entry)
        {
            return -ENOMEM;
        }
    }

    return 0;
}

module_init(mt_power_gs_audio_playback_init);
module_exit(mt_power_gs_audio_playback_exit);

MODULE_DESCRIPTION("MT Power Golden Setting - Audio Playback");
