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
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/dma-mapping.h>

#include <mach/subsystem_restart.h>
#include <mach/clk.h>
#include <mach/msm_smsm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "sysmon.h"

/* Q6 Register Offsets */
#define QDSP6SS_RST_EVB			0x010

/* AXI Halting Registers */
#define MSS_Q6_HALT_BASE		0x180
#define MSS_MODEM_HALT_BASE		0x200
#define MSS_NC_HALT_BASE		0x280

/* RMB Status Register Values */
#define STATUS_PBL_SUCCESS		0x1
#define STATUS_XPU_UNLOCKED		0x1
#define STATUS_XPU_UNLOCKED_SCRIBBLED	0x2

/* PBL/MBA interface registers */
#define RMB_MBA_IMAGE			0x00
#define RMB_PBL_STATUS			0x04
#define RMB_MBA_COMMAND			0x08
#define RMB_MBA_STATUS			0x0C
#define RMB_PMI_META_DATA		0x10
#define RMB_PMI_CODE_START		0x14
#define RMB_PMI_CODE_LENGTH		0x18

#define VDD_MSS_UV			1050000
#define MAX_VDD_MSS_UV			1150000
#define MAX_VDD_MX_UV			1150000

#define PROXY_TIMEOUT_MS		10000
#define POLL_INTERVAL_US		50

#define CMD_META_DATA_READY		0x1
#define CMD_LOAD_READY			0x2

#define STATUS_META_DATA_AUTH_SUCCESS	0x3
#define STATUS_AUTH_COMPLETE		0x4

#define MAX_SSR_REASON_LEN 81U

/* External BHS */
#define EXTERNAL_BHS_ON			BIT(0)
#define EXTERNAL_BHS_STATUS		BIT(4)
#define BHS_TIMEOUT_US			50

#define STOP_ACK_TIMEOUT_MS		1000

struct mba_data {
	void __iomem *rmb_base;
	void __iomem *io_clamp_reg;
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void *adsp_state_notifier;
	u32 img_length;
	struct q6v5_data *q6;
	bool self_auth;
	void *ramdump_dev;
	void *smem_ramdump_dev;
	bool crash_shutdown;
	bool ignore_errors;
	int err_fatal_irq;
	unsigned int stop_ack_irq;
	int force_stop_gpio;
	struct completion stop_ack;
};

static int pbl_mba_boot_timeout_ms = 1000;
module_param(pbl_mba_boot_timeout_ms, int, S_IRUGO | S_IWUSR);

static int modem_auth_timeout_ms = 10000;
module_param(modem_auth_timeout_ms, int, S_IRUGO | S_IWUSR);

static int pil_mss_power_up(struct q6v5_data *drv)
{
	int ret = 0;
	struct device *dev = drv->desc.dev;
	u32 regval;

	if (drv->vreg) {
		ret = regulator_enable(drv->vreg);
		if (ret)
			dev_err(dev, "Failed to enable modem regulator.\n");
	}

	if (drv->cxrail_bhs) {
		regval = readl_relaxed(drv->cxrail_bhs);
		regval |= EXTERNAL_BHS_ON;
		writel_relaxed(regval, drv->cxrail_bhs);

		ret = readl_poll_timeout(drv->cxrail_bhs, regval,
			regval & EXTERNAL_BHS_STATUS, 1, BHS_TIMEOUT_US);
	}

	return ret;
}

static int pil_mss_power_down(struct q6v5_data *drv)
{
	u32 regval;

	if (drv->cxrail_bhs) {
		regval = readl_relaxed(drv->cxrail_bhs);
		regval &= ~EXTERNAL_BHS_ON;
		writel_relaxed(regval, drv->cxrail_bhs);
	}

	if (drv->vreg)
		return regulator_disable(drv->vreg);

	return 0;
}

static int pil_mss_enable_clks(struct q6v5_data *drv)
{
	int ret;

	ret = clk_prepare_enable(drv->ahb_clk);
	if (ret)
		goto err_ahb_clk;
	ret = clk_prepare_enable(drv->axi_clk);
	if (ret)
		goto err_axi_clk;
	ret = clk_prepare_enable(drv->rom_clk);
	if (ret)
		goto err_rom_clk;

	return 0;

err_rom_clk:
	clk_disable_unprepare(drv->axi_clk);
err_axi_clk:
	clk_disable_unprepare(drv->ahb_clk);
err_ahb_clk:
	return ret;
}

