// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "subsys-pil-tz: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/interconnect.h>
#include <dt-bindings/interconnect/qcom,lahaina.h>
#include <linux/dma-mapping.h>
#include <linux/qcom_scm.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/ramdump.h>

#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>

#include "peripheral-loader.h"

#define PIL_TZ_AVG_BW  0
#define PIL_TZ_PEAK_BW UINT_MAX

#define XO_FREQ			19200000
#define PROXY_TIMEOUT_MS	10000
#define MAX_SSR_REASON_LEN	256U
#define STOP_ACK_TIMEOUT_MS	1000
#define CRASH_STOP_ACK_TO_MS	200

#define ERR_READY	0
#define PBL_DONE	1

#define desc_to_data(d) container_of(d, struct pil_tz_data, desc)
#define subsys_to_data(d) container_of(d, struct pil_tz_data, subsys_desc)

/**
 * struct reg_info - regulator info
 * @reg: regulator handle
 * @uV: voltage in uV
 * @uA: current in uA
 */
struct reg_info {
	struct regulator *reg;
	int uV;
	int uA;
};

/**
 * struct pil_tz_data
 * @regs: regulators that should be always on when the subsystem is
 *	   brought out of reset
 * @proxy_regs: regulators that should be on during pil proxy voting
 * @clks: clocks that should be always on when the subsystem is
 *	  brought out of reset
 * @proxy_clks: clocks that should be on during pil proxy voting
 * @reg_count: the number of always on regulators
 * @proxy_reg_count: the number of proxy voting regulators
 * @clk_count: the number of always on clocks
 * @proxy_clk_count: the number of proxy voting clocks
 * @smem_id: the smem id used for read the subsystem crash reason
 * @ramdump_dev: ramdump device pointer
 * @pas_id: the PAS id for tz
 * @bus_client: bus client
 * @enable_bus_scaling: set to true if PIL needs to vote for
 *			bus bandwidth
 * @keep_proxy_regs_on: If set, during proxy unvoting, PIL removes the
 *			voltage/current vote for proxy regulators but leaves
 *			them enabled.
 * @stop_ack: state of completion of stop ack
 * @desc: PIL descriptor
 * @subsys: subsystem device pointer
 * @subsys_desc: subsystem descriptor
 * @u32 bits_arr[2]: array of bit positions in SCSR registers
 * @boot_enabled: true if subsystem is brough of reset during the bootloader.
 */
struct pil_tz_data {
	struct device *dev;
	struct reg_info *regs;
	struct reg_info *proxy_regs;
	struct clk **clks;
	struct clk **proxy_clks;
	int reg_count;
	int proxy_reg_count;
	int clk_count;
	int proxy_clk_count;
	int smem_id;
	void *ramdump_dev;
	void *minidump_dev;
	u32 pas_id;
	struct icc_path *bus_client;
	bool boot_enabled;
	bool enable_bus_scaling;
	bool keep_proxy_regs_on;
	struct completion err_ready;
	struct completion stop_ack;
	struct completion shutdown_ack;
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void __iomem *irq_status;
	void __iomem *irq_clear;
	void __iomem *irq_mask;
	void __iomem *err_status;
	void __iomem *err_status_spare;
	void __iomem *rmb_gp_reg;
	u32 bits_arr[2];
	int err_fatal_irq;
	int err_ready_irq;
	int stop_ack_irq;
	int wdog_bite_irq;
	int generic_irq;
	int ramdump_disable_irq;
	int shutdown_ack_irq;
	int force_stop_bit;
	struct qcom_smem_state *state;
};

enum pas_id {
	PAS_MODEM,
	PAS_Q6,
	PAS_DSPS,
	PAS_TZAPPS,
	PAS_MODEM_SW,
	PAS_MODEM_FW,
	PAS_WCNSS,
	PAS_SECAPP,
	PAS_GSS,
	PAS_VIDC,
	PAS_VPU,
	PAS_BCSS,
};

static struct icc_path *scm_perf_client;
static int scm_pas_bw_count;
static DEFINE_MUTEX(scm_pas_bw_mutex);
static int is_inited;

static void subsys_disable_all_irqs(struct pil_tz_data *d);
static void subsys_enable_all_irqs(struct pil_tz_data *d);
static bool generic_read_status(struct pil_tz_data *d);

static int enable_debug;
module_param(enable_debug, int, 0644);

static int wait_for_err_ready(struct pil_tz_data *d)
{
	int ret;

	/*
	 * If subsys is using generic_irq in which case err_ready_irq will be 0,
	 * don't return.
	 */
	if ((d->generic_irq <= 0 && !d->err_ready_irq) ||
				enable_debug == 1 || pil_is_timeout_disabled())
		return 0;

	ret = wait_for_completion_interruptible_timeout(&d->err_ready,
					  msecs_to_jiffies(10000));
	if (!ret) {
		pr_err("[%s]: Error ready timed out\n", d->desc.name);
		return -ETIMEDOUT;
	}

	return 0;
}

static int scm_pas_enable_bw(void)
{
	int ret = 0;

	if (IS_ERR(scm_perf_client))
		return -EINVAL;

	mutex_lock(&scm_pas_bw_mutex);
	if (!scm_pas_bw_count) {
		ret = icc_set_bw(scm_perf_client, PIL_TZ_AVG_BW,
						PIL_TZ_PEAK_BW);
		if (ret)
			goto err_bus;
		scm_pas_bw_count++;
	}

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;

err_bus:
	pr_err("scm-pas; Bandwidth request failed (%d)\n", ret);
	icc_set_bw(scm_perf_client, 0, 0);

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;
}

static void scm_pas_disable_bw(void)
{
	mutex_lock(&scm_pas_bw_mutex);
	if (scm_pas_bw_count-- == 1)
		icc_set_bw(scm_perf_client, 0, 0);
	mutex_unlock(&scm_pas_bw_mutex);
}

