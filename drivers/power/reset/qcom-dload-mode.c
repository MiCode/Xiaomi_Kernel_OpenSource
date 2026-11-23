// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/panic_notifier.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <soc/qcom/minidump.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>

enum qcom_download_dest {
	QCOM_DOWNLOAD_DEST_UNKNOWN = -1,
	QCOM_DOWNLOAD_DEST_QPST = 0,
	QCOM_DOWNLOAD_DEST_EMMC = 2,
};

struct reg_info {
	struct regulator *reg;
	int uV;
	int uA;
};

struct qcom_dload {
	struct notifier_block panic_nb;
	struct notifier_block reboot_nb;
	struct kobject kobj;

	bool in_panic;
	bool in_reboot_edl;
	void __iomem *dload_dest_addr;

	struct regulator *mmcx_supply;
	struct reg_info *regs;
	int reg_cnt;
	struct device *dev;
};

#define to_qcom_dload(o) container_of(o, struct qcom_dload, kobj)

#define QCOM_DOWNLOAD_BOTHDUMP (QCOM_DOWNLOAD_FULLDUMP | QCOM_DOWNLOAD_MINIDUMP)

static int enable_dump =
	IS_ENABLED(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE_DEFAULT);
static enum qcom_download_mode current_download_mode = QCOM_DOWNLOAD_NODUMP;
static enum qcom_download_mode dump_mode = QCOM_DOWNLOAD_BOTHDUMP;

static int set_download_mode(enum qcom_download_mode mode)
{
	if ((mode & QCOM_DOWNLOAD_MINIDUMP) && !msm_minidump_enabled()) {
		mode &= ~QCOM_DOWNLOAD_MINIDUMP;
		pr_warn("Minidump not enabled.\n");
		if (!mode)
			return -ENODEV;
	}
	current_download_mode = mode;
	qcom_scm_set_download_mode(mode);
	return 0;
}

static int set_dump_mode(enum qcom_download_mode mode)
{
	int ret = 0, temp;

	if (enable_dump) {
		ret = set_download_mode(mode);
		if (likely(!ret))
			dump_mode = qcom_scm_get_download_mode(&temp) ? dump_mode : temp;
	} else
		dump_mode = mode;

	if (dump_mode != mode)
		pr_err("Requested dload mode is not set\n");

	return ret;
}

static void msm_enable_dump_mode(bool enable)
{
	if (enable)
		set_download_mode(dump_mode);
	else
		set_download_mode(QCOM_DOWNLOAD_NODUMP);
}

static void set_download_dest(struct qcom_dload *poweroff,
			      enum qcom_download_dest dest)
{
	if (poweroff->dload_dest_addr)
		__raw_writel(dest, poweroff->dload_dest_addr);
}
static enum qcom_download_dest get_download_dest(struct qcom_dload *poweroff)
{
	if (poweroff->dload_dest_addr)
		return __raw_readl(poweroff->dload_dest_addr);
	else
		return QCOM_DOWNLOAD_DEST_UNKNOWN;
}

static int param_set_download_mode(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	/* update enable_dump according to user input */
	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	msm_enable_dump_mode(enable_dump);
	if (!enable_dump)
		qcom_scm_disable_sdi();

	return 0;
}
module_param_call(download_mode, param_set_download_mode, param_get_int,
			&enable_dump, 0644);

/* interface for exporting attributes */
struct reset_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};
#define to_reset_attr(_attr) \
	container_of(_attr, struct reset_attribute, attr)

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->show)
		ret = reset_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->store)
		ret = reset_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops reset_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static const struct kobj_type qcom_dload_kobj_type = {
	.sysfs_ops	= &reset_sysfs_ops,
};

static ssize_t emmc_dload_show(struct kobject *kobj,
			       struct attribute *this,
			       char *buf)
{
	struct qcom_dload *poweroff = to_qcom_dload(kobj);

	if (!poweroff->dload_dest_addr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			get_download_dest(poweroff) == QCOM_DOWNLOAD_DEST_EMMC);
}
static ssize_t emmc_dload_store(struct kobject *kobj,
				struct attribute *this,
				const char *buf, size_t count)
{
	int ret;
	bool enabled;
	struct qcom_dload *poweroff = to_qcom_dload(kobj);

	if (!poweroff->dload_dest_addr)
		return -ENODEV;

	ret = kstrtobool(buf, &enabled);

	if (ret < 0)
		return ret;

	if (enabled)
		set_download_dest(poweroff, QCOM_DOWNLOAD_DEST_EMMC);
	else
		set_download_dest(poweroff, QCOM_DOWNLOAD_DEST_QPST);

	return count;
}
static struct reset_attribute attr_emmc_dload = __ATTR_RW(emmc_dload);

