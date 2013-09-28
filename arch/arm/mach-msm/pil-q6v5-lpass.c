/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/of_gpio.h>

#include <mach/clk.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>
#include <mach/scm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>
#include <mach/msm_bus_board.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "scm-pas.h"
#include "sysmon.h"

#define QDSP6SS_RST_EVB			0x010
#define PROXY_TIMEOUT_MS		10000

static struct kobject *lpass_status;
static char status[32];

struct lpass_data {
	struct q6v5_data *q6;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void *ramdump_dev;
	struct work_struct work;
	void *wcnss_notif_hdle;
	void *modem_notif_hdle;
	int crash_shutdown;
};

#define subsys_to_drv(d) container_of(d, struct lpass_data, subsys_desc)

static int pil_lpass_enable_clks(struct q6v5_data *drv)
{
	int ret;

	ret = clk_reset(drv->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		goto err_reset;
	ret = clk_prepare_enable(drv->core_clk);
	if (ret)
		goto err_core_clk;
	ret = clk_prepare_enable(drv->ahb_clk);
	if (ret)
		goto err_ahb_clk;
	ret = clk_prepare_enable(drv->axi_clk);
	if (ret)
		goto err_axi_clk;
	ret = clk_prepare_enable(drv->reg_clk);
	if (ret)
		goto err_reg_clk;

	return 0;

err_reg_clk:
	clk_disable_unprepare(drv->axi_clk);
err_axi_clk:
	clk_disable_unprepare(drv->ahb_clk);
err_ahb_clk:
	clk_disable_unprepare(drv->core_clk);
err_core_clk:
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
err_reset:
	return ret;
}

static void pil_lpass_disable_clks(struct q6v5_data *drv)
{
	clk_disable_unprepare(drv->reg_clk);
	clk_disable_unprepare(drv->axi_clk);
	clk_disable_unprepare(drv->ahb_clk);
	clk_disable_unprepare(drv->core_clk);
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
}

static int pil_lpass_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base);

	/*
	 * If the shutdown function is called before the reset function, clocks
	 * will not be enabled yet. Enable them here so that register writes
	 * performed during the shutdown succeed.
	 */
	if (drv->is_booted == false)
		pil_lpass_enable_clks(drv);

	pil_q6v5_shutdown(pil);
	pil_lpass_disable_clks(drv);

	writel_relaxed(1, drv->restart_reg);

	drv->is_booted = false;

	return 0;
}

static int pil_lpass_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	phys_addr_t start_addr = pil_get_entry_addr(pil);
	int ret;

	/* Deassert reset to subsystem and wait for propagation */
	writel_relaxed(0, drv->restart_reg);
	mb();
	udelay(2);

	ret = pil_lpass_enable_clks(drv);
	if (ret)
		return ret;

	/* Program Image Address */
	writel_relaxed((start_addr >> 4) & 0x0FFFFFF0,
				drv->reg_base + QDSP6SS_RST_EVB);

	ret = pil_q6v5_reset(pil);
	if (ret) {
		pil_lpass_disable_clks(drv);
		return ret;
	}

	drv->is_booted = true;

	return 0;
}

static struct pil_reset_ops pil_lpass_ops = {
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_lpass_reset,
	.shutdown = pil_lpass_shutdown,
};

static int pil_lpass_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_Q6, metadata, size);
}

static int pil_lpass_mem_setup_trusted(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	return pas_mem_setup(PAS_Q6, addr, size);
}

static int pil_lpass_reset_trusted(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int ret;

	ret = clk_prepare_enable(drv->axi_clk);
	if (ret)
		return ret;
	return pas_auth_and_reset(PAS_Q6);
}

static int pil_lpass_shutdown_trusted(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int ret;

	ret = pas_shutdown(PAS_Q6);
	if (ret)
		return ret;
	clk_disable_unprepare(drv->axi_clk);
	return 0;
}

static struct pil_reset_ops pil_lpass_ops_trusted = {
	.init_image = pil_lpass_init_image_trusted,
	.mem_setup = pil_lpass_mem_setup_trusted,
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_lpass_reset_trusted,
	.shutdown = pil_lpass_shutdown_trusted,
};

static int wcnss_notifier_cb(struct notifier_block *this, unsigned long code,
								void *ss_handle)
{
	int ret;
	pr_debug("%s: W-Notify: event %lu\n", __func__, code);
	ret = sysmon_send_event(SYSMON_SS_LPASS, "wcnss", code);
	if (ret < 0)
		pr_err("%s: sysmon_send_event error %d", __func__, ret);
	return NOTIFY_DONE;
}