static int of_read_clocks(struct device *dev, struct clk ***clks_ref,
			  const char *propname)
{
	long clk_count;
	int i, len;
	struct clk **clks;

	if (!of_find_property(dev->of_node, propname, &len))
		return 0;

	clk_count = of_property_count_strings(dev->of_node, propname);
	if (IS_ERR_VALUE(clk_count)) {
		dev_err(dev, "Failed to get clock names\n");
		return -EINVAL;
	}

	clks = devm_kzalloc(dev, sizeof(struct clk *) * clk_count,
				GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	for (i = 0; i < clk_count; i++) {
		const char *clock_name;
		char clock_freq_name[50];
		u32 clock_rate = XO_FREQ;

		of_property_read_string_index(dev->of_node,
					      propname, i,
					      &clock_name);
		snprintf(clock_freq_name, ARRAY_SIZE(clock_freq_name),
						"qcom,%s-freq", clock_name);
		if (of_find_property(dev->of_node, clock_freq_name, &len))
			if (of_property_read_u32(dev->of_node, clock_freq_name,
								&clock_rate)) {
				dev_err(dev, "Failed to read %s clock's freq\n",
							clock_freq_name);
				return -EINVAL;
			}

		clks[i] = devm_clk_get(dev, clock_name);
		if (IS_ERR(clks[i])) {
			int rc = PTR_ERR(clks[i]);

			if (rc != -EPROBE_DEFER)
				dev_err(dev, "Failed to get %s clock\n",
								clock_name);
			return rc;
		}

		/* Make sure rate-settable clocks' rates are set */
		if (clk_get_rate(clks[i]) == 0)
			clk_set_rate(clks[i], clk_round_rate(clks[i],
								clock_rate));
	}

	*clks_ref = clks;
	return clk_count;
}

static int of_read_regs(struct device *dev, struct reg_info **regs_ref,
			const char *propname)
{
	long reg_count;
	int i, len, rc;
	struct reg_info *regs;

	if (!of_find_property(dev->of_node, propname, &len))
		return 0;

	reg_count = of_property_count_strings(dev->of_node, propname);
	if (IS_ERR_VALUE(reg_count)) {
		dev_err(dev, "Failed to get regulator names\n");
		return -EINVAL;
	}

	regs = devm_kzalloc(dev, sizeof(struct reg_info) * reg_count,
				GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	for (i = 0; i < reg_count; i++) {
		const char *reg_name;
		char reg_uV_uA_name[50];
		u32 vdd_uV_uA[2];

		of_property_read_string_index(dev->of_node,
					      propname, i,
					      &reg_name);

		regs[i].reg = devm_regulator_get(dev, reg_name);
		if (IS_ERR(regs[i].reg)) {
			int rc = PTR_ERR(regs[i].reg);

			if (rc != -EPROBE_DEFER)
				dev_err(dev, "Failed to get %s\n regulator\n",
								reg_name);
			return rc;
		}

		/*
		 * Read the voltage and current values for the corresponding
		 * regulator. The device tree property name is "qcom," +
		 *  "regulator_name" + "-uV-uA".
		 */
		rc = snprintf(reg_uV_uA_name, ARRAY_SIZE(reg_uV_uA_name),
			 "qcom,%s-uV-uA", reg_name);
		if (rc < strlen(reg_name) + 6) {
			dev_err(dev, "Failed to hold reg_uV_uA_name\n");
			return -EINVAL;
		}

		if (!of_find_property(dev->of_node, reg_uV_uA_name, &len))
			continue;

		len /= sizeof(vdd_uV_uA[0]);

		/* There should be two entries: one for uV and one for uA */
		if (len != 2) {
			dev_err(dev, "Missing uV/uA value\n");
			return -EINVAL;
		}

		rc = of_property_read_u32_array(dev->of_node, reg_uV_uA_name,
					vdd_uV_uA, len);
		if (rc) {
			dev_err(dev, "Failed to read uV/uA values(rc:%d)\n",
									rc);
			return rc;
		}

		regs[i].uV = vdd_uV_uA[0];
		regs[i].uA = vdd_uV_uA[1];
	}

	*regs_ref = regs;
	return reg_count;
}

#if IS_ENABLED(CONFIG_INTERCONNECT_QCOM)
static int of_read_bus_client(struct platform_device *pdev,
			     struct pil_tz_data *d)
{
	d->bus_client = of_icc_get(&pdev->dev, NULL);
	if (!d->bus_client)
		pr_warn("%s: Unable to register bus client\n", __func__);

	return 0;
}
static int do_bus_scaling_request(struct pil_desc *pil, int enable)
{
	int rc;
	struct pil_tz_data *d = desc_to_data(pil);
	u32 avg_bw = enable ? PIL_TZ_AVG_BW : 0;
	u32 peak_bw = enable ? PIL_TZ_PEAK_BW : 0;

	if (d->bus_client) {
		rc = icc_set_bw(d->bus_client, avg_bw, peak_bw);
		if (rc) {
			dev_err(pil->dev, "bandwidth request failed(rc:%d)\n",
									rc);
			return rc;
		}
	} else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);
	return 0;
}
#else
static int of_read_bus_client(struct platform_device *pdev,
			     struct pil_tz_data *d)
{
	return 0;
}
static int do_bus_scaling_request(struct pil_desc *pil, int enable)
{
	return 0;
}
#endif

static int piltz_resc_init(struct platform_device *pdev, struct pil_tz_data *d)
{
	int len, count, rc;
	struct device *dev = &pdev->dev;

	count = of_read_clocks(dev, &d->clks, "qcom,active-clock-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup clocks(rc:%d).\n", count);
		return count;
	}
	d->clk_count = count;

	count = of_read_clocks(dev, &d->proxy_clks, "qcom,proxy-clock-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup proxy clocks(rc:%d).\n", count);
		return count;
	}
	d->proxy_clk_count = count;

	count = of_read_regs(dev, &d->regs, "qcom,active-reg-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup regulators(rc:%d).\n", count);
		return count;
	}
	d->reg_count = count;

	count = of_read_regs(dev, &d->proxy_regs, "qcom,proxy-reg-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup proxy regulators(rc:%d).\n",
				count);
		return count;
	}
	d->proxy_reg_count = count;

	if (of_find_property(dev->of_node, "interconnects", &len)) {
		d->enable_bus_scaling = true;
		rc = of_read_bus_client(pdev, d);
		if (rc) {
			dev_err(dev, "Failed to setup bus scaling client(rc:%d).\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static int enable_regulators(struct pil_tz_data *d, struct device *dev,
				struct reg_info *regs, int reg_count,
				bool reg_no_enable)
{
	int i, rc = 0;

	for (i = 0; i < reg_count; i++) {
		if (regs[i].uV > 0) {
			rc = regulator_set_voltage(regs[i].reg,
					regs[i].uV, INT_MAX);
			if (rc) {
				dev_err(dev, "Failed to request voltage(rc:%d)\n",
									rc);
				goto err_voltage;
			}
		}

		if (regs[i].uA > 0) {
			rc = regulator_set_load(regs[i].reg,
						regs[i].uA);
			if (rc < 0) {
				dev_err(dev, "Failed to set regulator mode(rc:%d)\n",
									rc);
				goto err_mode;
			}
		}

		if (d->keep_proxy_regs_on && reg_no_enable)
			continue;

		rc = regulator_enable(regs[i].reg);
		if (rc) {
			dev_err(dev, "Regulator enable failed(rc:%d)\n", rc);
			goto err_enable;
		}
	}

	return 0;
err_enable:
	if (regs[i].uA > 0) {
		regulator_set_voltage(regs[i].reg, 0, INT_MAX);
		regulator_set_load(regs[i].reg, 0);
	}
err_mode:
	if (regs[i].uV > 0)
		regulator_set_voltage(regs[i].reg, 0, INT_MAX);
err_voltage:
	for (i--; i >= 0; i--) {
		if (regs[i].uV > 0)
			regulator_set_voltage(regs[i].reg, 0, INT_MAX);

		if (regs[i].uA > 0)
			regulator_set_load(regs[i].reg, 0);

		if (d->keep_proxy_regs_on && reg_no_enable)
			continue;
		regulator_disable(regs[i].reg);
	}

	return rc;
}

static void disable_regulators(struct pil_tz_data *d, struct reg_info *regs,
					int reg_count, bool reg_no_disable)
{
	int i;

	for (i = 0; i < reg_count; i++) {
		if (regs[i].uV > 0)
			regulator_set_voltage(regs[i].reg, 0, INT_MAX);

		if (regs[i].uA > 0)
			regulator_set_load(regs[i].reg, 0);

		if (d->keep_proxy_regs_on && reg_no_disable)
			continue;
		regulator_disable(regs[i].reg);
	}
}

static int prepare_enable_clocks(struct device *dev, struct clk **clks,
								int clk_count)
{
	int rc = 0;
	int i;

	for (i = 0; i < clk_count; i++) {
		rc = clk_prepare_enable(clks[i]);
		if (rc) {
			dev_err(dev, "Clock enable failed(rc:%d)\n", rc);
			goto err;
		}
	}

	return 0;
err:
	for (i--; i >= 0; i--)
		clk_disable_unprepare(clks[i]);

	return rc;
}

static void disable_unprepare_clocks(struct clk **clks, int clk_count)
{
	int i;

	for (i = --clk_count; i >= 0; i--)
		clk_disable_unprepare(clks[i]);
}

static int pil_make_proxy_vote(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	int rc;

	if (d->subsys_desc.no_auth)
		return 0;

	rc = do_bus_scaling_request(pil, 1);
	if (rc)
		return rc;

	rc = enable_regulators(d, pil->dev, d->proxy_regs,
					d->proxy_reg_count, false);
	if (rc)
		return rc;

	rc = prepare_enable_clocks(pil->dev, d->proxy_clks,
							d->proxy_clk_count);
	if (rc)
		goto err_clks;

	return 0;

err_clks:
	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, false);

	return rc;
}

static void pil_remove_proxy_vote(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);

	if (d->subsys_desc.no_auth)
		return;

	disable_unprepare_clocks(d->proxy_clks, d->proxy_clk_count);

	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, true);

	do_bus_scaling_request(pil, 0);
}