static ssize_t dload_mode_show(struct kobject *kobj,
			       struct attribute *this,
			       char *buf)
{
	const char *mode;

	switch ((unsigned int)dump_mode) {
	case QCOM_DOWNLOAD_FULLDUMP:
		mode = "full";
		break;
	case QCOM_DOWNLOAD_MINIDUMP:
		mode = "mini";
		break;
	case QCOM_DOWNLOAD_BOTHDUMP:
		mode = "both";
		break;
	default:
		mode = "unknown";
		break;
	}
	return scnprintf(buf, PAGE_SIZE, "DLOAD dump type: %s\n", mode);
}
static ssize_t dload_mode_store(struct kobject *kobj,
				struct attribute *this,
				const char *buf, size_t count)
{
	enum qcom_download_mode mode;

	if (sysfs_streq(buf, "full"))
		mode = QCOM_DOWNLOAD_FULLDUMP;
	else if (sysfs_streq(buf, "mini"))
		mode = QCOM_DOWNLOAD_MINIDUMP;
	else if (sysfs_streq(buf, "both"))
		mode = QCOM_DOWNLOAD_BOTHDUMP;
	else {
		pr_err("Invalid dump mode request...\n");
		pr_err("Supported dumps: 'full', 'mini', or 'both'\n");
		return -EINVAL;
	}

	return set_dump_mode(mode) ? : count;
}
static struct reset_attribute attr_dload_mode = __ATTR_RW(dload_mode);

static struct attribute *qcom_dload_attrs[] = {
	&attr_emmc_dload.attr,
	&attr_dload_mode.attr,
	NULL
};
static struct attribute_group qcom_dload_attr_group = {
	.attrs = qcom_dload_attrs,
};

static int poweroff_init_regulator(struct qcom_dload *poweroff)
{
	int len;
	int i, rc;
	char uv_ua[50];
	u32 uv_ua_vals[2];
	const char *reg_name;

	poweroff->reg_cnt = of_property_count_strings(poweroff->dev->of_node,
						  "reg-names");
	if (poweroff->reg_cnt <= 0) {
		pr_debug("poweroff: No regulators added!\n");
		return 0;
	}

	poweroff->regs = devm_kzalloc(poweroff->dev,
				  sizeof(struct reg_info) * poweroff->reg_cnt,
				  GFP_KERNEL);
	if (!poweroff->regs)
		return -ENOMEM;

	for (i = 0; i < poweroff->reg_cnt; i++) {
		of_property_read_string_index(poweroff->dev->of_node, "reg-names",
					      i, &reg_name);

		poweroff->regs[i].reg = devm_regulator_get(poweroff->dev, reg_name);
		if (IS_ERR(poweroff->regs[i].reg)) {
			pr_err("poweroff: failed to get %s reg\n", reg_name);
			return PTR_ERR(poweroff->regs[i].reg);
		}

		/* Read current(uA) and voltage(uV) value */
		snprintf(uv_ua, sizeof(uv_ua), "%s-uV-uA", reg_name);
		if (!of_find_property(poweroff->dev->of_node, uv_ua, &len))
			continue;

		rc = of_property_read_u32_array(poweroff->dev->of_node, uv_ua,
						uv_ua_vals,
						ARRAY_SIZE(uv_ua_vals));
		if (rc) {
			pr_err("poweroff: Failed to read uVuA value(rc:%d)\n", rc);
			return rc;
		}

		if (uv_ua_vals[0] > 0)
			poweroff->regs[i].uV = uv_ua_vals[0];
		if (uv_ua_vals[1] > 0)
			poweroff->regs[i].uA = uv_ua_vals[1];
	}
	return 0;
}


static int qcom_dload_panic(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     panic_nb);
	poweroff->in_panic = true;
	if (enable_dump)
		msm_enable_dump_mode(true);
	return NOTIFY_OK;
}

