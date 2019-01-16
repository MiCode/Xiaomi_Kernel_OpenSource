#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/timer.h>
//#include <asm/system.h>
#include <asm-generic/irq_regs.h>
//#include <asm/mach/map.h>
#include <mach/sync_write.h>
#include <mach/irqs.h>
#include <asm/cacheflush.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/fb.h>
#include <linux/debugfs.h>
#include <mach/mt_typedefs.h>
#include <mach/m4u.h>
#include <mach/mt_smi.h>

#include "smi_common.h"

#include <linux/xlog.h>

#include "smi_reg.h"

#define SMI_LOG_TAG "smi"

static char debug_buffer[4096];

#if 0
static char STR_HELP[] =
    "\n"
    "USAGE\n"
    "        echo [ACTION]... > mau_dbg\n"
    "\n"
    "ACTION\n"
    "        module1|module2?R/W/RW(startPhyAddr,endPhyAddr)@MAU_Enty_ID\n"
    "             MAU will monitor specified module whether R/W specified range of memory\n"
    "             example: echo tvc|lcd_r?R(0,0x1000)@1 > mau_dbg\n"
    "             you can use [all] to specify all modules\n"
    "             example: echo all?W(0x2000,0x9000)@2 > mau_dbg\n"
    "\n"
    "        module1|module2@MAU_Enty_ID:off\n"
    "             Turn off specified module on specified MAU Entry\n"
    "             example: echo tvc|lcd_r@1:off > mau_dbg\n"
    "\n"
    "\n"
    "        all:off\n"
    "             Turn off all of modules\n"
    "             example: echo all:off > mau_dbg\n"
    "\n"
    "        list modules\n"
    "             list all module names MAU could monitor\n"
    "\n"
    "        reg:[MPU|MAU1|MAU2]\n"
    "             dump hw register values\n"
    "\n"
    "        regw:addr=val\n"
    "             write hw register\n"
    "\n"
    "        regr:addr\n"
    "             read hw register\n"
    "\n"
    "        m4u_log:on\n"
    "             start to print m4u translate miss rate every second \n"
    "\n"
    "        m4u_log:off\n"
    "             stop to print m4u translate miss rate every second \n"
    "\n"
    "        m4u_debug:[command] \n"
    "             input a command, used for debug \n"
    "\n"
    "        m4u_monitor:on\n"
    "             start to print m4u translate miss rate every second \n"
    "\n"
    "        m4u_monitor:off\n"
    "             stop to print m4u translate miss rate every second \n";
#endif

static void process_dbg_opt(const char *opt)
{
    if (0 == strncmp(opt, "set_reg:", 8 ))
    {
        unsigned long addr;
        unsigned int val;
		char *p = (char *)opt + 8;

		addr = (unsigned long) simple_strtoul(p, &p, 16);
		p++;
		val = (unsigned int) simple_strtoul(p, &p, 16);

		SMIMSG("set register: 0x%lx = 0x%x\n", addr, val);

        COM_WriteReg32(addr, val);
    }
    if (0 == strncmp(opt, "get_reg:", 8 ))
    {
        unsigned long addr;
		char *p = (char *)opt + 8;

		addr = (unsigned long) simple_strtoul(p, &p, 16);

		SMIMSG("get register: 0x%lx = 0x%x \n", addr, COM_ReadReg32(addr));
    }

    return;
}


static void process_dbg_cmd(char *cmd)
{
    char *tok;
    while ((tok = strsep(&cmd, " ")) != NULL)
    {
        process_dbg_opt(tok);
    }
}


// ---------------------------------------------------------------------------
//  Debug FileSystem Routines
// ---------------------------------------------------------------------------

struct dentry *smi_dbgfs = NULL;


static int debug_open(struct inode *inode, struct file *file)
{
    file->private_data = inode->i_private;
    return 0;
}

static ssize_t debug_read(struct file *file,
                          char __user *ubuf, size_t count, loff_t *ppos)
{
    int n = 0;
    return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}


static ssize_t debug_write(struct file *file,
                           const char __user *ubuf, size_t count, loff_t *ppos)
{
    const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
        count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

    process_dbg_cmd(debug_buffer);

    return ret;
}


static struct file_operations debug_fops = {
	.read  = debug_read,
    .write = debug_write,
	.open  = debug_open,
};


void SMI_DBG_Init(void)
{
    smi_dbgfs = debugfs_create_file("smi",
        S_IFREG|S_IRUGO, NULL, (void *)0, &debug_fops);
}


void SMI_DBG_Deinit(void)
{
    debugfs_remove(smi_dbgfs);
}