static struct notifier_block wnb = {
	.notifier_call = wcnss_notifier_cb,
};

static int modem_notifier_cb(struct notifier_block *this, unsigned long code,
								void *ss_handle)
{
	int ret;
	pr_debug("%s: M-Notify: event %lu\n", __func__, code);
	ret = sysmon_send_event(SYSMON_SS_LPASS, "modem", code);
	if (ret < 0)
		pr_err("%s: sysmon_send_event error %d", __func__, ret);
	return NOTIFY_DONE;
}

static struct notifier_block mnb = {
	.notifier_call = modem_notifier_cb,
};

static void adsp_log_failure_reason(void)
{
	char *reason;
	char buffer[81];
	unsigned size;

	reason = smem_get_entry(SMEM_SSR_REASON_LPASS0, &size);

	if (!reason) {
		pr_err("ADSP subsystem failure reason: (unknown, smem_get_entry failed).");
		return;
	}

	if (reason[0] == '\0') {
		pr_err("ADSP subsystem failure reason: (unknown, init value found)");
		return;
	}

	size = min(size, sizeof(buffer) - 1);
	memcpy(buffer, reason, size);
	buffer[size] = '\0';
	pr_err("ADSP subsystem failure reason: %s", buffer);
	memset((void *)reason, 0x0, size);
	wmb();
}

static void restart_adsp(struct lpass_data *drv)
{
	adsp_log_failure_reason();
	subsystem_restart_dev(drv->subsys);
}

static void adsp_fatal_fn(struct work_struct *work)
{
	struct lpass_data *drv = container_of(work, struct lpass_data, work);

	pr_err("Watchdog bite received from ADSP!\n");
	restart_adsp(drv);
}

static irqreturn_t adsp_err_fatal_intr_handler (int irq, void *dev_id)
{
	struct lpass_data *drv = subsys_to_drv(dev_id);

	/* Ignore if we're the one that set the force stop bit in the outbound
	 * entry
	 */
	if (drv->crash_shutdown)
		return IRQ_HANDLED;

	pr_err("Fatal error on the ADSP!\n");
	restart_adsp(drv);
	return IRQ_HANDLED;
}

#define SCM_Q6_NMI_CMD 0x1

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	scm_call_atomic1(SCM_SVC_UTIL, SCM_Q6_NMI_CMD, 0x1);
	pr_debug("%s: Q6 NMI was sent.\n", __func__);
}

/*
 * The "status" file where a static variable is read from and written to.
 */
static ssize_t adsp_state_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	return snprintf(buf, sizeof(status), "%s\n", status);
}

static struct kobj_attribute adsp_state_attribute =
	__ATTR(status, 0444, adsp_state_show, NULL);

static struct attribute *attrs[] = {
	&adsp_state_attribute.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static void adsp_set_state(char *state)
{
	strlcpy(status, state, sizeof(status));
	sysfs_notify(lpass_status, NULL, "status");
}

#define subsys_to_lpass(d) container_of(d, struct lpass_data, subsys_desc)

static int adsp_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct lpass_data *drv = subsys_to_lpass(subsys);

	if (force_stop) {
		send_q6_nmi();
		/* The write needs to go through before the q6 is shutdown. */
		mb();
	}
	pil_shutdown(&drv->q6->desc);
	disable_irq(drv->subsys_desc.wdog_bite_irq);

	pr_debug("ADSP is Down\n");
	adsp_set_state("OFFLINE");
	return 0;
}

static int adsp_powerup(const struct subsys_desc *subsys)
{
	struct lpass_data *drv = subsys_to_lpass(subsys);
	int ret = 0;
	ret = pil_boot(&drv->q6->desc);
	enable_irq(drv->subsys_desc.wdog_bite_irq);

	pr_debug("ADSP is back online\n");
	adsp_set_state("ONLINE");
	return ret;
}

static int adsp_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct lpass_data *drv = subsys_to_lpass(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->q6->desc, drv->ramdump_dev);
}

static void adsp_crash_shutdown(const struct subsys_desc *subsys)
{
	struct lpass_data *drv = subsys_to_lpass(subsys);

	drv->crash_shutdown = 1;
	gpio_set_value(subsys->force_stop_gpio, 1);
	send_q6_nmi();
}

