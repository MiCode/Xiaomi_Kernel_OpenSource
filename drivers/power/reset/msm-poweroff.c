// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/of_address.h>

#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/memory.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/restart.h>
#include <soc/qcom/watchdog.h>
#include <soc/qcom/minidump.h>

#define EMERGENCY_DLOAD_MAGIC1    0x322A4F99
#define EMERGENCY_DLOAD_MAGIC2    0xC67E4350
#define EMERGENCY_DLOAD_MAGIC3    0x77777777
#define EMMC_DLOAD_TYPE		0x2

#define SCM_IO_DISABLE_PMIC_ARBITER	1
#define SCM_IO_DEASSERT_PS_HOLD		2
#define SCM_WDOG_DEBUG_BOOT_PART	0x9
#define SCM_DLOAD_FULLDUMP		0X10
#define SCM_EDLOAD_MODE			0X01
#define SCM_DLOAD_CMD			0x10
#define SCM_DLOAD_MINIDUMP		0X40
#define SCM_DLOAD_BOTHDUMPS	(SCM_DLOAD_MINIDUMP | SCM_DLOAD_FULLDUMP)

#define DL_MODE_PROP "qcom,msm-imem-download_mode"
#define EDL_MODE_PROP "qcom,msm-imem-emergency_download_mode"
#define IMEM_DL_TYPE_PROP "qcom,msm-imem-dload-type"

#define KASLR_OFFSET_PROP "qcom,msm-imem-kaslr_offset"
#define KASLR_OFFSET_BIT_MASK	0x00000000FFFFFFFF
#define DISPLAY_CONFIG_OFFSET_PROP "qcom,msm-imem-display_config_offset"

static int restart_mode;
static void *restart_reason, *dload_type_addr;
static bool scm_pmic_arbiter_disable_supported;
static bool scm_deassert_ps_hold_supported;
/* Download mode master kill-switch */
static void __iomem *msm_ps_hold;
static phys_addr_t tcsr_boot_misc_detect;
static void scm_disable_sdi(void);

/*
 * Runtime could be only changed value once.
 * There is no API from TZ to re-enable the registers.
 * So the SDI cannot be re-enabled when it already by-passed.
 */
static int download_mode = 1;
static struct kobject dload_kobj;

int in_panic = 0;
static int dload_type = SCM_DLOAD_BOTHDUMPS;
static void *dload_mode_addr;
static bool dload_mode_enabled;
static void *emergency_dload_mode_addr;
static bool scm_dload_supported;

static bool force_warm_reboot;

/* interface for exporting attributes */
struct reset_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	size_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};
#define to_reset_attr(_attr) \
	container_of(_attr, struct reset_attribute, attr)
#define RESET_ATTR(_name, _mode, _show, _store)	\
	static struct reset_attribute reset_attr_##_name = \
			__ATTR(_name, _mode, _show, _store)

/* sysfs related globals */
static ssize_t show_emmc_dload(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static size_t store_emmc_dload(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
RESET_ATTR(emmc_dload, 0644, show_emmc_dload, store_emmc_dload);

#ifdef CONFIG_QCOM_MINIDUMP
static ssize_t show_dload_mode(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static size_t store_dload_mode(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
RESET_ATTR(dload_mode, 0644, show_dload_mode, store_dload_mode);
#endif /* CONFIG_QCOM_MINIDUMP */

static struct attribute *reset_attrs[] = {
	&reset_attr_emmc_dload.attr,
#ifdef CONFIG_QCOM_MINIDUMP
	&reset_attr_dload_mode.attr,
#endif
	NULL
};

static struct attribute_group reset_attr_group = {
	.attrs = reset_attrs,
};

static int dload_set(const char *val, const struct kernel_param *kp);
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
static const struct sysfs_ops reset_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type reset_ktype = {
	.sysfs_ops	= &reset_sysfs_ops,
};

static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

int scm_set_dload_mode(int arg1, int arg2)
{
	struct scm_desc desc = {
		.args[0] = arg1,
		.args[1] = arg2,
		.arginfo = SCM_ARGS(2),
	};

	if (!scm_dload_supported) {
		if (tcsr_boot_misc_detect)
			return scm_io_write(tcsr_boot_misc_detect, arg1);

		return 0;
	}

	return scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT, SCM_DLOAD_CMD),
				&desc);
}

static void set_dload_mode(int on)
{
	int ret;

	if (dload_mode_addr) {
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
		/* Make sure the download cookie is updated */
		mb();
	}

	ret = scm_set_dload_mode(on ? dload_type : 0, 0);
	if (ret)
		pr_err("Failed to set secure DLOAD mode: %d\n", ret);

	dload_mode_enabled = on;
}

static bool get_dload_mode(void)
{
	return dload_mode_enabled;
}

static void enable_emergency_dload_mode(void)
{
	int ret;

	if (emergency_dload_mode_addr) {
		__raw_writel(EMERGENCY_DLOAD_MAGIC1,
				emergency_dload_mode_addr);
		__raw_writel(EMERGENCY_DLOAD_MAGIC2,
				emergency_dload_mode_addr +
				sizeof(unsigned int));
		__raw_writel(EMERGENCY_DLOAD_MAGIC3,
				emergency_dload_mode_addr +
				(2 * sizeof(unsigned int)));

		/* Need disable the pmic wdt, then the emergency dload mode
		 * will not auto reset.
		 */
		qpnp_pon_wd_config(0);
		/* Make sure all the cookied are flushed to memory */
		mb();
	}

	ret = scm_set_dload_mode(SCM_EDLOAD_MODE, 0);
	if (ret)
		pr_err("Failed to set secure EDLOAD mode: %d\n", ret);
}

static int dload_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);

	return 0;
}

