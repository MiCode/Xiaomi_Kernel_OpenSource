#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include <asm/atomic.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot_common.h>

#include <mach/mt_ccci_common.h>

extern int ccci_common_sysfs_init(void);
extern void ccci_log_init(void);
extern int ccci_util_fo_init(void);

//===================================================
// Dummy function part
//===================================================
int ccci_parse_meta_md_setting(unsigned char args[])
{
	return 0;
}

static int __init ccci_util_init(void)
{
	ccci_util_fo_init();
	ccci_log_init();
	ccci_common_sysfs_init();
	return 0;
}

subsys_initcall(ccci_util_init);
MODULE_DESCRIPTION("MTK CCCI UTIL Driver");
MODULE_LICENSE("GPL");