static irqreturn_t adsp_wdog_bite_irq(int irq, void *dev_id)
{
	struct lpass_data *drv = subsys_to_drv(dev_id);

	schedule_work(&drv->work);
	return IRQ_HANDLED;
}

static int pil_lpass_driver_probe(struct platform_device *pdev)
{
	struct lpass_data *drv;
	struct q6v5_data *q6;
	struct pil_desc *desc;
	struct resource *res;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	q6 = pil_q6v5_init(pdev);
	if (IS_ERR(q6))
		return PTR_ERR(q6);
	drv->q6 = q6;

	desc = &q6->desc;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = PROXY_TIMEOUT_MS;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "restart_reg");
	q6->restart_reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!q6->restart_reg)
		return -ENOMEM;

	q6->core_clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(q6->core_clk))
		return PTR_ERR(q6->core_clk);

	q6->ahb_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(q6->ahb_clk))
		return PTR_ERR(q6->ahb_clk);

	q6->axi_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(q6->axi_clk))
		return PTR_ERR(q6->axi_clk);

	q6->reg_clk = devm_clk_get(&pdev->dev, "reg_clk");
	if (IS_ERR(q6->reg_clk))
		return PTR_ERR(q6->reg_clk);

	if (pas_supported(PAS_Q6) > 0) {
		desc->ops = &pil_lpass_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_lpass_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	scm_pas_init(MSM_BUS_MASTER_CRYPTO_CORE0);

	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->subsys_desc.name = desc->name;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.shutdown = adsp_shutdown;
	drv->subsys_desc.powerup = adsp_powerup;
	drv->subsys_desc.ramdump = adsp_ramdump;
	drv->subsys_desc.crash_shutdown = adsp_crash_shutdown;
	drv->subsys_desc.err_fatal_handler = adsp_err_fatal_intr_handler;
	drv->subsys_desc.wdog_bite_handler = adsp_wdog_bite_irq;

	INIT_WORK(&drv->work, adsp_fatal_fn);

	drv->ramdump_dev = create_ramdump_device("adsp", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	drv->wcnss_notif_hdle = subsys_notif_register_notifier("wcnss", &wnb);
	if (IS_ERR(drv->wcnss_notif_hdle)) {
		ret = PTR_ERR(drv->wcnss_notif_hdle);
		goto err_notif_wcnss;
	}

	drv->modem_notif_hdle = subsys_notif_register_notifier("modem", &mnb);
	if (IS_ERR(drv->modem_notif_hdle)) {
		ret = PTR_ERR(drv->modem_notif_hdle);
		goto err_notif_modem;
	}
	lpass_status = kobject_create_and_add("audio_voice_service",
						kernel_kobj);
	if (!lpass_status) {
		pr_err("%s: kobject create failed\n", __func__);
		ret = -ENOMEM;
		goto err_notif_modem;
	}

	ret = sysfs_create_group(lpass_status, &attr_group);
	if (ret) {
		pr_err("%s: sysfs create group failed\n", __func__);
		goto err_kobj;
	}

	adsp_set_state("ONLINE");
	return 0;
err_kobj:
	kobject_put(lpass_status);
err_notif_modem:
	subsys_notif_unregister_notifier(drv->wcnss_notif_hdle, &wnb);
err_notif_wcnss:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	pil_desc_release(desc);
	return ret;
}

static int pil_lpass_driver_exit(struct platform_device *pdev)
{
	struct lpass_data *drv = platform_get_drvdata(pdev);
	subsys_notif_unregister_notifier(drv->wcnss_notif_hdle, &wnb);
	subsys_notif_unregister_notifier(drv->modem_notif_hdle, &mnb);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->q6->desc);
	sysfs_remove_group(lpass_status, &attr_group);
	kobject_del(lpass_status);
	return 0;
}

static struct of_device_id lpass_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-lpass" },
	{ .compatible = "qcom,pil-q6v55-lpass" },
	{}
};

static struct platform_driver pil_lpass_driver = {
	.probe = pil_lpass_driver_probe,
	.remove = pil_lpass_driver_exit,
	.driver = {
		.name = "pil-q6v5-lpass",
		.of_match_table = lpass_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_lpass_init(void)
{
	return platform_driver_register(&pil_lpass_driver);
}
module_init(pil_lpass_init);

static void __exit pil_lpass_exit(void)
{
	platform_driver_unregister(&pil_lpass_driver);
}
module_exit(pil_lpass_exit);

MODULE_DESCRIPTION("Support for booting low-power audio subsystems with QDSP6v5 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