static int pil_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	struct pil_tz_data *d = desc_to_data(pil);
	u32 scm_ret = 0;
	int ret;

	if (d->subsys_desc.no_auth)
		return 0;

	ret = scm_pas_enable_bw();
	if (ret)
		return ret;

	scm_ret = qcom_scm_pas_init_image(d->pas_id, metadata, size);

	scm_pas_disable_bw();
	return scm_ret;
}

static int pil_mem_setup_trusted(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	struct pil_tz_data *d = desc_to_data(pil);
	u32 scm_ret = 0;

	if (d->subsys_desc.no_auth)
		return 0;

	size += pil->extra_size;
	scm_ret = qcom_scm_pas_mem_setup(d->pas_id, addr, size);

	return scm_ret;
}

static int pil_auth_and_reset(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	int rc;
	u32 scm_ret = 0;
	unsigned long pfn_start, pfn_end, pfn;

	if (d->subsys_desc.no_auth)
		return 0;

	rc = scm_pas_enable_bw();
	if (rc)
		return rc;

	rc = enable_regulators(d, pil->dev, d->regs, d->reg_count, false);
	if (rc)
		return rc;

	rc = prepare_enable_clocks(pil->dev, d->clks, d->clk_count);
	if (rc)
		goto err_clks;

	scm_ret = qcom_scm_pas_auth_and_reset(d->pas_id);

	pfn_start = pil->priv->region_start >> PAGE_SHIFT;
	if (pfn_valid(pfn_start) && !scm_ret) {
		pfn_end = (PAGE_ALIGN(pil->priv->region_start +
				pil->priv->region_size)) >> PAGE_SHIFT;
		for (pfn = pfn_start; pfn < pfn_end; pfn++)
			set_page_private(pfn_to_page(pfn), SECURE_PAGE_MAGIC);
	}

	scm_pas_disable_bw();
	if (rc)
		goto err_reset;

	return scm_ret;
err_reset:
	disable_unprepare_clocks(d->clks, d->clk_count);
err_clks:
	disable_regulators(d, d->regs, d->reg_count, false);

	return rc;
}

