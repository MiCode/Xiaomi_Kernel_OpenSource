/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <soc/qcom/scm.h>

#define SCM_KRYO_ERRATA_ID	0x12
#define ERRATA_WA_DISABLE	0
#define ERRATA_WA_ENABLE	1
#define ERRATA_74_75_ID_BIT	0x000
#define ERRATA_76_ID_BIT	0x100

static struct dentry *debugfs_base;
static bool kryo_e74_e75_wa = true;
static bool kryo_e76_wa;

static void kryo_e74_e75_scm(void *enable)
{
	int ret;
	struct scm_desc desc = {0};

	if (!is_scm_armv8())
		return;

	desc.arginfo = SCM_ARGS(1);
	desc.args[0] = enable ? ERRATA_WA_ENABLE : ERRATA_WA_DISABLE;
	desc.args[0] |= ERRATA_74_75_ID_BIT;

	ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT, SCM_KRYO_ERRATA_ID),
				&desc);
	if (ret)
		pr_err("Failed to %s ERRATA_74_75 workaround\n",
			enable ? "enable" : "disable");
}

static int kryo_e74_e75_set(void *data, u64 val)
{
	if ((val && kryo_e74_e75_wa) || (!val && !kryo_e74_e75_wa))
		return 0;

	kryo_e74_e75_wa = !!val;

	get_cpu();
	on_each_cpu(kryo_e74_e75_scm, (void *)kryo_e74_e75_wa, 1);
	put_cpu();

	return 0;
}

static int kryo_e74_e75_get(void *data, u64 *val)
{
	*val = kryo_e74_e75_wa;
	return 0;
}

static void kryo_e76_scm(void *enable)
{
	int ret;
	struct scm_desc desc = {0};

	if (!is_scm_armv8())
		return;

	desc.arginfo = SCM_ARGS(1);
	desc.args[0] = enable ? ERRATA_WA_ENABLE : ERRATA_WA_DISABLE;
	desc.args[0] |= ERRATA_76_ID_BIT;

	ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT, SCM_KRYO_ERRATA_ID),
				&desc);
	if (ret)
		pr_err("Failed to %s ERRATA_76 workaround\n",
			enable ? "enable" : "disable");
}

static int kryo_e76_set(void *data, u64 val)
{
	if ((val && kryo_e76_wa) || (!val && !kryo_e76_wa))
		return 0;

	kryo_e76_wa = !!val;

	get_cpu();
	on_each_cpu(kryo_e76_scm, (void *)kryo_e76_wa, 1);
	put_cpu();

	return 0;
}

static int kryo_e76_get(void *data, u64 *val)
{
	*val = kryo_e76_wa;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(kryo_e74_e75_fops, kryo_e74_e75_get,
			kryo_e74_e75_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(kryo_e76_fops, kryo_e76_get,
			kryo_e76_set, "%llu\n");

static int scm_errata_notifier_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		kryo_e74_e75_scm((void *)kryo_e74_e75_wa);
		kryo_e76_scm((void *)kryo_e76_wa);
		break;
	}
	return 0;
}

static struct notifier_block scm_errata_notifier = {
	.notifier_call = scm_errata_notifier_callback,
};

static int __init scm_errata_init(void)
{
	int ret;

	debugfs_base = debugfs_create_dir("scm_errata", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("kryo_e74_e75", S_IRUGO | S_IWUSR,
			debugfs_base, NULL, &kryo_e74_e75_fops))
		goto err;
	if (!debugfs_create_file("kryo_e76", S_IRUGO | S_IWUSR,
			debugfs_base, NULL, &kryo_e76_fops))
		goto err;
	ret = register_hotcpu_notifier(&scm_errata_notifier);
	if (ret)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(debugfs_base);
	return ret;
}
device_initcall(scm_errata_init);