static void free_dload_mode_mem(void)
{
	iounmap(emergency_dload_mode_addr);
	iounmap(dload_mode_addr);
}

static void *map_prop_mem(const char *propname)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, propname);
	void *addr;

	if (!np) {
		pr_err("Unable to find DT property: %s\n", propname);
		return NULL;
	}

	addr = of_iomap(np, 0);
	if (!addr)
		pr_err("Unable to map memory for DT property: %s\n", propname);

	return addr;
}

#ifdef CONFIG_RANDOMIZE_BASE
static void *kaslr_imem_addr;
static void store_kaslr_offset(void)
{
	kaslr_imem_addr = map_prop_mem(KASLR_OFFSET_PROP);

	if (kaslr_imem_addr) {
		__raw_writel(0xdead4ead, kaslr_imem_addr);
		__raw_writel(KASLR_OFFSET_BIT_MASK &
		(kimage_vaddr - KIMAGE_VADDR), kaslr_imem_addr + 4);
		__raw_writel(KASLR_OFFSET_BIT_MASK &
			((kimage_vaddr - KIMAGE_VADDR) >> 32),
			kaslr_imem_addr + 8);
		iounmap(kaslr_imem_addr);
	}
}
#else
static void store_kaslr_offset(void)
{
}
#endif /* CONFIG_RANDOMIZE_BASE */

/*
 * set display config imem first 4 bytes to 0xdead4ead, because imem context
 * will not lost when warm reset. if panic, xbl ramdump will display orange
 * screen, and framebuffer addr is determined by these four bytes in
 * MDP_GetDisplayBootConfig function. so set these four bytes to a invalid
 * value and let the framebuffer of orange screen use
 * RAMDUMP_FRAME_BUFFER_ADDRESS(0xb0400000)
 */
static void clear_display_config(void)
{
	void *display_config_imem_addr = map_prop_mem(DISPLAY_CONFIG_OFFSET_PROP);

	if (display_config_imem_addr) {
		__raw_writel(0xdead4ead, display_config_imem_addr);
		iounmap(display_config_imem_addr);
	}
	pr_err("%s clear display config\n", __func__);
}

static void setup_dload_mode_support(void)
{
	int ret;

	if (scm_is_call_available(SCM_SVC_BOOT, SCM_DLOAD_CMD) > 0)
		scm_dload_supported = true;

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);

	dload_mode_addr = map_prop_mem(DL_MODE_PROP);

	emergency_dload_mode_addr = map_prop_mem(EDL_MODE_PROP);

	store_kaslr_offset();
	clear_display_config();

	dload_type_addr = map_prop_mem(IMEM_DL_TYPE_PROP);
	if (!dload_type_addr)
		return;

	ret = kobject_init_and_add(&dload_kobj, &reset_ktype,
			kernel_kobj, "%s", "dload");
	if (ret) {
		pr_err("%s:Error in creation kobject_add\n", __func__);
		kobject_put(&dload_kobj);
		return;
	}

	ret = sysfs_create_group(&dload_kobj, &reset_attr_group);
	if (ret) {
		pr_err("%s:Error in creation sysfs_create_group\n", __func__);
		kobject_del(&dload_kobj);
	}
}

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

static ssize_t show_emmc_dload(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	uint32_t read_val, show_val;

	if (!dload_type_addr)
		return -ENODEV;

	read_val = __raw_readl(dload_type_addr);
	if (read_val == EMMC_DLOAD_TYPE)
		show_val = 1;
	else
		show_val = 0;

	return snprintf(buf, sizeof(show_val), "%u\n", show_val);
}

