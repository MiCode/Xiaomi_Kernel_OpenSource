#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/time.h>

#include <linux/string.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/irqs.h>
#include <accdet_custom_def.h>
#include <accdet_custom.h>
#include <mach/reg_accdet.h>


/*******************************************************************************
costom API
*******************************************************************************/
/*******************************************************************************
HAL API
*******************************************************************************/

int dump_register(void);
void mt_accdet_remove(void);
void mt_accdet_suspend(void);
void mt_accdet_resume(void);
void mt_accdet_pm_restore_noirq(void);
int mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg);
//int mt_accdet_store_call_state(const char *buf, size_t count);
//int accdet_get_pin_reg_state(void);
int mt_accdet_probe(void);
int accdet_get_cable_type(void);
struct file_operations *accdet_ops(void);