static int pil_shutdown_trusted(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	u32 scm_ret = 0;
	int rc;
	unsigned long pfn_start, pfn_end, pfn;

	if (d->subsys_desc.no_auth)
		return 0;

	rc = do_bus_scaling_request(pil, 1);
	if (rc)
		return rc;

	rc = enable_regulators(d, pil->dev, d->proxy_regs,
					d->proxy_reg_count, true);
	if (rc)
		goto err_regulators;

	rc = prepare_enable_clocks(pil->dev, d->proxy_clks,
						d->proxy_clk_count);
	if (rc)
		goto err_clks;

	scm_ret = qcom_scm_pas_shutdown(d->pas_id);

	pfn_start = pil->priv->region_start >> PAGE_SHIFT;
	if (pfn_valid(pfn_start) && !scm_ret) {
		pfn_end = (PAGE_ALIGN(pil->priv->region_start +
				pil->priv->region_size)) >> PAGE_SHIFT;
		for (pfn = pfn_start; pfn < pfn_end; pfn++)
			set_page_private(pfn_to_page(pfn), 0);
	}

	disable_unprepare_clocks(d->proxy_clks, d->proxy_clk_count);
	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, false);

	do_bus_scaling_request(pil, 0);

	if (rc)
		return rc;

	disable_unprepare_clocks(d->clks, d->clk_count);
	disable_regulators(d, d->regs, d->reg_count, false);

	return scm_ret;

err_clks:
	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, false);
err_regulators:
	do_bus_scaling_request(pil, 0);

	return rc;
}

static int pil_deinit_image_trusted(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	u32 scm_ret = 0;
	unsigned long pfn_start, pfn_end, pfn;

	if (d->subsys_desc.no_auth)
		return 0;

	scm_ret = qcom_scm_pas_shutdown(d->pas_id);
	pfn_start = pil->priv->region_start >> PAGE_SHIFT;
	if (pfn_valid(pfn_start) && !scm_ret) {
		pfn_end = (PAGE_ALIGN(pil->priv->region_start +
			    pil->priv->region_size)) >> PAGE_SHIFT;
		for (pfn = pfn_start; pfn < pfn_end; pfn++)
			set_page_private(pfn_to_page(pfn), 0);
	}

	return scm_ret;
}

static struct pil_reset_ops pil_ops_trusted = {
	.init_image = pil_init_image_trusted,
	.mem_setup =  pil_mem_setup_trusted,
	.auth_and_reset = pil_auth_and_reset,
	.shutdown = pil_shutdown_trusted,
	.proxy_vote = pil_make_proxy_vote,
	.proxy_unvote = pil_remove_proxy_vote,
	.deinit_image = pil_deinit_image_trusted,
};

static void log_failure_reason(const struct pil_tz_data *d)
{
	size_t size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];
	const char *name = d->subsys_desc.name;

	if (d->smem_id == -1)
		return;

	smem_reason = qcom_smem_get(QCOM_SMEM_HOST_ANY, d->smem_id, &size);
	if (IS_ERR(smem_reason) || !size) {
		pr_err("%s SFR: (unknown, qcom_smem_get failed).\n",
									name);
		return;
	}
	if (!smem_reason[0]) {
		pr_err("%s SFR: (unknown, empty string found).\n", name);
		return;
	}

	strlcpy(reason, smem_reason, min(size, (size_t)MAX_SSR_REASON_LEN));
	pr_err("%s subsystem failure reason: %s.\n", name, reason);
}

static int subsys_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct pil_tz_data *d = subsys_to_data(subsys);
	int ret;

	if (!subsys_get_crash_status(d->subsys) && force_stop &&
						d->state) {
		qcom_smem_state_update_bits(d->state,
				BIT(d->force_stop_bit),
				BIT(d->force_stop_bit));
		ret = wait_for_completion_timeout(&d->stop_ack,
				msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));
		if (!ret)
			pr_warn("Timed out on stop ack from %s.\n",
							subsys->name);
		qcom_smem_state_update_bits(d->state,
				BIT(d->force_stop_bit), 0);
	}

	pil_shutdown(&d->desc);
	subsys_disable_all_irqs(d);
	return 0;
}

static int subsys_powerup(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);
	int ret = 0;

	reinit_completion(&d->err_ready);

	if (d->stop_ack_irq)
		reinit_completion(&d->stop_ack);

	d->desc.fw_name = subsys->fw_name;
	ret = pil_boot(&d->desc);
	if (ret) {
		pr_err("pil_boot failed for %s\n",  d->subsys_desc.name);
		return ret;
	}

	pr_info("pil_boot is successful from %s and waiting for error ready\n",
				d->subsys_desc.name);
	subsys_enable_all_irqs(d);
	ret = wait_for_err_ready(d);
	if (ret) {
		pr_err("%s failed to get error ready for %s\n", __func__,
			d->subsys_desc.name);
		pil_shutdown(&d->desc);
		subsys_disable_all_irqs(d);
	}

	return ret;
}

static int subsys_powerup_boot_enabled(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);
	int ret = 0;

	if (generic_read_status(d)) {
		pr_info("%s: subsystem %s is alive at during device bootup\n",
			 __func__, d->subsys_desc.name);
		subsys_enable_all_irqs(d);
		ret = wait_for_err_ready(d);
		if (ret) {
			pr_err("%s failed to get error ready for %s\n",
				__func__, d->subsys_desc.name);
			pil_shutdown(&d->desc);
			subsys_disable_all_irqs(d);
		}
	} else {
		pil_shutdown(&d->desc);
		ret = -EAGAIN;
		pr_err("%s: subsystem %s is crashed while device booting\n",
			 __func__, d->subsys_desc.name);
	}

	/* Update the .powerup call back to regular subsys powerup function.*/
	d->subsys_desc.powerup = subsys_powerup;
	return ret;
}

