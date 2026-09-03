// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "printk_cpuid: " fmt
#include <linux/module.h>
#include <trace/hooks/printk.h>

#define CALLER_ENCODED		(1 << 30)
#define CALLER_THREAD_MASK	0x3FFFFF
#define CALLER_THREAD_SHIFT	22
#define CALLER_CPUID_MASK	0xF

static void encode_caller_id(void *a, u32 *caller_id)
{
	if (in_task())
		*caller_id = CALLER_ENCODED | task_pid_nr(current) |
			raw_smp_processor_id() << CALLER_THREAD_SHIFT;
}

static void
decode_caller_id(void *a, char *caller, size_t size, u32 id, int *ret)
{
	if (likely(id & CALLER_ENCODED))
		snprintf(caller, size, "T%u@C%u", id & CALLER_THREAD_MASK,
			 id >> CALLER_THREAD_SHIFT & CALLER_CPUID_MASK);
	else
		snprintf(caller, size, "%c%u",
			 id & 0x80000000 ? 'C' : 'T', id & ~0x80000000);

	*ret = 1;
}

static void
print_ext_header(void *a, char *caller, size_t size, u32 id, int *ret)
{
	if (likely(id & CALLER_ENCODED))
		snprintf(caller, size, ",caller=T%u@C%u", id & CALLER_THREAD_MASK,
			 id >> CALLER_THREAD_SHIFT & CALLER_CPUID_MASK);
	else
		snprintf(caller, size, ",caller=%c%u",
			 id & 0x80000000 ? 'C' : 'T', id & ~0x80000000);

	*ret = 1;
}

static int __init printk_cpuid_init(void)
{

	if (register_trace_android_vh_printk_caller_id(encode_caller_id, NULL))
		goto fail1;
	if (register_trace_android_vh_printk_caller(decode_caller_id, NULL))
		goto fail2;
	if (register_trace_android_vh_printk_ext_header(print_ext_header, NULL))
		goto fail3;

	pr_info("add cpu ID info to kernel log\n");
	return 0;

fail3:
	unregister_trace_android_vh_printk_caller(decode_caller_id, NULL);
fail2:
	unregister_trace_android_vh_printk_caller_id(encode_caller_id, NULL);
fail1:
	pr_err("failed to register vendor hooks\n");
	return -1;
}

static void __exit printk_cpuid_exit(void)
{

	unregister_trace_android_vh_printk_caller_id(encode_caller_id, NULL);
	unregister_trace_android_vh_printk_caller(decode_caller_id, NULL);
	unregister_trace_android_vh_printk_ext_header(print_ext_header, NULL);

	pr_err("module exited, above logs' caller will be wrong!\n");
}

module_init(printk_cpuid_init);
module_exit(printk_cpuid_exit);

MODULE_AUTHOR("ben.dai@unisoc.com");
MODULE_LICENSE("GPL");