static void pil_mss_disable_clks(struct q6v5_data *drv)
{
	clk_disable_unprepare(drv->rom_clk);
	clk_disable_unprepare(drv->axi_clk);
	clk_disable_unprepare(drv->ahb_clk);
}

static int wait_for_mba_ready(struct q6v5_data *drv)
{
	struct device *dev = drv->desc.dev;
	struct mba_data *mba = platform_get_drvdata(to_platform_device(dev));
	int ret;
	u32 status;

	/* Wait for PBL completion. */
	ret = readl_poll_timeout(mba->rmb_base + RMB_PBL_STATUS, status,
		status != 0, POLL_INTERVAL_US, pbl_mba_boot_timeout_ms * 1000);
	if (ret) {
		dev_err(dev, "PBL boot timed out\n");
		return ret;
	}
	if (status != STATUS_PBL_SUCCESS) {
		dev_err(dev, "PBL returned unexpected status %d\n", status);
		return -EINVAL;
	}

	/* Wait for MBA completion. */
	ret = readl_poll_timeout(mba->rmb_base + RMB_MBA_STATUS, status,
		status != 0, POLL_INTERVAL_US, pbl_mba_boot_timeout_ms * 1000);
	if (ret) {
		dev_err(dev, "MBA boot timed out\n");
		return ret;
	}
	if (status != STATUS_XPU_UNLOCKED &&
	    status != STATUS_XPU_UNLOCKED_SCRIBBLED) {
		dev_err(dev, "MBA returned unexpected status %d\n", status);
		return -EINVAL;
	}

	return 0;
}

static int pil_mss_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_Q6_HALT_BASE);
	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_MODEM_HALT_BASE);
	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_NC_HALT_BASE);

	writel_relaxed(1, drv->restart_reg);

	if (drv->is_booted) {
		pil_mss_disable_clks(drv);
		pil_mss_power_down(drv);
		drv->is_booted = false;
	}

	return 0;
}

static int pil_mss_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	struct platform_device *pdev = to_platform_device(pil->dev);
	struct mba_data *mba = platform_get_drvdata(pdev);
	phys_addr_t start_addr = pil_get_entry_addr(pil);
	int ret;

	/*
	 * Bring subsystem out of reset and enable required
	 * regulators and clocks.
	 */
	ret = pil_mss_power_up(drv);
	if (ret)
		goto err_power;

	/* Deassert reset to subsystem and wait for propagation */
	writel_relaxed(0, drv->restart_reg);
	mb();
	udelay(2);

	ret = pil_mss_enable_clks(drv);
	if (ret)
		goto err_clks;

	/* Program Image Address */
	if (mba->self_auth) {
		writel_relaxed(start_addr, mba->rmb_base + RMB_MBA_IMAGE);
		/* Ensure write to RMB base occurs before reset is released. */
		mb();
	} else {
		writel_relaxed((start_addr >> 4) & 0x0FFFFFF0,
				drv->reg_base + QDSP6SS_RST_EVB);
	}

	ret = pil_q6v5_reset(pil);
	if (ret)
		goto err_q6v5_reset;

	/* Wait for MBA to start. Check for PBL and MBA errors while waiting. */
	if (mba->self_auth) {
		ret = wait_for_mba_ready(drv);
		if (ret)
			goto err_auth;
	}

	drv->is_booted = true;

	return 0;

err_auth:
	pil_q6v5_shutdown(pil);
err_q6v5_reset:
	pil_mss_disable_clks(drv);
err_clks:
	pil_mss_power_down(drv);
err_power:
	return ret;
}