static int subsys_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&d->desc, d->ramdump_dev, d->minidump_dev);
}

static void subsys_free_memory(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	pil_free_memory(&d->desc);
}

static void subsys_crash_shutdown(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	if (d->state && !subsys_get_crash_status(d->subsys)) {
		qcom_smem_state_update_bits(d->state,
			BIT(d->force_stop_bit),
			BIT(d->force_stop_bit));
		mdelay(CRASH_STOP_ACK_TO_MS);
	}
}


static irqreturn_t subsys_err_ready_intr_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	pr_info("Subsystem error monitoring/handling services are up from%s\n",
					d->subsys_desc.name);
	complete(&d->err_ready);
	return IRQ_HANDLED;
}

static irqreturn_t subsys_err_fatal_intr_handler (int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	pr_err("Fatal error on %s!\n", d->subsys_desc.name);
	if (subsys_get_crash_status(d->subsys)) {
		pr_err("%s: Ignoring error fatal, restart in progress\n",
							d->subsys_desc.name);
		return IRQ_HANDLED;
	}
	subsys_set_crash_status(d->subsys, CRASH_STATUS_ERR_FATAL);
	log_failure_reason(d);
	subsystem_restart_dev(d->subsys);

	return IRQ_HANDLED;
}

static irqreturn_t subsys_wdog_bite_irq_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	if (subsys_get_crash_status(d->subsys))
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from %s!\n", d->subsys_desc.name);

	if (d->subsys_desc.system_debug)
		panic("%s: System ramdump requested. Triggering device restart!\n",
							__func__);
	subsys_set_crash_status(d->subsys, CRASH_STATUS_WDOG_BITE);
	log_failure_reason(d);
	subsystem_restart_dev(d->subsys);

	return IRQ_HANDLED;
}

static irqreturn_t subsys_stop_ack_intr_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	pr_info("Received stop ack interrupt from %s\n", d->subsys_desc.name);
	complete(&d->stop_ack);
	return IRQ_HANDLED;
}

static irqreturn_t subsys_shutdown_ack_intr_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	pr_info("Received stop shutdown interrupt from %s\n",
			d->subsys_desc.name);
	complete_shutdown_ack(&d->subsys_desc);
	return IRQ_HANDLED;
}

static irqreturn_t subsys_ramdump_disable_intr_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;

	pr_info("Received ramdump disable interrupt from %s\n",
			d->subsys_desc.name);
	d->subsys_desc.ramdump_disable = 1;
	return IRQ_HANDLED;
}

static void clear_pbl_done(struct pil_tz_data *d)
{
	uint32_t err_value;

	err_value =  __raw_readl(d->err_status);

	if (err_value) {
		uint32_t rmb_err_spare0;
		uint32_t rmb_err_spare1;
		uint32_t rmb_err_spare2;

		pr_debug("PBL_DONE received from %s!\n", d->subsys_desc.name);

		rmb_err_spare2 =  __raw_readl(d->err_status_spare);
		rmb_err_spare1 =  __raw_readl(d->err_status_spare-4);
		rmb_err_spare0 =  __raw_readl(d->err_status_spare-8);

		pr_err("PBL error status register: 0x%08x\n", err_value);

		pr_err("PBL error status spare0 register: 0x%08x\n",
			rmb_err_spare0);
		pr_err("PBL error status spare1 register: 0x%08x\n",
			rmb_err_spare1);
		pr_err("PBL error status spare2 register: 0x%08x\n",
			rmb_err_spare2);
	} else {
		pr_info("PBL_DONE - 1st phase loading [%s] completed ok\n",
			d->subsys_desc.name);
	}
	__raw_writel(BIT(d->bits_arr[PBL_DONE]), d->irq_clear);
}

static void clear_err_ready(struct pil_tz_data *d)
{
	pr_debug("Subsystem error services up received from %s\n",
							d->subsys_desc.name);

	pr_info("SW_INIT_DONE - 2nd phase loading [%s] completed ok\n",
		d->subsys_desc.name);

	__raw_writel(BIT(d->bits_arr[ERR_READY]), d->irq_clear);
	complete(&d->err_ready);
}

static void clear_sw_init_done_error(struct pil_tz_data *d, int err)
{
	uint32_t rmb_err_spare0;
	uint32_t rmb_err_spare1;
	uint32_t rmb_err_spare2;

	pr_info("SW_INIT_DONE - ERROR [%s] [0x%x].\n",
		d->subsys_desc.name, err);

	rmb_err_spare2 =  __raw_readl(d->err_status_spare);
	rmb_err_spare1 =  __raw_readl(d->err_status_spare-4);
	rmb_err_spare0 =  __raw_readl(d->err_status_spare-8);

	pr_err("spare0 register: 0x%08x\n", rmb_err_spare0);
	pr_err("spare1 register: 0x%08x\n", rmb_err_spare1);
	pr_err("spare2 register: 0x%08x\n", rmb_err_spare2);

	/* Clear the interrupt source */
	__raw_writel(BIT(d->bits_arr[ERR_READY]), d->irq_clear);
}



static void clear_wdog(struct pil_tz_data *d)
{
	/* Check crash status to know if device is restarting*/
	if (!subsys_get_crash_status(d->subsys)) {
		pr_err("wdog bite received from %s!\n", d->subsys_desc.name);
		__raw_writel(BIT(d->bits_arr[ERR_READY]), d->irq_clear);
		subsys_set_crash_status(d->subsys, CRASH_STATUS_WDOG_BITE);
		log_failure_reason(d);
		subsystem_restart_dev(d->subsys);
	}
}

