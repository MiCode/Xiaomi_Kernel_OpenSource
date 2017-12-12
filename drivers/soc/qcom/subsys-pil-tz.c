/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>
#include <linux/dma-mapping.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/scm.h>

#include <soc/qcom/smem.h>

#include "peripheral-loader.h"

#define XO_FREQ			19200000
#define PROXY_TIMEOUT_MS	10000
#define MAX_SSR_REASON_LEN	256U
#define STOP_ACK_TIMEOUT_MS	1000
#define CRASH_STOP_ACK_TO_MS	200

#define ERR_READY	0
#define PBL_DONE	1
#define QDSP6SS_NMI_STATUS	0x44

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
 * @bus_client: bus client id
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
 */
struct pil_tz_data {
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
	u32 pas_id;
	u32 bus_client;
	bool enable_bus_scaling;
	bool keep_proxy_regs_on;
	struct completion stop_ack;
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void __iomem *irq_status;
	void __iomem *irq_clear;
	void __iomem *irq_mask;
	void __iomem *err_status;
	void __iomem *err_status_spare;
	void __iomem *reg_base;
	u32 bits_arr[2];
};

enum scm_cmd {
	PAS_INIT_IMAGE_CMD = 1,
	PAS_MEM_SETUP_CMD,
	PAS_AUTH_AND_RESET_CMD = 5,
	PAS_SHUTDOWN_CMD,
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

static struct msm_bus_paths scm_pas_bw_tbl[] = {
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
			},
		},
		.num_paths = 1,
	},
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
				.ib = 492 * 8 * 1000000UL,
				.ab = 492 * 8 *  100000UL,
			},
		},
		.num_paths = 1,
	},
};

static struct msm_bus_scale_pdata scm_pas_bus_pdata = {
	.usecase = scm_pas_bw_tbl,
	.num_usecases = ARRAY_SIZE(scm_pas_bw_tbl),
	.name = "scm_pas",
};

static uint32_t scm_perf_client;
static int scm_pas_bw_count;
static DEFINE_MUTEX(scm_pas_bw_mutex);

static int scm_pas_enable_bw(void)
{
	int ret = 0;

	if (!scm_perf_client)
		return -EINVAL;

	mutex_lock(&scm_pas_bw_mutex);
	if (!scm_pas_bw_count) {
		ret = msm_bus_scale_client_update_request(scm_perf_client, 1);
		if (ret)
			goto err_bus;
		scm_pas_bw_count++;
	}

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;

err_bus:
	pr_err("scm-pas; Bandwidth request failed (%d)\n", ret);
	msm_bus_scale_client_update_request(scm_perf_client, 0);

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;
}

static void scm_pas_disable_bw(void)
{
	mutex_lock(&scm_pas_bw_mutex);
	if (scm_pas_bw_count-- == 1)
		msm_bus_scale_client_update_request(scm_perf_client, 0);
	mutex_unlock(&scm_pas_bw_mutex);
}

