#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
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
#include <mach/mt_boot_reason.h>


typedef enum {
	BOOT_REASON_UNINIT = 0,
	BOOT_REASON_INITIALIZING = 1,
	BOOT_REASON_INITIALIZED = 2,
} BOOT_REASON_STATE;

boot_reason_t g_boot_reason __nosavedata = BR_UNKNOWN;

static atomic_t g_br_state  = ATOMIC_INIT(BOOT_REASON_UNINIT);
static atomic_t g_br_errcnt = ATOMIC_INIT(0);
static atomic_t g_br_status = ATOMIC_INIT(0);

#ifdef CONFIG_OF
static int __init dt_get_boot_reason(unsigned long node, const char *uname, int depth, void *data)
{
	char *ptr = NULL, *br_ptr = NULL;

	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	ptr = (char *)of_get_flat_dt_prop(node, "bootargs", NULL);
	if (ptr) {
		if ((br_ptr = strstr(ptr, "boot_reason=")) != 0) {
			g_boot_reason = br_ptr[12] - '0';	/* get boot reason */
            atomic_set(&g_br_status, 1);
        }
		else
			pr_warn("'boot_reason=' is not found\n");
		pr_debug("%s\n", ptr);
	} else
		pr_warn("'bootargs' is not found\n");

	/* break now */
	return 1;
}
#endif


void init_boot_reason(unsigned int line)
{
#ifdef CONFIG_OF
	int rc;

	if (BOOT_REASON_INITIALIZING == atomic_read(&g_br_state)) {    
		pr_warn("%s (%d) state(%d)\n", __func__, line, atomic_read(&g_br_state));
        atomic_inc(&g_br_errcnt);
		return;
	}

	if (BOOT_REASON_UNINIT == atomic_read(&g_br_state))
		atomic_set(&g_br_state, BOOT_REASON_INITIALIZING);
	else
		return;

	if ((BR_UNKNOWN != g_boot_reason)) {
		atomic_set(&g_br_state, BOOT_REASON_INITIALIZED);
        pr_alert("boot_reason = %d\n", g_boot_reason);
		return;
    }

	pr_info("%s %d %d %d\n", __func__, line, g_boot_reason,
		atomic_read(&g_br_state));
	rc = of_scan_flat_dt(dt_get_boot_reason, NULL);
	if (0 != rc)
		atomic_set(&g_br_state, BOOT_REASON_INITIALIZED);
	else 
		atomic_set(&g_br_state, BOOT_REASON_UNINIT);
	pr_info("%s %d %d %d\n", __func__, line, g_boot_reason,
		atomic_read(&g_br_state));
#endif
}

/* return boot reason */
boot_reason_t get_boot_reason(void)
{
	init_boot_reason(__LINE__);
	return g_boot_reason;
}

static int __init boot_reason_core(void)
{
	init_boot_reason(__LINE__);
	return 0;
}

static int __init boot_reason_init(void)
{
    pr_alert("boot_reason = %d, state(%d,%d,%d)", g_boot_reason, 
             atomic_read(&g_br_state), atomic_read(&g_br_errcnt),
             atomic_read(&g_br_status));
	return 0;
}

early_initcall(boot_reason_core);
module_init(boot_reason_init);
MODULE_DESCRIPTION("Mediatek Boot Reason Driver");
MODULE_LICENSE("GPL");