static bool generic_read_status(struct pil_tz_data *d)
{
	uint32_t status_val, err_value;

	err_value =  __raw_readl(d->err_status_spare);
	status_val = __raw_readl(d->irq_status);

	if (status_val & BIT(d->bits_arr[ERR_READY])) {
		if (err_value == 0x44554d50) {
			pr_err("wdog bite is pending\n");
			__raw_writel(BIT(d->bits_arr[ERR_READY]), d->irq_clear);
			return false;
		}
	}

	return true;
}

static irqreturn_t subsys_generic_handler(int irq, void *drv_data)
{
	struct pil_tz_data *d = drv_data;
	uint32_t status_val, err_value;

	err_value =  __raw_readl(d->err_status_spare);
	status_val = __raw_readl(d->irq_status);

	if (status_val & BIT(d->bits_arr[ERR_READY])) {
		if (!err_value)
			clear_err_ready(d);
		else if (err_value == 0x44554d50)
			clear_wdog(d);
		else
			clear_sw_init_done_error(d, err_value);
	}

	if (status_val & BIT(d->bits_arr[PBL_DONE]))
		clear_pbl_done(d);

	return IRQ_HANDLED;
}

static void mask_scsr_irqs(struct pil_tz_data *d)
{
	uint32_t mask_val;

	/* Masking all interrupts from subsystem */
	mask_val = ~0;
	__raw_writel(mask_val, d->irq_mask);
}

static void unmask_scsr_irqs(struct pil_tz_data *d)
{
	uint32_t mask_val;

	/* Un masking interrupts from subsystem to be handled by HLOS */
	mask_val = ~0;
	__raw_writel(mask_val & ~BIT(d->bits_arr[ERR_READY]) &
			~BIT(d->bits_arr[PBL_DONE]), d->irq_mask);
}

static void subsys_enable_all_irqs(struct pil_tz_data *d)
{
	if (d->err_ready_irq)
		enable_irq(d->err_ready_irq);
	if (d->wdog_bite_irq) {
		enable_irq(d->wdog_bite_irq);
		irq_set_irq_wake(d->wdog_bite_irq, 1);
	}
	if (d->err_fatal_irq)
		enable_irq(d->err_fatal_irq);
	if (d->stop_ack_irq)
		enable_irq(d->stop_ack_irq);
	if (d->shutdown_ack_irq)
		enable_irq(d->shutdown_ack_irq);
	if (d->ramdump_disable_irq)
		enable_irq(d->ramdump_disable_irq);
	if (d->generic_irq) {
		unmask_scsr_irqs(d);
		enable_irq(d->generic_irq);
		irq_set_irq_wake(d->generic_irq, 1);
	}
}

static void subsys_disable_all_irqs(struct pil_tz_data *d)
{
	if (d->err_ready_irq)
		disable_irq(d->err_ready_irq);
	if (d->wdog_bite_irq) {
		disable_irq(d->wdog_bite_irq);
		irq_set_irq_wake(d->wdog_bite_irq, 0);
	}
	if (d->err_fatal_irq)
		disable_irq(d->err_fatal_irq);
	if (d->stop_ack_irq)
		disable_irq(d->stop_ack_irq);
	if (d->shutdown_ack_irq)
		disable_irq(d->shutdown_ack_irq);
	if (d->generic_irq) {
		mask_scsr_irqs(d);
		irq_set_irq_wake(d->generic_irq, 0);
		disable_irq(d->generic_irq);
	}
}

static int __get_irq(struct platform_device *pdev, const char *prop,
		unsigned int *irq)
{
	int irql = 0;
	struct device_node *dnode = pdev->dev.of_node;

	if (of_property_match_string(dnode, "interrupt-names", prop) < 0)
		return -ENOENT;

	irql = of_irq_get_byname(dnode, prop);
	if (irql < 0) {
		pr_err("[%s]: Error getting IRQ \"%s\"\n", pdev->name,
		prop);
		return irql;
	}
	*irq = irql;
	return 0;
}

static int __get_smem_state(struct pil_tz_data *d, const char *prop,
		int *smem_bit)
{
	struct device_node *dnode = d->dev->of_node;

	if (of_find_property(dnode, "qcom,smem-states", NULL)) {
		d->state = qcom_smem_state_get(d->dev, prop, smem_bit);
		if (IS_ERR_OR_NULL(d->state)) {
			pr_err("Could not get smem-states %s\n", prop);
			return PTR_ERR(d->state);
		}
		return 0;
	}
	return -ENOENT;
}


