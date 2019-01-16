#define LOG_TAG "mtk_mira"

#include "disp_drv_log.h"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#include "mtk_mira.h"
#include "mtk_disp_mgr.h"


#define DISP_DEVNAME "mtk_mira"

static struct proc_dir_entry * proc_entry;

static long disp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   return mtk_disp_mgr_ioctl(file,  cmd,  arg);
}
#ifdef CONFIG_COMPAT
static long disp_compat_ioctl(struct file *file, unsigned int cmd,  unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	switch(cmd) {	
	
    // add cases here for 32bit/64bit conversion
    // ...
    
	default:
		return mtk_disp_mgr_ioctl(file,  cmd,  arg);
	}
	return ret;
}
#endif

static int disp_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t disp_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
    return 0;
}

static int disp_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int disp_flush(struct file * file , fl_owner_t a_id)
{
    return 0;
}

// remap register to user space
static int disp_mmap(struct file * file, struct vm_area_struct * a_pstVMArea)
{
    return 0;
}

/* Kernel interface */
static struct file_operations disp_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = disp_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = disp_compat_ioctl,
#endif	

	.open		= disp_open,
	.release	= disp_release,
	.flush		= disp_flush,
	.read       = disp_read,
	.mmap       = disp_mmap
};

static int __init disp_init(void)
{
    int ret = 0;
    DISPMSG("Register the disp driver\n");
    proc_entry = proc_create(DISP_DEVNAME,0644, NULL,&disp_fops);
	if(proc_entry == NULL)
	{
	    ret = -ENOMEM;
	}
    return ret;
}

static void __exit disp_exit(void)
{
    remove_proc_entry(DISP_DEVNAME,proc_entry);
    DISPMSG("Done\n");
}

module_init(disp_init);
module_exit(disp_exit);
MODULE_AUTHOR("Tzu-Meng, Chung <Tzu-Meng.Chung@mediatek.com>");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
