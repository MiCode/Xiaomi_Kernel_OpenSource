#define pr_fmt(fmt) "millet-oem_cgroup: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/signal.h>
#include <linux/cgroup.h>
#include <../../kernel/cgroup/cgroup-internal.h>

static __init int oem_cgrp_init(void)
{
	pr_err("enter oem_cgrp_init func!\n");
	return 0;
}

module_init(oem_cgrp_init);

MODULE_LICENSE("GPL");