static int subsys_parse_irqs(struct platform_device *pdev)
{
	int ret;
	struct pil_tz_data *d = platform_get_drvdata(pdev);

	ret = __get_irq(pdev, "qcom,err-fatal", &d->err_fatal_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(pdev, "qcom,err-ready", &d->err_ready_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(pdev, "qcom,stop-ack", &d->stop_ack_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(pdev, "qcom,ramdump-disabled",
			&d->ramdump_disable_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(pdev, "qcom,shutdown-ack", &d->shutdown_ack_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(pdev, "qcom,wdog", &d->wdog_bite_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_smem_state(d, "qcom,force-stop", &d->force_stop_bit);
	if (ret && ret != -ENOENT)
		return ret;

	if (of_property_read_bool(pdev->dev.of_node,
					"qcom,pil-generic-irq-handler")) {
		ret = platform_get_irq(pdev, 0);
		if (ret > 0)
			d->generic_irq = ret;
	}

	return 0;
}

static int subsys_setup_irqs(struct platform_device *pdev)
{
	int ret;
	struct pil_tz_data *d = platform_get_drvdata(pdev);

	if (d->err_fatal_irq) {
		ret = devm_request_threaded_irq(&pdev->dev, d->err_fatal_irq,
				NULL, subsys_err_fatal_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register error fatal IRQ handler: %d, irq is %d\n",
				d->desc.name, ret, d->err_fatal_irq);
			return ret;
		}
		disable_irq(d->err_fatal_irq);
	}

	if (d->stop_ack_irq) {
		ret = devm_request_threaded_irq(&pdev->dev, d->stop_ack_irq,
				NULL, subsys_stop_ack_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register stop ack handler: %d\n",
				d->desc.name, ret);
			return ret;
		}
		disable_irq(d->stop_ack_irq);
	}

	if (d->wdog_bite_irq) {
		ret = devm_request_irq(&pdev->dev, d->wdog_bite_irq,
			subsys_wdog_bite_irq_handler,
			IRQF_TRIGGER_RISING, d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register wdog bite handler: %d\n",
				d->desc.name, ret);
			return ret;
		}
		disable_irq(d->wdog_bite_irq);
	}

	if (d->shutdown_ack_irq) {
		ret = devm_request_threaded_irq(&pdev->dev,
				d->shutdown_ack_irq,
				NULL, subsys_shutdown_ack_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register shutdown ack handler: %d\n",
				d->desc.name, ret);
			return ret;
		}
		disable_irq(d->shutdown_ack_irq);
	}

	if (d->ramdump_disable_irq) {
		ret = devm_request_threaded_irq(d->dev,
				d->ramdump_disable_irq,
				NULL, subsys_ramdump_disable_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register shutdown ack handler: %d\n",
				d->desc.name, ret);
			return ret;
		}
		disable_irq(d->ramdump_disable_irq);
	}

	if (d->generic_irq) {
		ret = devm_request_irq(&pdev->dev, d->generic_irq,
			subsys_generic_handler,
			IRQF_TRIGGER_HIGH, d->desc.name, d);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]: Unable to register generic irq handler: %d\n",
				d->desc.name, ret);
			return ret;
		}
		disable_irq(d->generic_irq);
	}

	if (d->err_ready_irq) {
		ret = devm_request_threaded_irq(d->dev,
				d->err_ready_irq,
				NULL, subsys_err_ready_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"error_ready_interrupt", d);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"[%s]: Unable to register err ready handler\n",
				d->desc.name);
			return ret;
		}
		disable_irq(d->err_ready_irq);
	}

	return 0;
}

static int pil_tz_generic_probe(struct platform_device *pdev)
{
	struct pil_tz_data *d;
	struct resource *res;
	u32 proxy_timeout, rmb_gp_reg_val;
	int len, rc;
	char md_node[20];

	/* Do not probe the generic PIL driver yet if the SCM BW driver
	 * is not yet registered. Return error if that driver returns with
	 * any error other than EPROBE_DEFER.
	 */
	if (!is_inited)
		return -EPROBE_DEFER;
	if (IS_ERR(scm_perf_client))
		return PTR_ERR(scm_perf_client);

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	platform_set_drvdata(pdev, d);

	if (of_property_read_bool(pdev->dev.of_node, "qcom,pil-no-auth"))
		d->subsys_desc.no_auth = true;

	d->keep_proxy_regs_on = of_property_read_bool(pdev->dev.of_node,
						"qcom,keep-proxy-regs-on");

	rc = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &d->desc.name);
	if (rc)
		return rc;

	/* Defaulting smem_id to be not present */
	d->smem_id = -1;

	if (of_find_property(pdev->dev.of_node, "qcom,smem-id", &len)) {
		rc = of_property_read_u32(pdev->dev.of_node, "qcom,smem-id",
						&d->smem_id);
		if (rc) {
			dev_err(&pdev->dev, "Failed to get the smem_id(rc:%d)\n",
									rc);
			return rc;
		}
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,extra-size",
						&d->desc.extra_size);
	if (rc)
		d->desc.extra_size = 0;

	d->dev = &pdev->dev;
	d->desc.dev = &pdev->dev;
	d->desc.owner = THIS_MODULE;
	d->desc.ops = &pil_ops_trusted;

	d->desc.proxy_timeout = PROXY_TIMEOUT_MS;
	rc = subsys_parse_irqs(pdev);
	if (rc)
		return rc;

	d->desc.clear_fw_region = true;

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,proxy-timeout-ms",
					&proxy_timeout);
	if (!rc)
		d->desc.proxy_timeout = proxy_timeout;

	if (!d->subsys_desc.no_auth) {
		rc = piltz_resc_init(pdev, d);
		if (rc)
			return rc;

		rc = of_property_read_u32(pdev->dev.of_node, "qcom,pas-id",
								&d->pas_id);
		if (rc) {
			dev_err(&pdev->dev, "Failed to find the pas_id(rc:%d)\n",
									rc);
			goto err_deregister_bus;
		}
	}

	rc = pil_desc_init(&d->desc);
	if (rc)
		goto err_deregister_bus;

	init_completion(&d->stop_ack);
	init_completion(&d->err_ready);

	d->subsys_desc.name = d->desc.name;
	d->subsys_desc.owner = THIS_MODULE;
	d->subsys_desc.dev = &pdev->dev;
	d->subsys_desc.shutdown = subsys_shutdown;
	d->subsys_desc.powerup = subsys_powerup;
	d->subsys_desc.ramdump = subsys_ramdump;
	d->subsys_desc.free_memory = subsys_free_memory;
	d->subsys_desc.crash_shutdown = subsys_crash_shutdown;

	if (of_property_read_bool(pdev->dev.of_node,
					"qcom,pil-generic-irq-handler")) {

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"rmb_general_purpose");
		d->rmb_gp_reg = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->rmb_gp_reg)) {
			dev_err(&pdev->dev, "Invalid resource for rmb_gp_reg\n");
			rc = PTR_ERR(d->rmb_gp_reg);
			goto load_from_pil;
		}

		rmb_gp_reg_val = __raw_readl(d->rmb_gp_reg);
		/*
		 * If subsystem is already bought out reset during the
		 * bootloader stage, need to check subsystem status instead
		 * of doing regular power. So override power up function
		 * to check subsystem crash status.
		 */
		if (!(rmb_gp_reg_val & BIT(0))) {
			d->boot_enabled = true;
			pr_info("spss is brought out of reset by UEFI\n");
			d->subsys_desc.powerup = subsys_powerup_boot_enabled;
		}