static void scm_pas_init(int id)
{
	static int is_inited;

	if (is_inited)
		return;

	scm_pas_bw_tbl[0].vectors[0].src = id;
	scm_pas_bw_tbl[1].vectors[0].src = id;

	scm_perf_client = msm_bus_scale_register_client(&scm_pas_bus_pdata);
	if (!scm_perf_client)
		pr_warn("scm-pas: Unable to register bus client\n");

	is_inited = 1;
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
				dev_err(dev, "Failed to get %s\n regulator",
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

static int of_read_bus_pdata(struct platform_device *pdev,
			     struct pil_tz_data *d)
{
	struct msm_bus_scale_pdata *pdata;

	pdata = msm_bus_cl_get_pdata(pdev);

	if (!pdata)
		return -EINVAL;

	d->bus_client = msm_bus_scale_register_client(pdata);
	if (!d->bus_client)
		pr_warn("%s: Unable to register bus client\n", __func__);

	return 0;
}

static int piltz_resc_init(struct platform_device *pdev, struct pil_tz_data *d)
{
	int len, count, rc;
	struct device *dev = &pdev->dev;

	count = of_read_clocks(dev, &d->clks, "qcom,active-clock-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup clocks.\n");
		return count;
	}
	d->clk_count = count;

	count = of_read_clocks(dev, &d->proxy_clks, "qcom,proxy-clock-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup proxy clocks.\n");
		return count;
	}
	d->proxy_clk_count = count;

	count = of_read_regs(dev, &d->regs, "qcom,active-reg-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup regulators.\n");
		return count;
	}
	d->reg_count = count;

	count = of_read_regs(dev, &d->proxy_regs, "qcom,proxy-reg-names");
	if (count < 0) {
		dev_err(dev, "Failed to setup proxy regulators.\n");
		return count;
	}
	d->proxy_reg_count = count;

	if (of_find_property(dev->of_node, "qcom,msm-bus,name", &len)) {
		d->enable_bus_scaling = true;
		rc = of_read_bus_pdata(pdev, d);
		if (rc) {
			dev_err(dev, "Failed to setup bus scaling client.\n");
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

	if (d->bus_client) {
		rc = msm_bus_scale_client_update_request(d->bus_client, 1);
		if (rc) {
			dev_err(pil->dev, "bandwidth request failed(rc:%d)\n",
									rc);
			return rc;
		}
	} else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);

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

	if (d->bus_client)
		msm_bus_scale_client_update_request(d->bus_client, 0);
	else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);
}

static int pil_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	struct pil_tz_data *d = desc_to_data(pil);
	struct pas_init_image_req {
		u32	proc;
		u32	image_addr;
	} request;
	u32 scm_ret = 0;
	void *mdata_buf;
	dma_addr_t mdata_phys;
	int ret;
	unsigned long attrs = 0;
	struct device dev = {0};
	struct scm_desc desc = {0};

	if (d->subsys_desc.no_auth)
		return 0;

	ret = scm_pas_enable_bw();
	if (ret)
		return ret;
	arch_setup_dma_ops(&dev, 0, 0, NULL, 0);

	dev.coherent_dma_mask =
		DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	attrs |= DMA_ATTR_STRONGLY_ORDERED;
	mdata_buf = dma_alloc_attrs(&dev, size, &mdata_phys, GFP_KERNEL,
					attrs);
	if (!mdata_buf) {
		pr_err("scm-pas: Allocation for metadata failed.\n");
		scm_pas_disable_bw();
		return -ENOMEM;
	}

	memcpy(mdata_buf, metadata, size);

	request.proc = d->pas_id;
	request.image_addr = mdata_phys;

	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_PIL, PAS_INIT_IMAGE_CMD, &request,
				sizeof(request), &scm_ret, sizeof(scm_ret));
	} else {
		desc.args[0] = d->pas_id;
		desc.args[1] = mdata_phys;
		desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_RW);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL, PAS_INIT_IMAGE_CMD),
				&desc);
		scm_ret = desc.ret[0];
	}

	dma_free_attrs(&dev, size, mdata_buf, mdata_phys, attrs);
	scm_pas_disable_bw();
	if (ret)
		return ret;
	return scm_ret;
}

static int pil_mem_setup_trusted(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	struct pil_tz_data *d = desc_to_data(pil);
	struct pas_init_image_req {
		u32	proc;
		u32	start_addr;
		u32	len;
	} request;
	u32 scm_ret = 0;
	int ret;
	struct scm_desc desc = {0};

	if (d->subsys_desc.no_auth)
		return 0;

	request.proc = d->pas_id;
	request.start_addr = addr;
	request.len = size;

	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_PIL, PAS_MEM_SETUP_CMD, &request,
				sizeof(request), &scm_ret, sizeof(scm_ret));
	} else {
		desc.args[0] = d->pas_id;
		desc.args[1] = addr;
		desc.args[2] = size;
		desc.arginfo = SCM_ARGS(3);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL, PAS_MEM_SETUP_CMD),
				&desc);
		scm_ret = desc.ret[0];
	}
	if (ret)
		return ret;
	return scm_ret;
}