static int qcom_dload_reboot(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     reboot_nb);
	/* Clean shutdown, disable dump mode to allow normal restart */
	if (!poweroff->in_panic)
		set_download_mode(QCOM_DOWNLOAD_NODUMP);

	if (current_download_mode != QCOM_DOWNLOAD_NODUMP)
		reboot_mode = REBOOT_WARM;

	return NOTIFY_OK;
}

static void __iomem *map_prop_mem(const char *propname)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, propname);
	void __iomem *addr;

	if (!np) {
		pr_err("Unable to find DT property: %s\n", propname);
		return NULL;
	}

	addr = of_iomap(np, 0);
	if (!addr)
		pr_err("Unable to map memory for DT property: %s\n", propname);
	return addr;
}

static int qcom_dload_probe(struct platform_device *pdev)
{
	struct qcom_dload *poweroff;
	int ret, temp;

	if (IS_ENABLED(CONFIG_QCOM_MINIDUMP) && !msm_minidump_enabled())
		return -EPROBE_DEFER;

	poweroff = devm_kzalloc(&pdev->dev, sizeof(*poweroff), GFP_KERNEL);
	if (!poweroff)
		return -ENOMEM;

	poweroff->dev = &pdev->dev;
	ret = kobject_init_and_add(&poweroff->kobj, &qcom_dload_kobj_type,
				   kernel_kobj, "dload");
	if (ret) {
		pr_err("%s: Error in creation kobject_add\n", __func__);
		kobject_put(&poweroff->kobj);
		return ret;
	}

	ret = sysfs_create_group(&poweroff->kobj, &qcom_dload_attr_group);
	if (ret) {
		pr_err("%s: Error in creation sysfs_create_group\n", __func__);
		kobject_del(&poweroff->kobj);
		return ret;
	}

	poweroff->dload_dest_addr = map_prop_mem("qcom,msm-imem-dload-type");
	msm_enable_dump_mode(enable_dump);
	dump_mode = qcom_scm_get_download_mode(&temp) ? dump_mode : temp;
	pr_info("%s: Current dump mode: 0x%x\n", __func__, dump_mode);

	if (!enable_dump)
		qcom_scm_disable_sdi();

	poweroff->panic_nb.notifier_call = qcom_dload_panic;
	poweroff->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &poweroff->panic_nb);

	poweroff->reboot_nb.notifier_call = qcom_dload_reboot;
	poweroff->reboot_nb.priority = 255;
	register_reboot_notifier(&poweroff->reboot_nb);

	platform_set_drvdata(pdev, poweroff);
	ret = poweroff_init_regulator(poweroff);
	if (ret)
		pr_err("poweroff_init_regulator failed.\n");

	return 0;
}

static void qcom_dload_remove(struct platform_device *pdev)
{
	struct qcom_dload *poweroff = platform_get_drvdata(pdev);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &poweroff->panic_nb);
	unregister_reboot_notifier(&poweroff->reboot_nb);

	if (poweroff->dload_dest_addr)
		iounmap(poweroff->dload_dest_addr);
}

static const struct platform_device_id qcom_dload_id_match[] = {
	{ .name = "qcom-dload-mode", },
	{},
};
MODULE_DEVICE_TABLE(platform, qcom_dload_id_match);

static const struct of_device_id of_qcom_dload_match[] = {
	{ .compatible = "qcom,dload-mode" },
	{}
};
MODULE_DEVICE_TABLE(of, of_qcom_dload_match);

static struct platform_driver qcom_dload_driver = {
	.probe = qcom_dload_probe,
	.remove = qcom_dload_remove,
	.driver = {
		.name = "qcom-dload-mode",
		.of_match_table = of_match_ptr(of_qcom_dload_match),
	},
	.id_table = qcom_dload_id_match,
};

static int __init qcom_dload_driver_init(void)
{
	return platform_driver_register(&qcom_dload_driver);
}
#if IS_MODULE(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE)
module_init(qcom_dload_driver_init);
#else
fs_initcall(qcom_dload_driver_init);
#endif

static void __exit qcom_dload_driver_exit(void)
{
	return platform_driver_unregister(&qcom_dload_driver);
}
module_exit(qcom_dload_driver_exit);

MODULE_DESCRIPTION("MSM Download Mode Driver");
MODULE_LICENSE("GPL");