static size_t store_emmc_dload(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	uint32_t enabled;
	int ret;

	if (!dload_type_addr)
		return -ENODEV;

	ret = kstrtouint(buf, 0, &enabled);
	if (ret < 0)
		return ret;

	if (!((enabled == 0) || (enabled == 1)))
		return -EINVAL;

	if (enabled == 1)
		__raw_writel(EMMC_DLOAD_TYPE, dload_type_addr);
	else
		__raw_writel(0, dload_type_addr);

	return count;
}

#ifdef CONFIG_QCOM_MINIDUMP
static DEFINE_MUTEX(tcsr_lock);

static ssize_t show_dload_mode(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "DLOAD dump type: %s\n",
		(dload_type == SCM_DLOAD_BOTHDUMPS) ? "both" :
		((dload_type == SCM_DLOAD_MINIDUMP) ? "mini" : "full"));
}

static size_t store_dload_mode(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	if (sysfs_streq(buf, "full")) {
		dload_type = SCM_DLOAD_FULLDUMP;
	} else if (sysfs_streq(buf, "mini")) {
		if (!msm_minidump_enabled()) {
			pr_err("Minidump is not enabled\n");
			return -ENODEV;
		}
		dload_type = SCM_DLOAD_MINIDUMP;
	} else if (sysfs_streq(buf, "both")) {
		if (!msm_minidump_enabled()) {
			pr_err("Minidump not enabled, setting fulldump only\n");
			dload_type = SCM_DLOAD_FULLDUMP;
			return count;
		}
		dload_type = SCM_DLOAD_BOTHDUMPS;
	} else {
		pr_err("Invalid Dump setup request..\n");
		pr_err("Supported dumps:'full', 'mini', or 'both'\n");
		return -EINVAL;
	}

	mutex_lock(&tcsr_lock);
	/*Overwrite TCSR reg*/
	set_dload_mode(dload_type);
	mutex_unlock(&tcsr_lock);
	return count;
}
#endif /* CONFIG_QCOM_MINIDUMP */

static void scm_disable_sdi(void)
{
	int ret;
	struct scm_desc desc = {
		.args[0] = 1,
		.args[1] = 0,
		.arginfo = SCM_ARGS(2),
	};

	/* Needed to bypass debug image on some chips */
	ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT,
			  SCM_WDOG_DEBUG_BOOT_PART), &desc);
	if (ret)
		pr_err("Failed to disable secure wdog debug: %d\n", ret);
}

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);

/*
 * Force the SPMI PMIC arbiter to shutdown so that no more SPMI transactions
 * are sent from the MSM to the PMIC.  This is required in order to avoid an
 * SPMI lockup on certain PMIC chips if PS_HOLD is lowered in the middle of
 * an SPMI transaction.
 */
static void halt_spmi_pmic_arbiter(void)
{
	struct scm_desc desc = {
		.args[0] = 0,
		.arginfo = SCM_ARGS(1),
	};

	if (scm_pmic_arbiter_disable_supported) {
		pr_crit("Calling SCM to disable SPMI PMIC arbiter\n");
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR,
				SCM_IO_DISABLE_PMIC_ARBITER), &desc);
	}
}

static void msm_restart_prepare(const char *cmd)
{
	bool need_warm_reset = false;
	/* Write download mode flags if we're panic'ing
	 * Write download mode flags if restart_mode says so
	 * Kill download mode if master-kill switch is set
	 */

	set_dload_mode(download_mode &&
			(in_panic || restart_mode == RESTART_DLOAD));

	if (qpnp_pon_check_hard_reset_stored()) {
		/* Set warm reset as true when device is in dload mode */
		if (get_dload_mode() ||
			((cmd != NULL && cmd[0] != '\0') &&
			!strcmp(cmd, "edl")))
			need_warm_reset = true;
	} else {
		need_warm_reset = (get_dload_mode() ||
				(cmd != NULL && cmd[0] != '\0'));
	}

	if (force_warm_reboot)
		pr_info("Forcing a warm reset of the system\n");

	/* Hard reset the PMIC unless memory contents must be maintained. */
	if (force_warm_reboot || need_warm_reset)
		qpnp_pon_system_pwr_off(PON_POWER_OFF_WARM_RESET);
	else
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);

	if (in_panic) {
		qpnp_pon_set_restart_reason(PON_RESTART_REASON_PANIC);
	} else if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_BOOTLOADER);
			__raw_writel(0x77665500, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_RECOVERY);
			__raw_writel(0x77665502, restart_reason);
		} else if (!strncmp(cmd, "exaid", 5)) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_EXAID);
			__raw_writel(0x77665502, restart_reason);
		} else if (!strcmp(cmd, "rtc")) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_RTC);
			__raw_writel(0x77665503, restart_reason);
		} else if (!strcmp(cmd, "dm-verity device corrupted")) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_DMVERITY_CORRUPTED);
			__raw_writel(0x77665508, restart_reason);
		} else if (!strcmp(cmd, "dm-verity enforcing")) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_DMVERITY_ENFORCE);
			__raw_writel(0x77665509, restart_reason);
		} else if (!strcmp(cmd, "keys clear")) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_KEYS_CLEAR);
			__raw_writel(0x7766550a, restart_reason);
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			int ret;

			ret = kstrtoul(cmd + 4, 16, &code);
			if (!ret)
				__raw_writel(0x6f656d00 | (code & 0xff),
					     restart_reason);
		} else if (!strncmp(cmd, "edl", 3)) {
			if (0)
			enable_emergency_dload_mode();
		} else {
			qpnp_pon_set_restart_reason(PON_RESTART_REASON_NORMAL);
			__raw_writel(0x77665501, restart_reason);
		}
	} else {
		qpnp_pon_set_restart_reason(PON_RESTART_REASON_NORMAL);
		__raw_writel(0x77665501, restart_reason);
	}

	flush_cache_all();

	/*outer_flush_all is not supported by 64bit kernel*/