static int pil_auth_and_reset(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	int rc;
	u32 proc, scm_ret = 0;
	struct scm_desc desc = {0};

	if (d->subsys_desc.no_auth)
		return 0;

	desc.args[0] = proc = d->pas_id;
	desc.arginfo = SCM_ARGS(1);

	rc = scm_pas_enable_bw();
	if (rc)
		return rc;

	rc = enable_regulators(d, pil->dev, d->regs, d->reg_count, false);
	if (rc)
		return rc;

	rc = prepare_enable_clocks(pil->dev, d->clks, d->clk_count);
	if (rc)
		goto err_clks;

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_PIL, PAS_AUTH_AND_RESET_CMD, &proc,
				sizeof(proc), &scm_ret, sizeof(scm_ret));
	} else {
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL,
			       PAS_AUTH_AND_RESET_CMD), &desc);
		scm_ret = desc.ret[0];
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
	u32 proc, scm_ret = 0;
	int rc;
	struct scm_desc desc = {0};

	if (d->subsys_desc.no_auth)
		return 0;

	desc.args[0] = proc = d->pas_id;
	desc.arginfo = SCM_ARGS(1);

	if (d->bus_client) {
		rc = msm_bus_scale_client_update_request(d->bus_client, 1);
		if (rc) {
			dev_err(pil->dev, "bandwidth request failed(rc:%d)\n",
									rc);
			return rc;
		}
	} else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);

	rc = enable_regulators(d, pil->dev, d->proxy_regs,
					d->proxy_reg_count, true);
	if (rc)
		goto err_regulators;

	rc = prepare_enable_clocks(pil->dev, d->proxy_clks,
						d->proxy_clk_count);
	if (rc)
		goto err_clks;

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_PIL, PAS_SHUTDOWN_CMD, &proc,
			      sizeof(proc), &scm_ret, sizeof(scm_ret));
	} else {
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL, PAS_SHUTDOWN_CMD),
			       &desc);
		scm_ret = desc.ret[0];
	}

	disable_unprepare_clocks(d->proxy_clks, d->proxy_clk_count);
	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, false);
	if (d->bus_client)
		msm_bus_scale_client_update_request(d->bus_client, 0);
	else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);

	if (rc)
		return rc;

	disable_unprepare_clocks(d->clks, d->clk_count);
	disable_regulators(d, d->regs, d->reg_count, false);

	return scm_ret;

err_clks:
	disable_regulators(d, d->proxy_regs, d->proxy_reg_count, false);
err_regulators:
	if (d->bus_client)
		msm_bus_scale_client_update_request(d->bus_client, 0);
	else
		WARN(d->enable_bus_scaling, "Bus scaling not set up for %s!\n",
					d->subsys_desc.name);
	return rc;
}

static int pil_deinit_image_trusted(struct pil_desc *pil)
{
	struct pil_tz_data *d = desc_to_data(pil);
	u32 proc, scm_ret = 0;
	int rc;
	struct scm_desc desc = {0};

	if (d->subsys_desc.no_auth)
		return 0;

	desc.args[0] = proc = d->pas_id;
	desc.arginfo = SCM_ARGS(1);

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_PIL, PAS_SHUTDOWN_CMD, &proc,
			      sizeof(proc), &scm_ret, sizeof(scm_ret));
	} else {
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL, PAS_SHUTDOWN_CMD),
			       &desc);
		scm_ret = desc.ret[0];
	}

	if (rc)
		return rc;
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
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];
	const char *name = d->subsys_desc.name;

	if (d->smem_id == -1)
		return;

	smem_reason = smem_get_entry_no_rlock(d->smem_id, &size, 0,
							SMEM_ANY_HOST_FLAG);
	if (!smem_reason || !size) {
		pr_err("%s SFR: (unknown, smem_get_entry_no_rlock failed).\n",
									name);
		return;
	}
	if (!smem_reason[0]) {
		pr_err("%s SFR: (unknown, empty string found).\n", name);
		return;
	}

	strlcpy(reason, smem_reason, min(size, MAX_SSR_REASON_LEN));
	pr_err("%s subsystem failure reason: %s.\n", name, reason);

	smem_reason[0] = '\0';
	wmb();
}