static int pil_q6v5_mss_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	ret = regulator_set_voltage(drv->vreg_mx, VDD_MSS_UV, MAX_VDD_MX_UV);
	if (ret) {
		dev_err(pil->dev, "Failed to request vreg_mx voltage\n");
		return ret;
	}

	ret = regulator_enable(drv->vreg_mx);
	if (ret) {
		dev_err(pil->dev, "Failed to enable vreg_mx\n");
		regulator_set_voltage(drv->vreg_mx, 0, MAX_VDD_MX_UV);
		return ret;
	}

	ret = pil_q6v5_make_proxy_votes(pil);
	if (ret) {
		regulator_disable(drv->vreg_mx);
		regulator_set_voltage(drv->vreg_mx, 0, MAX_VDD_MX_UV);
	}

	return ret;
}

static void pil_q6v5_mss_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	pil_q6v5_remove_proxy_votes(pil);
	regulator_disable(drv->vreg_mx);
	regulator_set_voltage(drv->vreg_mx, 0, MAX_VDD_MX_UV);
}

static struct pil_reset_ops pil_mss_ops = {
	.proxy_vote = pil_q6v5_mss_make_proxy_votes,
	.proxy_unvote = pil_q6v5_mss_remove_proxy_votes,
	.auth_and_reset = pil_mss_reset,
	.shutdown = pil_mss_shutdown,
};

static int pil_mba_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct mba_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->q6->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void pil_mba_remove_proxy_votes(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->q6->xo);
}

static int pil_mba_init_image(struct pil_desc *pil,
			      const u8 *metadata, size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	void *mdata_virt;
	dma_addr_t mdata_phys;
	s32 status;
	int ret;

	/* Make metadata physically contiguous and 4K aligned. */
	mdata_virt = dma_alloc_coherent(pil->dev, size, &mdata_phys,
					GFP_KERNEL);
	if (!mdata_virt) {
		dev_err(pil->dev, "MBA metadata buffer allocation failed\n");
		return -ENOMEM;
	}
	memcpy(mdata_virt, metadata, size);
	/* wmb() ensures copy completes prior to starting authentication. */
	wmb();

	/* Initialize length counter to 0 */
	writel_relaxed(0, drv->rmb_base + RMB_PMI_CODE_LENGTH);
	drv->img_length = 0;

	/* Pass address of meta-data to the MBA and perform authentication */
	writel_relaxed(mdata_phys, drv->rmb_base + RMB_PMI_META_DATA);
	writel_relaxed(CMD_META_DATA_READY, drv->rmb_base + RMB_MBA_COMMAND);
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
		status == STATUS_META_DATA_AUTH_SUCCESS || status < 0,
		POLL_INTERVAL_US, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of headers timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for headers\n",
				status);
		ret = -EINVAL;
	}

	dma_free_coherent(pil->dev, size, mdata_virt, mdata_phys);

	return ret;
}

static int pil_mba_verify_blob(struct pil_desc *pil, phys_addr_t phy_addr,
			       size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	s32 status;

	/* Begin image authentication */
	if (drv->img_length == 0) {
		writel_relaxed(phy_addr, drv->rmb_base + RMB_PMI_CODE_START);
		writel_relaxed(CMD_LOAD_READY, drv->rmb_base + RMB_MBA_COMMAND);
	}
	/* Increment length counter */
	drv->img_length += size;
	writel_relaxed(drv->img_length, drv->rmb_base + RMB_PMI_CODE_LENGTH);

	status = readl_relaxed(drv->rmb_base + RMB_MBA_STATUS);
	if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		return -EINVAL;
	}

	return 0;
}

static int pil_mba_auth(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	int ret;
	s32 status;

	/* Wait for all segments to be authenticated or an error to occur */
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
			status == STATUS_AUTH_COMPLETE || status < 0,
			50, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of image timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for image\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static struct pil_reset_ops pil_mba_ops = {
	.init_image = pil_mba_init_image,
	.proxy_vote = pil_mba_make_proxy_votes,
	.proxy_unvote = pil_mba_remove_proxy_votes,
	.verify_blob = pil_mba_verify_blob,
	.auth_and_reset = pil_mba_auth,
};

#define subsys_to_drv(d) container_of(d, struct mba_data, subsys_desc)

static void log_modem_sfr(void)
{
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];

	smem_reason = smem_get_entry(SMEM_SSR_REASON_MSS0, &size);
	if (!smem_reason || !size) {
		pr_err("modem subsystem failure reason: (unknown, smem_get_entry failed).\n");
		return;
	}
	if (!smem_reason[0]) {
		pr_err("modem subsystem failure reason: (unknown, empty string found).\n");
		return;
	}

	strlcpy(reason, smem_reason, min(size, sizeof(reason)));
	pr_err("modem subsystem failure reason: %s.\n", reason);

	smem_reason[0] = '\0';
	wmb();
}

