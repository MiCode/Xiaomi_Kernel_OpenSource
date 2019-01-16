#include <linux/bug.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/kgdb.h>
#include <linux/kdb.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/wd_api.h>
#include "aee-common.h"
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>


#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show(void);
#endif

static struct aee_kernel_api *g_aee_api;
#define KERNEL_REPORT_LENGTH 512

#ifdef CONFIG_KGDB_KDB
/* Press key to enter kdb */
void aee_trigger_kdb(void)
{
	int res = 0;
	struct wd_api *wd_api = NULL;
	res = get_wd_api(&wd_api);
	/* disable Watchdog HW, note it will not enable WDT again when kdb return */
	if (res) {
		LOGE("aee_trigger_kdb, get wd api error\n");
	} else {
		wd_api->wd_disable_all();
	}

#ifdef CONFIG_SCHED_DEBUG
	sysrq_sched_debug_show();
#endif

	LOGI("User trigger KDB\n");
	mtk_set_kgdboc_var();
	kgdb_breakpoint();

	LOGI("Exit KDB\n");
#ifdef CONFIG_LOCAL_WDT
	/* enable local WDT */
	if (res) {
		LOGD("aee_trigger_kdb, get wd api error\n");
	} else {
		wd_api->wd_restart(WD_TYPE_NOLOCK);
	}

#endif

}
#else
#ifdef CONFIG_MTK_AEE_IPANIC
extern void aee_dumpnative(void);
#endif
/* For user mode or the case KDB is not enabled, print basic debug messages */
void aee_dumpbasic(void)
{
	struct task_struct *p = current;
	int orig_log_level = console_loglevel;

	preempt_disable();
	console_loglevel = 7;
	LOGI("kernel  : %s-%s\n", init_uts_ns.name.sysname, init_uts_ns.name.release);
	LOGI("version : %s\n", init_uts_ns.name.version);
	LOGI("machine : %s\n\n", init_uts_ns.name.machine);

#ifdef CONFIG_SCHED_DEBUG
	sysrq_sched_debug_show();
#endif
	LOGI("\n%-*s      Pid   Parent Command\n", (int)(2 * sizeof(void *)) + 2, "Task Addr");
	LOGI("0x%p %8d %8d  %s\n\n", (void *)p, p->pid, p->parent->pid, p->comm);
	LOGI("Stack traceback for current pid %d\n", p->pid);
	show_stack(p, NULL);

#ifdef CONFIG_MTK_AEE_IPANIC_64
	aee_dumpnative();
#endif

	console_loglevel = orig_log_level;
	preempt_enable();
}

void aee_trigger_kdb(void)
{
	LOGI("\nKDB is not enabled ! Dump basic debug info...\n\n");
	aee_dumpbasic();
}
#endif

struct aee_oops *aee_oops_create(AE_DEFECT_ATTR attr, AE_EXP_CLASS clazz, const char *module)
{
	struct aee_oops *oops = kzalloc(sizeof(struct aee_oops), GFP_ATOMIC);
	if (NULL == oops) {
		LOGE("%s : kzalloc() fail\n", __func__);
		return NULL;
	}
	oops->attr = attr;
	oops->clazz = clazz;
	if (module != NULL) {
		strlcpy(oops->module, module, sizeof(oops->module));
	} else {
		strcpy(oops->module, "N/A");
	}
	strcpy(oops->backtrace, "N/A");
	strcpy(oops->process_path, "N/A");

	return oops;
}
EXPORT_SYMBOL(aee_oops_create);

void aee_oops_set_process_path(struct aee_oops *oops, const char *process_path)
{
	if (process_path != NULL) {
		strlcpy(oops->process_path, process_path, sizeof(oops->process_path));
	}
}

void aee_oops_set_backtrace(struct aee_oops *oops, const char *backtrace)
{
	if (backtrace != NULL) {
		strlcpy(oops->backtrace, backtrace, sizeof(oops->backtrace));
	}
}

void aee_oops_free(struct aee_oops *oops)
{
	if (oops->console) {
		kfree(oops->console);
	}
	if (oops->android_main) {
		kfree(oops->android_main);
	}
	if (oops->android_radio) {
		kfree(oops->android_radio);
	}
	if (oops->android_system) {
		kfree(oops->android_system);
	}
	if (oops->userspace_info) {
		kfree(oops->userspace_info);
	}
	if (oops->mmprofile) {
		kfree(oops->mmprofile);
	}
	if (oops->mini_rdump) {
		kfree(oops->mini_rdump);
	}
	if(oops->userthread_stack.Userthread_Stack)
		vfree(oops->userthread_stack.Userthread_Stack);
	kfree(oops);
	LOGE("aee_oops_free\n");
}
EXPORT_SYMBOL(aee_oops_free);