#ifndef CONFIG_ARM64
	outer_flush_all();
#endif

}

/*
 * Deassert PS_HOLD to signal the PMIC that we are ready to power down or reset.
 * Do this by calling into the secure environment, if available, or by directly
 * writing to a hardware register.
 *
 * This function should never return.
 */
static void deassert_ps_hold(void)
{
	struct scm_desc desc = {
		.args[0] = 0,
		.arginfo = SCM_ARGS(1),
	};

	if (scm_deassert_ps_hold_supported) {
		/* This call will be available on ARMv8 only */
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR,
				 SCM_IO_DEASSERT_PS_HOLD), &desc);
	}

	/* Fall-through to the direct write in case the scm_call "returns" */
	__raw_writel(0, msm_ps_hold);
}

static void do_msm_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	pr_notice("Going down for restart now\n");

	msm_restart_prepare(cmd);

	/*
	 * Trigger a watchdog bite here and if this fails,
	 * device will take the usual restart path.
	 */
	if (WDOG_BITE_ON_PANIC && in_panic)
		msm_trigger_wdog_bite();

	scm_disable_sdi();
	halt_spmi_pmic_arbiter();
	deassert_ps_hold();

	msleep(10000);
}

static void do_msm_poweroff(void)
{
	pr_notice("Powering off the SoC\n");

	set_dload_mode(0);
	scm_disable_sdi();
	qpnp_pon_system_pwr_off(PON_POWER_OFF_SHUTDOWN);

	halt_spmi_pmic_arbiter();
	deassert_ps_hold();

	msleep(10000);
	pr_err("Powering off has failed\n");
}

static int msm_restart_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *mem;
	struct device_node *np;
	int ret = 0;

	setup_dload_mode_support();

	np = of_find_compatible_node(NULL, NULL,
				"qcom,msm-imem-restart_reason");
	if (!np) {
		pr_err("unable to find DT imem restart reason node\n");
	} else {
		restart_reason = of_iomap(np, 0);
		if (!restart_reason) {
			pr_err("unable to map imem restart reason offset\n");
			ret = -ENOMEM;
			goto err_restart_reason;
		}
	}

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pshold-base");
	msm_ps_hold = devm_ioremap_resource(dev, mem);
	if (IS_ERR(msm_ps_hold))
		return PTR_ERR(msm_ps_hold);

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "tcsr-boot-misc-detect");
	if (mem)
		tcsr_boot_misc_detect = mem->start;

	pm_power_off = do_msm_poweroff;
	arm_pm_restart = do_msm_restart;

	if (scm_is_call_available(SCM_SVC_PWR, SCM_IO_DISABLE_PMIC_ARBITER) > 0)
		scm_pmic_arbiter_disable_supported = true;

	if (scm_is_call_available(SCM_SVC_PWR, SCM_IO_DEASSERT_PS_HOLD) > 0)
		scm_deassert_ps_hold_supported = true;

	set_dload_mode(download_mode);
	if (!download_mode)
		scm_disable_sdi();

	force_warm_reboot = of_property_read_bool(dev->of_node,
						"qcom,force-warm-reboot");

	return 0;

err_restart_reason:
	free_dload_mode_mem();
	return ret;
}

static const struct of_device_id of_msm_restart_match[] = {
	{ .compatible = "qcom,pshold", },
	{},
};
MODULE_DEVICE_TABLE(of, of_msm_restart_match);

static struct platform_driver msm_restart_driver = {
	.probe = msm_restart_probe,
	.driver = {
		.name = "msm-restart",
		.of_match_table = of_match_ptr(of_msm_restart_match),
	},
};

static int __init msm_restart_init(void)
{
	return platform_driver_register(&msm_restart_driver);
}
pure_initcall(msm_restart_init);