static void restart_modem(struct mba_data *drv)
{
	log_modem_sfr();
	drv->ignore_errors = true;
	subsystem_restart_dev(drv->subsys);
}

static irqreturn_t modem_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct mba_data *drv = dev_id;

	/* Ignore if we're the one that set the force stop GPIO */
	if (drv->crash_shutdown)
		return IRQ_HANDLED;

	pr_err("Fatal error on the modem.\n");
	subsys_set_crash_status(drv->subsys, true);
	restart_modem(drv);
	return IRQ_HANDLED;
}

static irqreturn_t modem_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct mba_data *drv = dev_id;
	pr_info("Received stop ack interrupt from modem\n");
	complete(&drv->stop_ack);
	return IRQ_HANDLED;
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	unsigned long ret;

	if (subsys->is_not_loadable)
		return 0;

	if (!subsys_get_crash_status(drv->subsys)) {
		gpio_set_value(drv->force_stop_gpio, 1);
		ret = wait_for_completion_timeout(&drv->stop_ack,
				msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));
		if (!ret)
			pr_warn("Timed out on stop ack from modem.\n");
		gpio_set_value(drv->force_stop_gpio, 0);
	}

	pil_shutdown(&drv->desc);
	pil_shutdown(&drv->q6->desc);
	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	int ret;

	if (subsys->is_not_loadable)
		return 0;
	/*
	 * At this time, the modem is shutdown. Therefore this function cannot
	 * run concurrently with either the watchdog bite error handler or the
	 * SMSM callback, making it safe to unset the flag below.
	 */
	init_completion(&drv->stop_ack);
	drv->ignore_errors = false;
	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;
	ret = pil_boot(&drv->desc);
	if (ret)
		pil_shutdown(&drv->q6->desc);
	return ret;
}

static void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	drv->crash_shutdown = true;
	if (!subsys_get_crash_status(drv->subsys)) {
		gpio_set_value(drv->force_stop_gpio, 1);
		mdelay(STOP_ACK_TIMEOUT_MS);
	}
}

static struct ramdump_segment smem_segments[] = {
	{0x0FA00000, 0x0FC00000 - 0x0FA00000},
};

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	int ret;

	if (!enable)
		return 0;

	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;

	ret = pil_do_ramdump(&drv->desc, drv->ramdump_dev);
	if (ret < 0) {
		pr_err("Unable to dump modem fw memory (rc = %d).\n", ret);
		goto out;
	}

	ret = do_elf_ramdump(drv->smem_ramdump_dev, smem_segments,
		ARRAY_SIZE(smem_segments));
	if (ret < 0) {
		pr_err("Unable to dump smem memory (rc = %d).\n", ret);
		goto out;
	}

out:
	pil_shutdown(&drv->q6->desc);
	return ret;
}

static int adsp_state_notifier_fn(struct notifier_block *this,
				unsigned long code, void *ss_handle)
{
	int ret;
	ret = sysmon_send_event(SYSMON_SS_MODEM, "adsp", code);
	if (ret < 0)
		pr_err("%s: sysmon_send_event failed (%d).", __func__, ret);
	return NOTIFY_DONE;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = adsp_state_notifier_fn,
};

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	struct mba_data *drv = dev_id;
	if (drv->ignore_errors)
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from modem software!\n");
	subsys_set_crash_status(drv->subsys, true);
	restart_modem(drv);
	return IRQ_HANDLED;
}

static int mss_start(const struct subsys_desc *desc)
{
	int ret;
	struct mba_data *drv = subsys_to_drv(desc);

	if (desc->is_not_loadable)
		return 0;

	init_completion(&drv->stop_ack);
	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;
	ret = pil_boot(&drv->desc);
	if (ret)
		pil_shutdown(&drv->q6->desc);
	return ret;
}