void aee_register_api(struct aee_kernel_api *aee_api)
{
	if (!aee_api) {
		BUG();
	}
	g_aee_api = aee_api;
}
EXPORT_SYMBOL(aee_register_api);

void aee_disable_api(void)
{
	if (g_aee_api) {
		LOGI("disable aee kernel api");
		g_aee_api = NULL;
	}
}
EXPORT_SYMBOL(aee_disable_api);

void aee_kernel_exception_api(const char *file, const int line, const int db_opt,
			      const char *module, const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file, line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset, msg, args);
	if (g_aee_api && g_aee_api->kernel_reportAPI) {
		g_aee_api->kernel_reportAPI(AE_DEFECT_EXCEPTION, db_opt, module, msgbuf);
	} else {
		LOGE("AEE kernel exception: %s", msgbuf);
	}
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_exception_api);

void aee_kernel_warning_api(const char *file, const int line, const int db_opt, const char *module,
			    const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file, line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset, msg, args);
	if (g_aee_api && g_aee_api->kernel_reportAPI) {
		g_aee_api->kernel_reportAPI(AE_DEFECT_WARNING, db_opt, module, msgbuf);
	} else {
		LOGE("AEE kernel warning: %s", msgbuf);
	}
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_warning_api);

void aee_kernel_reminding_api(const char *file, const int line, const int db_opt,
			      const char *module, const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file, line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset, msg, args);
	if (g_aee_api && g_aee_api->kernel_reportAPI) {
		g_aee_api->kernel_reportAPI(AE_DEFECT_REMINDING, db_opt, module, msgbuf);
	} else {
		LOGE("AEE kernel reminding: %s", msgbuf);
	}
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_reminding_api);

void aed_md_exception_api(const int *log, int log_size, const int *phy, int phy_size,
			  const char *detail, const int db_opt)
{
#ifdef CONFIG_MTK_AEE_AED
	LOGD("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->md_exception) {
			g_aee_api->md_exception("modem", log, log_size, phy, phy_size, detail,
						db_opt);
		} else {
			LOGD("g_aee_api->md_exception = 0x%p\n", g_aee_api->md_exception);
		}
	} else {
		LOGD("g_aee_api is null\n");
	}
	LOGD("%s out\n", __func__);
#endif
}
EXPORT_SYMBOL(aed_md_exception_api);

void aed_md32_exception_api(const int *log, int log_size, const int *phy, int phy_size,
			    const char *detail, const int db_opt)
{
#ifdef CONFIG_MTK_AEE_AED
	LOGD("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->md_exception) {
			g_aee_api->md_exception("md32", log, log_size, phy, phy_size, detail,
						db_opt);
		} else {
			LOGD("g_aee_api->md32_exception = 0x%p\n", g_aee_api->md32_exception);
		}
	} else {
		LOGD("g_aee_api is null\n");
	}
	LOGD("%s out\n", __func__);
#endif
}
EXPORT_SYMBOL(aed_md32_exception_api);

void aed_combo_exception_api(const int *log, int log_size, const int *phy, int phy_size,
			     const char *detail, const int db_opt)
{
#ifdef CONFIG_MTK_AEE_AED
	LOGD("aed_combo_exception\n");
	if (g_aee_api) {
		if (g_aee_api->combo_exception) {
			g_aee_api->combo_exception("combo", log, log_size, phy, phy_size, detail,
						   db_opt);
		} else {
			LOGD("g_aee_api->combo_exception = 0x%p\n", g_aee_api->combo_exception);
		}
	} else {
		LOGD("g_aee_api is null\n");
	}
	LOGD("aed_combo_exception out\n");
#endif
}
EXPORT_SYMBOL(aed_combo_exception_api);

char sram_printk_buf[256];

void ram_console_write(struct console *console, const char *s, unsigned int count);

void aee_sram_printk(const char *fmt, ...)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	unsigned long long t;
	unsigned long nanosec_rem;
	va_list args;
	int r, tlen;

	va_start(args, fmt);

	preempt_disable();
	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000);
	tlen = sprintf(sram_printk_buf, ">%5lu.%06lu< ", (unsigned long)t, nanosec_rem / 1000);

	r = vsnprintf(sram_printk_buf + tlen, sizeof(sram_printk_buf) - tlen, fmt, args);

	ram_console_write(NULL, sram_printk_buf, r + tlen);
	preempt_enable();
	va_end(args);
#endif
}
EXPORT_SYMBOL(aee_sram_printk);

static int __init aee_common_init(void)
{
	int ret = 0;
	return ret;
}

static void __exit aee_common_exit(void)
{
}
module_init(aee_common_init);
module_exit(aee_common_exit);