load_from_pil:
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"sp2soc_irq_status");
		d->irq_status = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->irq_status)) {
			dev_err(&pdev->dev, "Invalid resource for sp2soc_irq_status\n");
			rc = PTR_ERR(d->irq_status);
			goto err_ramdump;
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"sp2soc_irq_clr");
		d->irq_clear = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->irq_clear)) {
			dev_err(&pdev->dev, "Invalid resource for sp2soc_irq_clr\n");
			rc = PTR_ERR(d->irq_clear);
			goto err_ramdump;
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"sp2soc_irq_mask");
		d->irq_mask = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->irq_mask)) {
			dev_err(&pdev->dev, "Invalid resource for sp2soc_irq_mask\n");
			rc = PTR_ERR(d->irq_mask);
			goto err_ramdump;
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"rmb_err");
		d->err_status = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->err_status)) {
			dev_err(&pdev->dev, "Invalid resource for rmb_err\n");
			rc = PTR_ERR(d->err_status);
			goto err_ramdump;
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"rmb_err_spare2");
		d->err_status_spare = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(d->err_status_spare)) {
			dev_err(&pdev->dev, "Invalid resource for rmb_err_spare2\n");
			rc = PTR_ERR(d->err_status_spare);
			goto err_ramdump;
		}

		rc = of_property_read_u32_array(pdev->dev.of_node,
				       "qcom,spss-scsr-bits", d->bits_arr,
					ARRAY_SIZE(d->bits_arr));
		if (rc) {
			dev_err(&pdev->dev,
				"Failed to read qcom,spss-scsr-bits(rc:%d)\n",
				rc);
			goto err_ramdump;
		}
		mask_scsr_irqs(d);
	}

	d->desc.signal_aop = of_property_read_bool(pdev->dev.of_node,
						"qcom,signal-aop");
	if (d->desc.signal_aop) {
		d->desc.cl.dev = &pdev->dev;
		d->desc.cl.tx_block = true;
		d->desc.cl.tx_tout = 1000;
		d->desc.cl.knows_txdone = false;
		d->desc.mbox = mbox_request_channel(&d->desc.cl, 0);
		if (IS_ERR(d->desc.mbox)) {
			rc = PTR_ERR(d->desc.mbox);
			dev_err(&pdev->dev, "Failed to get mailbox channel %pK %d\n",
				d->desc.mbox, rc);
			goto err_ramdump;
		}
	}

	d->ramdump_dev = create_ramdump_device(d->subsys_desc.name,
								&pdev->dev);
	if (!d->ramdump_dev) {
		rc = -ENOMEM;
		goto err_ramdump;
	}

	scnprintf(md_node, sizeof(md_node), "md_%s", d->subsys_desc.name);

	d->minidump_dev = create_ramdump_device(md_node, &pdev->dev);
	if (!d->minidump_dev) {
		pr_err("%s: Unable to create a %s minidump device.\n",
				__func__, d->subsys_desc.name);
		rc = -ENOMEM;
		goto err_minidump;
	}

	d->subsys = subsys_register(&d->subsys_desc);
	if (IS_ERR(d->subsys)) {
		rc = PTR_ERR(d->subsys);
		goto err_subsys;
	}

	rc = subsys_setup_irqs(pdev);
	if (rc) {
		subsys_unregister(d->subsys);
		goto err_subsys;
	}

	return 0;
err_subsys:
	destroy_ramdump_device(d->minidump_dev);
err_minidump:
	destroy_ramdump_device(d->ramdump_dev);
err_ramdump:
	pil_desc_release(&d->desc);
	platform_set_drvdata(pdev, NULL);
err_deregister_bus:
	if (d->bus_client)
		icc_put(d->bus_client);

	return rc;
}

static int pil_tz_scm_pas_probe(struct platform_device *pdev)
{
	int ret = 0;

	scm_perf_client = of_icc_get(&pdev->dev, NULL);
	if (IS_ERR(scm_perf_client)) {
		ret = PTR_ERR(scm_perf_client);
		pr_err("scm-pas: Unable to register bus client: %d\n", ret);
	}
	is_inited = 1;

	return ret;
}

static const struct of_device_id pil_tz_match_table[] = {
	{.compatible = "qcom,pil-tz-generic", .data = pil_tz_generic_probe},
	{.compatible = "qcom,pil-tz-scm-pas", .data = pil_tz_scm_pas_probe},
	{}
};

static int pil_tz_driver_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int (*pil_tz_probe)(struct platform_device *pdev);

	match = of_match_node(pil_tz_match_table, pdev->dev.of_node);
	if (!match)
		return -ENODEV;

	pil_tz_probe = match->data;
	return pil_tz_probe(pdev);
}

static int pil_tz_driver_exit(struct platform_device *pdev)
{
	struct pil_tz_data *d = platform_get_drvdata(pdev);
	const struct of_device_id *match;

	match = of_match_node(pil_tz_match_table, pdev->dev.of_node);
	if (!match)
		return -ENODEV;

	if (match->data == pil_tz_scm_pas_probe) {
		icc_put(scm_perf_client);
	} else {
		subsys_unregister(d->subsys);
		destroy_ramdump_device(d->ramdump_dev);
		destroy_ramdump_device(d->minidump_dev);
		pil_desc_release(&d->desc);
		if (d->bus_client)
			icc_put(d->bus_client);
	}

	return 0;
}

static struct platform_driver pil_tz_driver = {
	.probe = pil_tz_driver_probe,
	.remove = pil_tz_driver_exit,
	.driver = {
		.name = "subsys-pil-tz",
		.of_match_table = pil_tz_match_table,
	},
};

static int __init pil_tz_init(void)
{
	return platform_driver_register(&pil_tz_driver);
}
module_init(pil_tz_init);

static void __exit pil_tz_exit(void)
{
	platform_driver_unregister(&pil_tz_driver);
}
module_exit(pil_tz_exit);

MODULE_DESCRIPTION("Support for booting subsystems");
MODULE_LICENSE("GPL v2");
