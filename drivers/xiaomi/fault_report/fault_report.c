
#include <trace/hooks/health_report.h>
#include <linux/string.h>
#include <linux/stacktrace.h>
#include <linux/sizes.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_MIEV)
#include "miev/mievent.h"
#endif

#define DFX_ID_F2FS_BUG_ON     910021001
#define DFX_ID_F2FS_NEED_FSCK  910021002
#define DFX_ID_F2FS_PAGE_EIO   910021003
#define DFX_ID_F2FS_STOP_CP    910021004
#define DFX_ID_F2FS_OTHER      910021005
#define MAX_STACK_TRACE_LEN    1024

enum {
	REPORT_FAULT_BUG_ON,
	REPORT_FAULT_NEED_FSCK,
	REPORT_FAULT_PAGE_EIO,
	REPORT_FAULT_STOP_CP,
	REPORT_FAULT_OTHER,
	REPORT_FAULT_MAX,
};

struct report_info_struct {
	const char *func;
	int line;
	char stack[MAX_STACK_TRACE_LEN];
};

struct report_work_struct {
	struct work_struct work;
	int miev_code;
	unsigned long entries[10];
	unsigned int nr_entries;
	struct report_info_struct report_info;
};

static void report_work_handler(struct work_struct *work)
{
	int i;
	int len = 0;
	struct report_work_struct *report_work;

	report_work = container_of(work, struct report_work_struct, work);
	len = snprintf(report_work->report_info.stack, MAX_STACK_TRACE_LEN, "Call trace:\n");
	for (i = 0; i < report_work->nr_entries && len < MAX_STACK_TRACE_LEN - 64; i++) {
		len += snprintf(report_work->report_info.stack + len, MAX_STACK_TRACE_LEN - len,
			"%pB\n", (void *)report_work->entries[i]);
	}

#if IS_ENABLED(CONFIG_MIEV)
	if (report_work->miev_code != 0 && report_work->report_info.func != NULL) {
		struct misight_mievent *event = cdev_tevent_alloc(report_work->miev_code);
		if (cdev_tevent_add_str(event, "func", report_work->report_info.func) != -1 &&
		    cdev_tevent_add_int(event, "line", report_work->report_info.line) != -1 &&
		    cdev_tevent_add_str(event, "stack", report_work->report_info.stack) != -1) {
		    cdev_tevent_write(event);
		}
		cdev_tevent_destroy(event);
	}
#endif

	kfree(report_work);
}

static void report_info_copy(void *data, unsigned int err_code, const char *func,
            unsigned int line, unsigned long *entries, unsigned int nr_entries)
{
	static const unsigned int err_code_to_miev[] = {
		[REPORT_FAULT_BUG_ON] = DFX_ID_F2FS_BUG_ON,
		[REPORT_FAULT_NEED_FSCK] = DFX_ID_F2FS_NEED_FSCK,
		[REPORT_FAULT_PAGE_EIO] = DFX_ID_F2FS_PAGE_EIO,
		[REPORT_FAULT_STOP_CP] = DFX_ID_F2FS_STOP_CP,
		[REPORT_FAULT_OTHER] = DFX_ID_F2FS_OTHER
	};
	struct report_work_struct *work;

	work = kmalloc(sizeof(struct report_work_struct), GFP_ATOMIC);
	if (!work) {
		pr_err("Failed to allocate work structure");
		return;
	}

	work->report_info.func = func;
	work->report_info.line = line;
	work->miev_code = err_code_to_miev[err_code];

	work->nr_entries = nr_entries < ARRAY_SIZE(work->entries) ?
				nr_entries : ARRAY_SIZE(work->entries);
	memcpy(work->entries, entries, work->nr_entries * sizeof(unsigned long));

	INIT_WORK(&work->work, report_work_handler);
	queue_work(system_wq, &work->work);

	return;
}

static int register_fault_report_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_health_report(report_info_copy, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_health_report failed! ret=%d\n", ret);
		goto out;
	}

out:
	return ret;
}

static void unregister_fault_report_hooks(void)
{
	unregister_trace_android_vh_health_report(report_info_copy, NULL);

	return;
}

static int __init fault_report_init(void)
{
	int ret = 0;

	ret = register_fault_report_hooks();
	if (ret != 0)
		return ret;

	return 0;
}

static void __exit fault_report_exit(void)
{
	unregister_fault_report_hooks();

	pr_info("fault_report exit succeed!\n");

	return;
}

module_init(fault_report_init);
module_exit(fault_report_exit);

MODULE_LICENSE("GPL v2");
