// MIUI ADD: Performance_FramePredictBoost
#include <linux/preempt.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>

#include "hyperframe_utils.h"

#define HYPERFRAME_DIR_NAME "hyperframe"

uint32_t hyperframe_systrace_mask;

static struct kobject *base_kobj;
static struct kobject *hyperframe_kobj;

#define GENERATE_STRING(name, unused) #name

#define NSEC_PER_HUSEC 100000

static const char * const mask_string[] = {
	HYPERFRAME_SYSTRACE_LIST(GENERATE_STRING)
};

static int hyperframe_update_tracemark(void) 
{
	return 1;
}

static noinline int tracing_mark_write(const char *buf)
{
	trace_puts(buf);
	return 0;
}

void __hyperframe_systrace_c(pid_t pid, unsigned long long val, const char *fmt, ...)
{
	char name[256];
	va_list args;
	int len;
	char buf[256];
	if (unlikely(!hyperframe_update_tracemark())) return;
	memset(name, ' ', sizeof(name));
	va_start(args, fmt);
	len = vsnprintf(name, sizeof(name), fmt, args);
	va_end(args);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		name[255] = '\0';
	len = snprintf(buf, sizeof(buf), "C|%d|[hyperframe] %s|%ld\n", pid, name, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';
	tracing_mark_write(buf);
}

void __hyperframe_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[256];
	char buf2[256];
	va_list args;
	int len;

	if (unlikely(!hyperframe_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "B|%d|%s\n", tgid, log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	tracing_mark_write(buf2);
}

void __hyperframe_systrace_e(void)
{
	char buf2[256];
	if (unlikely(!hyperframe_update_tracemark()))
		return;

	snprintf(buf2, sizeof(buf2), "E\n");
	tracing_mark_write(buf2);
}

static ssize_t hyperframe_systrace_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i;
	char temp[HYPERFRAME_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;
	length = scnprintf(temp + pos, HYPERFRAME_SYSFS_MAX_BUFF_SIZE - pos,
			" Current enabled systrace:\n");
	pos += length;
	for (i = 0; (1U << i) < HYPERFRAME_DEBUG_MAX; i++) {
		length = scnprintf(temp + pos, HYPERFRAME_SYSFS_MAX_BUFF_SIZE - pos,
			"  %-*s ... %s\n", 12, mask_string[i],
			hyperframe_systrace_mask & (1U << i) ?
			"On" : "Off");
		pos += length;
	}
	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t hyperframe_systrace_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t val = -1;
	char acBuffer[HYPERFRAME_SYSFS_MAX_BUFF_SIZE];
	uint32_t arg;
	if ((count > 0) && (count < HYPERFRAME_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, HYPERFRAME_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtou32(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}
	hyperframe_systrace_mask = val & (HYPERFRAME_DEBUG_MAX - 1U);
	return count;
}

static KOBJ_ATTR_RW(hyperframe_systrace_mask);

int hyperframe_create_dir(struct kobject *parent, const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;
	if (name == NULL || ppsKobj == NULL) {
		return -1;
	}
	parent = (parent != NULL) ? parent : hyperframe_kobj;
	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj) {
		return -1;
	}
	*ppsKobj = psKobj;
	return 0;
}

void hyperframe_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;
	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}

void hyperframe_create_file(struct kobject *parent, struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL) {
		return;
	}
	parent = (parent != NULL) ? parent : hyperframe_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		return;
	}
}

void hyperframe_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : hyperframe_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}

void hyperframe_filesystem_init(void)
{
	hyperframe_kobj = kobject_create_and_add(HYPERFRAME_DIR_NAME, kernel_kobj);
	if (!hyperframe_kobj) {
	}
}

void hyperframe_filesystem_exit(void)
{
	kobject_put(hyperframe_kobj);
	hyperframe_kobj = NULL;
}

int init_hyperframe_debug(void) {
	if (!hyperframe_create_dir(NULL, "common", &base_kobj)) {
		hyperframe_create_file(base_kobj, &kobj_attr_hyperframe_systrace_mask);
	}
	hyperframe_update_tracemark();
	hyperframe_systrace_mask = 0;
	return 0;
}

void idx_add(int* val, int base, int increase, int boundary)
{
	int tmp;

	if (base >= boundary)
		return;

	tmp = base;
	tmp = tmp + increase;
	if (tmp >= boundary)
		tmp = tmp - boundary;

	*val = tmp;
}

int idx_add_safe(int base, int increase, int boundary)
{
	int tmp;

	if (base >= boundary || increase > boundary)
		return base;

	tmp = base;
	tmp = tmp + increase;
	if (tmp >= boundary)
		tmp = tmp - boundary;

	return tmp;
}

void idx_sub(int* val, int base, int decrease, int boundary)
{
	int tmp;

	if (base < 0 || decrease < 0)
		return;

	tmp = base;
	tmp = tmp - decrease;
	if (tmp < 0)
		tmp = tmp + boundary;

	*val = tmp;
}

void idx_add_self(int* val, int increase, int boundary)
{
	int tmp;

	if (*val >= boundary)
		return;

	tmp = *val;
	tmp = tmp + increase;
	if (tmp >= boundary)
		tmp = tmp - boundary;

	*val = tmp;
}

void idx_sub_self(int* val, int decrease, int boundary)
{
	int tmp;

	if (*val < 0 || decrease < 0)
		return;

	tmp = *val;
	tmp = tmp - decrease;
	if (tmp < 0)
		tmp = tmp + boundary;

	*val = tmp;
}

int idx_sub_safe(int base, int decrease, int boundary)
{
	int tmp;

	if (base < 0 || decrease < 0 || decrease >= boundary)
		return base;

	tmp = base;
	tmp = tmp - decrease;
	if (tmp < 0)
		tmp = tmp + boundary;

	return tmp;
}

unsigned long long hyperframe_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}
// END Performance_FramePredictBoost