static void mss_stop(const struct subsys_desc *desc)
{
	struct mba_data *drv = subsys_to_drv(desc);

	if (desc->is_not_loadable)
		return;

	pil_shutdown(&drv->desc);
	pil_shutdown(&drv->q6->desc);
}

static int __devinit pil_subsys_init(struct mba_data *drv,
					struct platform_device *pdev)
{
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	drv->subsys_desc.name = "modem";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;
	drv->subsys_desc.start = mss_start;
	drv->subsys_desc.stop = mss_stop;

	ret = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-err-ready", 0);
	if (ret < 0)
		return ret;

	ret = gpio_to_irq(ret);
	if (ret < 0)
		return ret;

	drv->subsys_desc.err_ready_irq = ret;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	drv->ramdump_dev = create_ramdump_device("modem", &pdev->dev);
	if (!drv->ramdump_dev) {
		pr_err("%s: Unable to create a modem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->smem_ramdump_dev = create_ramdump_device("smem-modem", &pdev->dev);
	if (!drv->smem_ramdump_dev) {
		pr_err("%s: Unable to create an smem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump_smem;
	}

	ret = devm_request_irq(&pdev->dev, irq, modem_wdog_bite_irq,
				IRQF_TRIGGER_RISING, "modem_wdog", drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to request watchdog IRQ.\n");
		goto err_irq;
	}

	ret = devm_request_irq(&pdev->dev, drv->err_fatal_irq,
			modem_err_fatal_intr_handler,
			IRQF_TRIGGER_RISING, "pil-mss", drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to register SMP2P err fatal handler!\n");
		goto err_irq;
	}

	ret = devm_request_irq(&pdev->dev, drv->stop_ack_irq,
			modem_stop_ack_intr_handler,
			IRQF_TRIGGER_RISING, "pil-mss", drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to register SMP2P stop ack handler!\n");
		goto err_irq;
	}

	drv->adsp_state_notifier = subsys_notif_register_notifier("adsp",
						&adsp_state_notifier_block);
	if (IS_ERR(drv->adsp_state_notifier)) {
		ret = PTR_ERR(drv->adsp_state_notifier);
		dev_err(&pdev->dev, "%s: Registration with the SSR notification driver failed (%d)",
			__func__, ret);
		goto err_irq;
	}

	return 0;

err_irq:
	destroy_ramdump_device(drv->smem_ramdump_dev);
err_ramdump_smem:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	subsys_unregister(drv->subsys);
err_subsys:
	return ret;
}

static int __devinit pil_mss_loadable_init(struct mba_data *drv,
					struct platform_device *pdev)
{
	struct q6v5_data *q6;
	struct pil_desc *q6_desc, *mba_desc;
	struct resource *res;
	struct property *prop;
	int ret;

	int clk_ready = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-proxy-unvote", 0);
	if (clk_ready < 0)
		return clk_ready;

	clk_ready = gpio_to_irq(clk_ready);
	if (clk_ready < 0)
		return clk_ready;

	q6 = pil_q6v5_init(pdev);
	if (IS_ERR(q6))
		return PTR_ERR(q6);
	drv->q6 = q6;

	q6_desc = &q6->desc;
	q6_desc->ops = &pil_mss_ops;
	q6_desc->owner = THIS_MODULE;
	q6_desc->proxy_timeout = PROXY_TIMEOUT_MS;
	q6_desc->proxy_unvote_irq = clk_ready;

	drv->self_auth = of_property_read_bool(pdev->dev.of_node,
							"qcom,pil-self-auth");
	if (drv->self_auth) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						    "rmb_base");
		drv->rmb_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!drv->rmb_base)
			return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "restart_reg");
	q6->restart_reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!q6->restart_reg)
		return -ENOMEM;

	q6->vreg = NULL;

	prop = of_find_property(pdev->dev.of_node, "vdd_mss-supply", NULL);
	if (prop) {
		q6->vreg = devm_regulator_get(&pdev->dev, "vdd_mss");
		if (IS_ERR(q6->vreg))
			return PTR_ERR(q6->vreg);

		ret = regulator_set_voltage(q6->vreg, VDD_MSS_UV,
						MAX_VDD_MSS_UV);
		if (ret)
			dev_err(&pdev->dev, "Failed to set vreg voltage.\n");

		ret = regulator_set_optimum_mode(q6->vreg, 100000);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set vreg mode.\n");
			return ret;
		}
	}

	q6->vreg_mx = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(q6->vreg_mx))
		return PTR_ERR(q6->vreg_mx);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"cxrail_bhs_reg");
	if (res)
		q6->cxrail_bhs = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));

	q6->ahb_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(q6->ahb_clk))
		return PTR_ERR(q6->ahb_clk);

	q6->axi_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(q6->axi_clk))
		return PTR_ERR(q6->axi_clk);

	q6->rom_clk = devm_clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(q6->rom_clk))
		return PTR_ERR(q6->rom_clk);

	ret = pil_desc_init(q6_desc);
	if (ret)
		return ret;

	mba_desc = &drv->desc;
	mba_desc->name = "modem";
	mba_desc->dev = &pdev->dev;
	mba_desc->ops = &pil_mba_ops;
	mba_desc->owner = THIS_MODULE;
	mba_desc->proxy_timeout = PROXY_TIMEOUT_MS;
	mba_desc->proxy_unvote_irq = clk_ready;

	ret = pil_desc_init(mba_desc);
	if (ret)
		goto err_mba_desc;

	return 0;