static int subsys_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct pil_tz_data *d = subsys_to_data(subsys);
	int ret;

	if (!subsys_get_crash_status(d->subsys) && force_stop &&
						subsys->force_stop_gpio) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		ret = wait_for_completion_timeout(&d->stop_ack,
				msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));
		if (!ret)
			pr_warn("Timed out on stop ack from %s.\n",
							subsys->name);
		gpio_set_value(subsys->force_stop_gpio, 0);
	}

	pil_shutdown(&d->desc);
	return 0;
}

static int subsys_powerup(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);
	int ret = 0;

	if (subsys->stop_ack_irq)
		reinit_completion(&d->stop_ack);

	d->desc.fw_name = subsys->fw_name;
	ret = pil_boot(&d->desc);

	return ret;
}

static int subsys_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&d->desc, d->ramdump_dev);
}

static void subsys_free_memory(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	pil_free_memory(&d->desc);
}

static void subsys_crash_shutdown(const struct subsys_desc *subsys)
{
	struct pil_tz_data *d = subsys_to_data(subsys);

	if (subsys->force_stop_gpio > 0 &&
				!subsys_get_crash_status(d->subsys)) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		mdelay(CRASH_STOP_ACK_TO_MS);
	}
}

static irqreturn_t subsys_err_fatal_intr_handler (int irq, void *dev_id)
{
	struct pil_tz_data *d = subsys_to_data(dev_id);
	u32 nmi_status = 0;

	if (d->reg_base)
		nmi_status = readl_relaxed(d->reg_base +
						QDSP6SS_NMI_STATUS);

	if (nmi_status & 0x04)
		pr_err("%s: Fatal error on the %s due to TZ NMI\n",
			__func__, d->subsys_desc.name);
	else
		pr_err("%s Fatal error on the %s\n",
			__func__, d->subsys_desc.name);

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

static irqreturn_t subsys_wdog_bite_irq_handler(int irq, void *dev_id)
{
	struct pil_tz_data *d = subsys_to_data(dev_id);

	if (subsys_get_crash_status(d->subsys))
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from %s!\n", d->subsys_desc.name);

	if (d->subsys_desc.system_debug &&
			!gpio_get_value(d->subsys_desc.err_fatal_gpio))
		panic("%s: System ramdump requested. Triggering device restart!\n",
							__func__);
	subsys_set_crash_status(d->subsys, CRASH_STATUS_WDOG_BITE);
	log_failure_reason(d);
	subsystem_restart_dev(d->subsys);

	return IRQ_HANDLED;
}

static irqreturn_t subsys_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct pil_tz_data *d = subsys_to_data(dev_id);

	pr_info("Received stop ack interrupt from %s\n", d->subsys_desc.name);
	complete(&d->stop_ack);
	return IRQ_HANDLED;
}

static void clear_pbl_done(struct pil_tz_data *d)
{
	uint32_t err_value;

	err_value =  __raw_readl(d->err_status);
	pr_debug("PBL_DONE received from %s!\n", d->subsys_desc.name);
	if (err_value) {
		uint32_t rmb_err_spare0;
		uint32_t rmb_err_spare1;
		uint32_t rmb_err_spare2;

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
	}
	__raw_writel(BIT(d->bits_arr[PBL_DONE]), d->irq_clear);
}

