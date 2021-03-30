#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/signal.h>
#include "millet.h"

#include "../../kernel/cgroup/cgrp_oem.h"

extern int millet_can_attach(struct cgroup_taskset *tset);

struct cgroup_subsys oem_cgroup_hook[CGRP_SUBSYS_NUM];

static __init int oem_cgrp_init(void)
{
	oem_cgroup_hook[FREERE_SUBSYS].can_attach = millet_can_attach;

	return 0;
}

module_init(oem_cgrp_init);

MODULE_LICENSE("GPL");

