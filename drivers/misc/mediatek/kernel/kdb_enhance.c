#include <linux/sched.h>
#include <linux/utsname.h>
#include <linux/kdb.h>

#ifdef CONFIG_SCHED_DEBUG

DEFINE_PER_CPU(int, kdb_in_use) = 0;

extern int sysrq_sched_debug_show(void);

/*
 * Display sched_debug information
 */
static int kdb_sched_debug(int argc, const char **argv)
{
	sysrq_sched_debug_show();
	return 0;
}

#endif

static __init int kdb_enhance_register(void)
{
#ifdef CONFIG_SCHED_DEBUG
	kdb_register_repeat("sched_debug", kdb_sched_debug, "",
			    "Display sched_debug information", 0, KDB_REPEAT_NONE);
#endif
	return 0;
}

__initcall(kdb_enhance_register);