static void clear_err_ready(struct pil_tz_data *d)
{
	pr_debug("Subsystem error services up received from %s\n",
							d->subsys_desc.name);
	__raw_writel(BIT(d->bits_arr[ERR_READY]), d->irq_clear);
	complete_err_ready(d->subsys);
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

static irqreturn_t subsys_generic_handler(int irq, void *dev_id)
{
	struct pil_tz_data *d = subsys_to_data(dev_id);
	uint32_t status_val, err_value;

	err_value =  __raw_readl(d->err_status_spare);
	status_val = __raw_readl(d->irq_status);

	if ((status_val & BIT(d->bits_arr[ERR_READY])) && !err_value)
		clear_err_ready(d);

	if ((status_val & BIT(d->bits_arr[ERR_READY])) &&
					err_value == 0x44554d50)
		clear_wdog(d);

	if (status_val & BIT(d->bits_arr[PBL_DONE]))
		clear_pbl_done(d);

	return IRQ_HANDLED;
}

static void mask_scsr_irqs(struct pil_tz_data *d)
{
	uint32_t mask_val;
	/* Masking all interrupts not handled by HLOS */
	mask_val = ~0;
	__raw_writel(mask_val & ~BIT(d->bits_arr[ERR_READY]) &
			~BIT(d->bits_arr[PBL_DONE]), d->irq_mask);
}

static int pil_tz_driver_probe(struct platform_device *pdev)
{
	struct pil_tz_data *d;
	struct resource *res;
	u32 proxy_timeout;
	int len, rc;

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	platform_set_drvdata(pdev, d);

	if (of_property_read_bool(pdev->dev.of_node, "qcom,pil-no-auth"))
		d->subsys_desc.no_auth = true;

	d->keep_proxy_regs_on = of_property_read_bool(pdev->dev.of_node,
						"qcom,keep-proxy-regs-on");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_reg");
	d->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(d->reg_base)) {
		dev_err(&pdev->dev, "Failed to ioremap base register\n");
		d->reg_base = NULL;
	}

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

	d->desc.dev = &pdev->dev;
	d->desc.owner = THIS_MODULE;
	d->desc.ops = &pil_ops_trusted;

	d->desc.proxy_timeout = PROXY_TIMEOUT_MS;
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
			return rc;
		}
		scm_pas_init(MSM_BUS_MASTER_CRYPTO_CORE_0);
	}

	rc = pil_desc_init(&d->desc);
	if (rc)
		return rc;

	init_completion(&d->stop_ack);

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
		d->subsys_desc.generic_handler = subsys_generic_handler;
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
		       "qcom,spss-scsr-bits", d->bits_arr, sizeof(d->bits_arr)/
							sizeof(d->bits_arr[0]));
		if (rc) {
			dev_err(&pdev->dev, "Failed to read qcom,spss-scsr-bits");
			goto err_ramdump;
		}
		mask_scsr_irqs(d);

	} else {
		d->subsys_desc.err_fatal_handler =
						subsys_err_fatal_intr_handler;
		d->subsys_desc.wdog_bite_handler = subsys_wdog_bite_irq_handler;
		d->subsys_desc.stop_ack_handler = subsys_stop_ack_intr_handler;
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

	d->subsys = subsys_register(&d->subsys_desc);
	if (IS_ERR(d->subsys)) {
		rc = PTR_ERR(d->subsys);
		goto err_subsys;
	}

	return 0;
err_subsys:
	destroy_ramdump_device(d->ramdump_dev);
err_ramdump:
	pil_desc_release(&d->desc);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

static int pil_tz_driver_exit(struct platform_device *pdev)
{
	struct pil_tz_data *d = platform_get_drvdata(pdev);

	subsys_unregister(d->subsys);
	destroy_ramdump_device(d->ramdump_dev);
	pil_desc_release(&d->desc);

	return 0;
}

static const struct of_device_id pil_tz_match_table[] = {
	{.compatible = "qcom,pil-tz-generic"},
	{}
};

static struct platform_driver pil_tz_driver = {
	.probe = pil_tz_driver_probe,
	.remove = pil_tz_driver_exit,
	.driver = {
		.name = "subsys-pil-tz",
		.of_match_table = pil_tz_match_table,
		.owner = THIS_MODULE,
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