err_mba_desc:
	pil_desc_release(q6_desc);
	return ret;

}

static int __devinit pil_mss_driver_probe(struct platform_device *pdev)
{
	struct mba_data *drv;
	int ret, err_fatal_gpio, is_not_loadable, stop_ack_gpio;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	is_not_loadable = of_property_read_bool(pdev->dev.of_node,
							"qcom,is-not-loadable");
	if (is_not_loadable) {
		drv->subsys_desc.is_not_loadable = 1;
	} else {
		ret = pil_mss_loadable_init(drv, pdev);
		if (ret)
			return ret;
	}

	/* Get the IRQ from the GPIO for registering inbound handler */
	err_fatal_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-err-fatal", 0);
	if (err_fatal_gpio < 0)
		return err_fatal_gpio;

	drv->err_fatal_irq = gpio_to_irq(err_fatal_gpio);
	if (drv->err_fatal_irq < 0)
		return drv->err_fatal_irq;

	stop_ack_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-stop-ack", 0);
	if (stop_ack_gpio < 0)
		return stop_ack_gpio;

	ret = gpio_to_irq(stop_ack_gpio);
	if (ret < 0)
		return ret;
	drv->stop_ack_irq = ret;

	/* Get the GPIO pin for writing the outbound bits: add more as needed */
	drv->force_stop_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-force-stop", 0);
	if (drv->force_stop_gpio < 0)
		return drv->force_stop_gpio;

	return pil_subsys_init(drv, pdev);
}

static int __devexit pil_mss_driver_exit(struct platform_device *pdev)
{
	struct mba_data *drv = platform_get_drvdata(pdev);

	subsys_notif_unregister_notifier(drv->adsp_state_notifier,
						&adsp_state_notifier_block);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->smem_ramdump_dev);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->desc);
	pil_desc_release(&drv->q6->desc);
	return 0;
}

static struct of_device_id mss_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-mss" },
	{}
};

static struct platform_driver pil_mss_driver = {
	.probe = pil_mss_driver_probe,
	.remove = __devexit_p(pil_mss_driver_exit),
	.driver = {
		.name = "pil-q6v5-mss",
		.of_match_table = mss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mss_init(void)
{
	return platform_driver_register(&pil_mss_driver);
}
module_init(pil_mss_init);

static void __exit pil_mss_exit(void)
{
	platform_driver_unregister(&pil_mss_driver);
}
module_exit(pil_mss_exit);

MODULE_DESCRIPTION("Support for booting modem subsystems with QDSP6v5 Hexagon processors");
MODULE_LICENSE("GPL v2");
