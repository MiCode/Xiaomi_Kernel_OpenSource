#include <linux/module.h>
#include <linux/swap.h>
#include <trace/hooks/vmscan.h>

enum EXTEND_RECLAIM_TYPE {
	EXTEND_RECLAIM_TYPE_BASE = 0,
	EXTEND_RECLAIM_TYPE_FILE = EXTEND_RECLAIM_TYPE_BASE,
	EXTEND_RECLAIM_TYPE_ANON,
	EXTEND_RECLAIM_TYPE_ALL,
	EXTEND_RECLAIM_TYPE_COUNT
};

enum scan_balance {
	SCAN_EQUAL,
	SCAN_FRACT,
	SCAN_ANON,
	SCAN_FILE,
};

#define EXTEND_RECLIAM_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = \
	__ATTR(_name, 0644, _name##_show, _name##_store)

static struct kobject *kobj;
static char reclaim_type_str[EXTEND_RECLAIM_TYPE_COUNT][5] = {"file", "anon", "all"};
static int reclaim_type;
static int extend_reclaim_debug;

static ssize_t debug_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d", extend_reclaim_debug);
}
static ssize_t debug_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &extend_reclaim_debug) != 1) {
		pr_err("extend_reclaim: invalid input: %s", buf);
		return -EINVAL;
	}
	return count;
}

static ssize_t reclaim_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "reclaim_show");
}
static ssize_t reclaim_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{

	bool may_swap = false;
	unsigned long nr_to_reclaim, nr_reclaimed;

	if (sscanf(buf, "%d %ld", &reclaim_type, &nr_to_reclaim) != 2) {
		pr_err("extend_reclaim: invalid input: %s", buf);
		return -EINVAL;
	}

	if (reclaim_type >= EXTEND_RECLAIM_TYPE_COUNT || reclaim_type < EXTEND_RECLAIM_TYPE_BASE || nr_to_reclaim < 0) {
		pr_err("extend_reclaim: invalid input: reclaim type = %s, nr_to_reclaim = %ld", reclaim_type_str[reclaim_type], nr_to_reclaim);
		return -EINVAL;
	}

	switch(reclaim_type)
	{
		case EXTEND_RECLAIM_TYPE_FILE:
			break;
		case EXTEND_RECLAIM_TYPE_ANON:
		case EXTEND_RECLAIM_TYPE_ALL:
			may_swap = true;
			break;
		default:
			break;
	}
	nr_reclaimed = try_to_free_mem_cgroup_pages(NULL, nr_to_reclaim, GFP_KERNEL, may_swap);

	if (extend_reclaim_debug)
		pr_info("extend_reclaim: current->comm = %s, reclaim type = %s, nr_to_reclaim = %ld, nr_reclaimed = %ld",
			current->comm,
			reclaim_type_str[reclaim_type],
			nr_to_reclaim,
			nr_reclaimed);

	return count;
}
EXTEND_RECLIAM_ATTR_RW(reclaim);
EXTEND_RECLIAM_ATTR_RW(debug);

static struct attribute * extend_reclaim_attrs[] = {
	&reclaim_attr.attr,
	&debug_attr.attr,
	NULL
};

static const struct attribute_group extend_reclaim_attr_group = {
	.attrs = extend_reclaim_attrs,
};

static void extend_reclaim_tune_scan_type_handler(void *data, char *pscan_type) {
	if ((reclaim_type == EXTEND_RECLAIM_TYPE_ANON) && (!strcmp(current->comm, "extend_reclaim"))) {
		*pscan_type = SCAN_ANON;
	}
}

static int __init extend_reclaim_init(void)
{
	int error;

	kobj = kobject_create_and_add("extend_reclaim", kernel_kobj);
	if (!kobj) {
		error = -ENOMEM;
		goto exit;
	}

	error = sysfs_create_group(kobj, &extend_reclaim_attr_group);
	if (error)
		goto kset_exit;

	register_trace_android_vh_tune_scan_type(extend_reclaim_tune_scan_type_handler, NULL);

	pr_info("extend_reclaim: module init!");
	return 0;

kset_exit:
	kobject_put(kobj);
exit:
	pr_err("extend_reclaim module init failed error = %d!", error);
	return error;
}

static void __exit extend_reclaim_exit(void)
{
	if (kobj)
		kobject_put(kobj);

	unregister_trace_android_vh_tune_scan_type(extend_reclaim_tune_scan_type_handler, NULL);

	pr_info("extend_reclaim: module exit!");
}

module_init(extend_reclaim_init);
module_exit(extend_reclaim_exit);

MODULE_AUTHOR("chenzhiwei<chenzhiwei@xiaomi.com>");
MODULE_DESCRIPTION("global page reclaim interface");
MODULE_LICENSE("GPL");
