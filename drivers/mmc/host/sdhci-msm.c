/*
 * drivers/mmc/host/sdhci-msm.c - Qualcomm Technologies, Inc. MSM SDHCI Platform
 * driver source file
 *
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
 *
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/gfp.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/wait.h>

#include "sdhci-pltfm.h"

#define CORE_HC_MODE		0x78
#define HC_MODE_EN		0x1

#define CORE_POWER		0x0
#define CORE_SW_RST		(1 << 7)

#define CORE_PWRCTL_STATUS	0xDC
#define CORE_PWRCTL_MASK	0xE0
#define CORE_PWRCTL_CLEAR	0xE4
#define CORE_PWRCTL_CTL		0xE8

#define CORE_PWRCTL_BUS_OFF	0x01
#define CORE_PWRCTL_BUS_ON	(1 << 1)
#define CORE_PWRCTL_IO_LOW	(1 << 2)
#define CORE_PWRCTL_IO_HIGH	(1 << 3)

#define CORE_PWRCTL_BUS_SUCCESS	0x01
#define CORE_PWRCTL_BUS_FAIL	(1 << 1)
#define CORE_PWRCTL_IO_SUCCESS	(1 << 2)
#define CORE_PWRCTL_IO_FAIL	(1 << 3)

#define INT_MASK		0xF

/* This structure keeps information per regulator */
struct sdhci_msm_reg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage level to be set */
	u32 low_vol_level;
	u32 high_vol_level;
	/* Load values for low power and high power mode */
	u32 lpm_uA;
	u32 hpm_uA;

	/* is this regulator enabled? */
	bool is_enabled;
	/* is this regulator needs to be always on? */
	bool is_always_on;
	/* is low power mode setting required for this regulator? */
	bool lpm_sup;
	bool set_voltage_sup;
};

/*
 * This structure keeps information for all the
 * regulators required for a SDCC slot.
 */
struct sdhci_msm_slot_reg_data {
	/* keeps VDD/VCC regulator info */
	struct sdhci_msm_reg_data *vdd_data;
	 /* keeps VDD IO regulator info */
	struct sdhci_msm_reg_data *vdd_io_data;
};

struct sdhci_msm_gpio {
	u32 no;
	const char *name;
	bool is_enabled;
};

struct sdhci_msm_gpio_data {
	struct sdhci_msm_gpio *gpio;
	u8 size;
};

struct sdhci_msm_pin_data {
	/*
	 * = 1 if controller pins are using gpios
	 * = 0 if controller has dedicated MSM pads
	 */
	bool cfg_sts;
	struct sdhci_msm_gpio_data *gpio_data;
};

struct sdhci_msm_pltfm_data {
	/* Supported UHS-I Modes */
	u32 caps;

	/* More capabilities */
	u32 caps2;

	unsigned long mmc_bus_width;
	u32 max_clk;
	struct sdhci_msm_slot_reg_data *vreg_data;
	bool nonremovable;
	struct sdhci_msm_pin_data *pin_data;
};

struct sdhci_msm_host {
	void __iomem *core_mem;    /* MSM SDCC mapped address */
	struct clk	 *clk;     /* main SD/MMC bus clock */
	struct clk	 *pclk;    /* SDHC peripheral bus clock */
	struct clk	 *bus_clk; /* SDHC bus voter clock */
	struct sdhci_msm_pltfm_data *pdata;
	struct mmc_host  *mmc;
	struct sdhci_pltfm_data sdhci_msm_pdata;
	wait_queue_head_t pwr_irq_wait;
};

enum vdd_io_level {
	/* set vdd_io_data->low_vol_level */
	VDD_IO_LOW,
	/* set vdd_io_data->high_vol_level */
	VDD_IO_HIGH,
	/*
	 * set whatever there in voltage_level (third argument) of
	 * sdhci_msm_set_vdd_io_vol() function.
	 */
	VDD_IO_SET_LEVEL,
};

static int sdhci_msm_setup_gpio(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	struct sdhci_msm_gpio_data *curr;
	int i, ret = 0;

	curr = pdata->pin_data->gpio_data;
	for (i = 0; i < curr->size; i++) {
		if (!gpio_is_valid(curr->gpio[i].no)) {
			ret = -EINVAL;
			pr_err("%s: Invalid gpio = %d\n", __func__,
					curr->gpio[i].no);
			goto free_gpios;
		}
		if (enable) {
			ret = gpio_request(curr->gpio[i].no,
						curr->gpio[i].name);
			if (ret) {
				pr_err("%s: gpio_request(%d, %s) failed %d\n",
					__func__, curr->gpio[i].no,
					curr->gpio[i].name, ret);
				goto free_gpios;
			}
			curr->gpio[i].is_enabled = true;
		} else {
			gpio_free(curr->gpio[i].no);
			curr->gpio[i].is_enabled = false;
		}
	}
	return ret;

free_gpios:
	for (i--; i >= 0; i--) {
		gpio_free(curr->gpio[i].no);
		curr->gpio[i].is_enabled = false;
	}
	return ret;
}

static int sdhci_msm_setup_pins(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	int ret = 0;

	if (!pdata->pin_data || (pdata->pin_data->cfg_sts == enable))
		return 0;

	ret = sdhci_msm_setup_gpio(pdata, enable);
	if (!ret)
		pdata->pin_data->cfg_sts = enable;

	return ret;
}

#define MAX_PROP_SIZE 32
static int sdhci_msm_dt_parse_vreg_info(struct device *dev,
		struct sdhci_msm_reg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct sdhci_msm_reg_data *vreg;
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_err(dev, "No vreg data found for %s\n", vreg_name);
		ret = -EINVAL;
		return ret;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		dev_err(dev, "No memory for vreg: %s\n", vreg_name);
		ret = -ENOMEM;
		return ret;
	}

	vreg->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-always-on", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->is_always_on = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-lpm-sup", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->lpm_sup = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-voltage-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->low_vol_level = be32_to_cpup(&prop[0]);
		vreg->high_vol_level = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-current-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->lpm_uA = be32_to_cpup(&prop[0]);
		vreg->hpm_uA = be32_to_cpup(&prop[1]);
	}

	*vreg_data = vreg;
	dev_dbg(dev, "%s: %s %s vol=[%d %d]uV, curr=[%d %d]uA\n",
		vreg->name, vreg->is_always_on ? "always_on," : "",
		vreg->lpm_sup ? "lpm_sup," : "", vreg->low_vol_level,
		vreg->high_vol_level, vreg->lpm_uA, vreg->hpm_uA);

	return ret;
}

#define GPIO_NAME_MAX_LEN 32
static int sdhci_msm_dt_parse_gpio_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	int ret = 0, cnt, i;
	struct sdhci_msm_pin_data *pin_data;
	struct device_node *np = dev->of_node;

	pin_data = devm_kzalloc(dev, sizeof(*pin_data), GFP_KERNEL);
	if (!pin_data) {
		dev_err(dev, "No memory for pin_data\n");
		ret = -ENOMEM;
		goto out;
	}

	cnt = of_gpio_count(np);
	if (cnt > 0) {
		pin_data->gpio_data = devm_kzalloc(dev,
				sizeof(struct sdhci_msm_gpio_data), GFP_KERNEL);
		if (!pin_data->gpio_data) {
			dev_err(dev, "No memory for gpio_data\n");
			ret = -ENOMEM;
			goto out;
		}
		pin_data->gpio_data->size = cnt;
		pin_data->gpio_data->gpio = devm_kzalloc(dev, cnt *
				sizeof(struct sdhci_msm_gpio), GFP_KERNEL);

		if (!pin_data->gpio_data->gpio) {
			dev_err(dev, "No memory for gpio\n");
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < cnt; i++) {
			const char *name = NULL;
			char result[GPIO_NAME_MAX_LEN];
			pin_data->gpio_data->gpio[i].no = of_get_gpio(np, i);
			of_property_read_string_index(np,
					"qcom,gpio-names", i, &name);

			snprintf(result, GPIO_NAME_MAX_LEN, "%s-%s",
					dev_name(dev), name ? name : "?");
			pin_data->gpio_data->gpio[i].name = result;
			dev_dbg(dev, "%s: gpio[%s] = %d\n", __func__,
					pin_data->gpio_data->gpio[i].name,
					pin_data->gpio_data->gpio[i].no);
			pdata->pin_data = pin_data;
		}
	}

out:
	if (ret)
		dev_err(dev, "%s failed with err %d\n", __func__, ret);
	return ret;
}

/* Parse platform data */
static struct sdhci_msm_pltfm_data *sdhci_msm_populate_pdata(struct device *dev)
{
	struct sdhci_msm_pltfm_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	int len, i;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate memory for platform data\n");
		goto out;
	}

	of_property_read_u32(np, "qcom,bus-width", &bus_width);
	if (bus_width == 8)
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	else if (bus_width == 4)
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	else {
		dev_notice(dev, "invalid bus-width, default to 1-bit mode\n");
		pdata->mmc_bus_width = 0;
	}

	pdata->vreg_data = devm_kzalloc(dev, sizeof(struct
						    sdhci_msm_slot_reg_data),
					GFP_KERNEL);
	if (!pdata->vreg_data) {
		dev_err(dev, "failed to allocate memory for vreg data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_vreg_info(dev, &pdata->vreg_data->vdd_data,
					 "vdd")) {
		dev_err(dev, "failed parsing vdd data\n");
		goto out;
	}
	if (sdhci_msm_dt_parse_vreg_info(dev,
					 &pdata->vreg_data->vdd_io_data,
					 "vdd-io")) {
		dev_err(dev, "failed parsing vdd-io data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_gpio_info(dev, pdata)) {
		dev_err(dev, "failed parsing gpio data\n");
		goto out;
	}

	of_property_read_u32(np, "qcom,max-clk-rate", &pdata->max_clk);

	len = of_property_count_strings(np, "qcom,bus-speed-mode");

	for (i = 0; i < len; i++) {
		const char *name = NULL;

		of_property_read_string_index(np,
			"qcom,bus-speed-mode", i, &name);
		if (!name)
			continue;

		if (!strncmp(name, "HS200_1p8v", sizeof("HS200_1p8v")))
			pdata->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
		else if (!strncmp(name, "HS200_1p2v", sizeof("HS200_1p2v")))
			pdata->caps2 |= MMC_CAP2_HS200_1_2V_SDR;
		else if (!strncmp(name, "DDR_1p8v", sizeof("DDR_1p8v")))
			pdata->caps |= MMC_CAP_1_8V_DDR
						| MMC_CAP_UHS_DDR50;
		else if (!strncmp(name, "DDR_1p2v", sizeof("DDR_1p2v")))
			pdata->caps |= MMC_CAP_1_2V_DDR
						| MMC_CAP_UHS_DDR50;
	}

	if (of_get_property(np, "qcom,nonremovable", NULL))
		pdata->nonremovable = true;

	return pdata;
out:
	return NULL;
}

/* Regulator utility functions */
static int sdhci_msm_vreg_init_reg(struct device *dev,
					struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* check if regulator is already initialized? */
	if (vreg->reg)
		goto out;

	/* Get the regulator handle */
	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		pr_err("%s: devm_regulator_get(%s) failed. ret=%d\n",
			__func__, vreg->name, ret);
		goto out;
	}

	/* sanity check */
	if (!vreg->high_vol_level || !vreg->hpm_uA) {
		pr_err("%s: %s invalid constraints specified\n",
		       __func__, vreg->name);
		ret = -EINVAL;
	}

out:
	return ret;
}

static void sdhci_msm_vreg_deinit_reg(struct sdhci_msm_reg_data *vreg)
{
	if (vreg->reg)
		devm_regulator_put(vreg->reg);
}

static int sdhci_msm_vreg_set_optimum_mode(struct sdhci_msm_reg_data
						  *vreg, int uA_load)
{
	int ret = 0;
	
	/*
	 * regulators that do not support regulator_set_voltage also
	 * do not support regulator_set_optimum_mode
	 */
	if (vreg->set_voltage_sup) {
		ret = regulator_set_load(vreg->reg, uA_load);
		if (ret < 0)
			pr_err("%s: regulator_set_load(reg=%s,uA_load=%d) failed. ret=%d\n",
			       __func__, vreg->name, uA_load, ret);
		else
			/*
			 * regulator_set_load() can return non zero
			 * value even for success case.
			 */
			ret = 0;
	}
	return ret;
}

static int sdhci_msm_vreg_set_voltage(struct sdhci_msm_reg_data *vreg,
					int min_uV, int max_uV)
{
	int ret = 0;

	ret = regulator_set_voltage(vreg->reg, min_uV, max_uV);
	if (ret) {
		pr_err("%s: regulator_set_voltage(%s)failed. min_uV=%d,max_uV=%d,ret=%d\n",
			       __func__, vreg->name, min_uV, max_uV, ret);
		}

	return ret;
}

static int sdhci_msm_vreg_enable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Put regulator in HPM (high power mode) */
	ret = sdhci_msm_vreg_set_optimum_mode(vreg, vreg->hpm_uA);
	if (ret < 0)
		return ret;

	if (!vreg->is_enabled) {
		/* Set voltage level */
		ret = sdhci_msm_vreg_set_voltage(vreg, vreg->high_vol_level,
						vreg->high_vol_level);
		if (ret)
			return ret;
	}
	ret = regulator_enable(vreg->reg);
	if (ret) {
		pr_err("%s: regulator_enable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
		return ret;
	}
	vreg->is_enabled = true;
	return ret;
}

static int sdhci_msm_vreg_disable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Never disable regulator marked as always_on */
	if (vreg->is_enabled && !vreg->is_always_on) {
		ret = regulator_disable(vreg->reg);
		if (ret) {
			pr_err("%s: regulator_disable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
			goto out;
		}
		vreg->is_enabled = false;

		ret = sdhci_msm_vreg_set_optimum_mode(vreg, 0);
		if (ret < 0)
			goto out;

		/* Set min. voltage level to 0 */
		ret = sdhci_msm_vreg_set_voltage(vreg, 0, vreg->high_vol_level);
		if (ret)
			goto out;
	} else if (vreg->is_enabled && vreg->is_always_on) {
		if (vreg->lpm_sup) {
			/* Put always_on regulator in LPM (low power mode) */
			ret = sdhci_msm_vreg_set_optimum_mode(vreg,
							      vreg->lpm_uA);
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

static int sdhci_msm_setup_vreg(struct sdhci_msm_pltfm_data *pdata,
			bool enable, bool is_init)
{
	int ret = 0, i;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *vreg_table[2];

	curr_slot = pdata->vreg_data;
	if (!curr_slot) {
		pr_debug("%s: vreg info unavailable,assuming the slot is powered by always on domain\n",
			 __func__);
		goto out;
	}

	vreg_table[0] = curr_slot->vdd_data;
	vreg_table[1] = curr_slot->vdd_io_data;

	for (i = 0; i < ARRAY_SIZE(vreg_table); i++) {
		if (vreg_table[i]) {
			if (enable)
				ret = sdhci_msm_vreg_enable(vreg_table[i]);
			else
				ret = sdhci_msm_vreg_disable(vreg_table[i]);
			if (ret)
				goto out;
		}
	}
out:
	return ret;
}

/*
 * Reset vreg by ensuring it is off during probe. A call
 * to enable vreg is needed to balance disable vreg
 */
static int sdhci_msm_vreg_reset(struct sdhci_msm_pltfm_data *pdata)
{
	int ret;

	ret = sdhci_msm_setup_vreg(pdata, 1, true);
	if (ret)
		return ret;
	ret = sdhci_msm_setup_vreg(pdata, 0, true);
	return ret;
}

/* This init function should be called only once for each SDHC slot */
static int sdhci_msm_vreg_init(struct device *dev,
				struct sdhci_msm_pltfm_data *pdata,
				bool is_init)
{
	int ret = 0;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *curr_vdd_reg, *curr_vdd_io_reg;

	curr_slot = pdata->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = curr_slot->vdd_data;
	curr_vdd_io_reg = curr_slot->vdd_io_data;

	if (!is_init)
		/* Deregister all regulators from regulator framework */
		goto vdd_io_reg_deinit;

	/*
	 * Get the regulator handle from voltage regulator framework
	 * and then try to set the voltage level for the regulator
	 */
	if (curr_vdd_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_reg);
		if (ret)
			goto out;
	}
	if (curr_vdd_io_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_io_reg);
		if (ret)
			goto vdd_reg_deinit;
	}
	ret = sdhci_msm_vreg_reset(pdata);
	if (ret)
		dev_err(dev, "vreg reset failed (%d)\n", ret);
	goto out;

vdd_io_reg_deinit:
	if (curr_vdd_io_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_io_reg);
vdd_reg_deinit:
	if (curr_vdd_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_reg);
out:
	return ret;
}


static int sdhci_msm_set_vdd_io_vol(struct sdhci_msm_pltfm_data *pdata,
			enum vdd_io_level level,
			unsigned int voltage_level)
{
	int ret = 0;
	int set_level;
	struct sdhci_msm_reg_data *vdd_io_reg;

	if (!pdata->vreg_data)
		return ret;

	vdd_io_reg = pdata->vreg_data->vdd_io_data;
	if (vdd_io_reg && vdd_io_reg->is_enabled) {
		switch (level) {
		case VDD_IO_LOW:
			set_level = vdd_io_reg->low_vol_level;
			break;
		case VDD_IO_HIGH:
			set_level = vdd_io_reg->high_vol_level;
			break;
		case VDD_IO_SET_LEVEL:
			set_level = voltage_level;
			break;
		default:
			pr_err("%s: invalid argument level = %d",
					__func__, level);
			ret = -EINVAL;
			return ret;
		}
		ret = sdhci_msm_vreg_set_voltage(vdd_io_reg, set_level,
				set_level);
	}
	return ret;
}

static irqreturn_t sdhci_msm_pwr_irq(int irq, void *data)
{
	struct sdhci_msm_host *msm_host = (struct sdhci_msm_host *)data;
	u8 irq_status = 0;
	u8 irq_ack = 0;
	int ret = 0;

	irq_status = readb_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS);
	pr_debug("%s: Received IRQ(%d), status=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, irq_status);

	/* Clear the interrupt */
	writeb_relaxed(irq_status, (msm_host->core_mem + CORE_PWRCTL_CLEAR));
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	/* Handle BUS ON/OFF*/
	if (irq_status & CORE_PWRCTL_BUS_ON) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, true, false);
		if (!ret)
			ret = sdhci_msm_setup_pins(msm_host->pdata, true);
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;
	}
	if (irq_status & CORE_PWRCTL_BUS_OFF) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, false, false);
		if (!ret)
			ret = sdhci_msm_setup_pins(msm_host->pdata, false);
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;
	}
	/* Handle IO LOW/HIGH */
	if (irq_status & CORE_PWRCTL_IO_LOW) {
		/* Switch voltage Low */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_LOW, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;
	}
	if (irq_status & CORE_PWRCTL_IO_HIGH) {
		/* Switch voltage High */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_HIGH, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;
	}

	/* ACK status to the core */
	writeb_relaxed(irq_ack, (msm_host->core_mem + CORE_PWRCTL_CTL));
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	pr_debug("%s: Handled IRQ(%d), ret=%d, ack=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, ret, irq_ack);
	wake_up_interruptible(&msm_host->pwr_irq_wait);
	return IRQ_HANDLED;
}

static void sdhci_msm_check_power_status(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;

	pr_debug("%s: %s: power status before waiting 0x%x\n",
		mmc_hostname(host->mmc), __func__,
		readb_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));

	ret = wait_event_interruptible(msm_host->pwr_irq_wait,
				       (readb_relaxed(msm_host->core_mem +
						      CORE_PWRCTL_CTL)) != 0x0);
	if (ret)
		pr_warning("%s: %s: returned due to error %d\n",
				mmc_hostname(host->mmc), __func__, ret);
	pr_debug("%s: %s: ret %d power status after handling power IRQ 0x%x\n",
		mmc_hostname(host->mmc), __func__, ret,
		readb_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));
}

static struct sdhci_ops sdhci_msm_ops = {
	.check_power_status = sdhci_msm_check_power_status,
};

static int sdhci_msm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_msm_host *msm_host;
	struct resource *core_memres = NULL;
	int ret = 0, pwr_irq = 0, dead = 0;

	pr_debug("%s: Enter %s\n", dev_name(&pdev->dev), __func__);
	msm_host = devm_kzalloc(&pdev->dev, sizeof(struct sdhci_msm_host),
				GFP_KERNEL);
	if (!msm_host) {
		ret = -ENOMEM;
		goto out;
	}
	init_waitqueue_head(&msm_host->pwr_irq_wait);

	msm_host->sdhci_msm_pdata.ops = &sdhci_msm_ops;
	host = sdhci_pltfm_init(pdev, &msm_host->sdhci_msm_pdata, 0);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto out;
	}

	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = msm_host;
	msm_host->mmc = host->mmc;

	/* Extract platform data */
	if (pdev->dev.of_node) {
		msm_host->pdata = sdhci_msm_populate_pdata(&pdev->dev);
		if (!msm_host->pdata) {
			dev_err(&pdev->dev, "DT parsing error\n");
			goto pltfm_free;
		}
	} else {
		dev_err(&pdev->dev, "No device tree node\n");
		goto pltfm_free;
	}

	/* Setup Clocks */

	/* Setup SDCC bus voter clock. */
	msm_host->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (!IS_ERR_OR_NULL(msm_host->bus_clk)) {
		/* Vote for max. clk rate for max. performance */
		ret = clk_set_rate(msm_host->bus_clk, INT_MAX);
		if (ret)
			goto pltfm_free;
		ret = clk_prepare_enable(msm_host->bus_clk);
		if (ret)
			goto pltfm_free;
	}

	/* Setup main peripheral bus clock */
	msm_host->pclk = devm_clk_get(&pdev->dev, "iface_clk");
	if (!IS_ERR(msm_host->pclk)) {
		ret = clk_prepare_enable(msm_host->pclk);
		if (ret)
			goto bus_clk_disable;
	}

	/* Setup SDC MMC clock */
	msm_host->clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_host->clk)) {
		ret = PTR_ERR(msm_host->clk);
		goto pclk_disable;
	}

	ret = clk_prepare_enable(msm_host->clk);
	if (ret)
		goto pclk_disable;

	/* Setup regulators */
	ret = sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, true);
	if (ret) {
		dev_err(&pdev->dev, "Regulator setup failed (%d)\n", ret);
		goto clk_disable;
	}

	/* Reset the core and Enable SDHC mode */
	core_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "core_mem");
	msm_host->core_mem = devm_ioremap(&pdev->dev, core_memres->start,
					resource_size(core_memres));

	if (!msm_host->core_mem) {
		dev_err(&pdev->dev, "Failed to remap registers\n");
		ret = -ENOMEM;
		goto vreg_deinit;
	}

	/* Set SW_RST bit in POWER register (Offset 0x0) */
	writel_relaxed(CORE_SW_RST, msm_host->core_mem + CORE_POWER);
	/* Set HC_MODE_EN bit in HC_MODE register */
	writel_relaxed(HC_MODE_EN, (msm_host->core_mem + CORE_HC_MODE));

	/*
	 * Following are the deviations from SDHC spec v3.0 -
	 * 1. Card detection is handled using separate GPIO.
	 * 2. Bus power control is handled by interacting with PMIC.
	 */
	host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
	host->quirks |= SDHCI_QUIRK_SINGLE_POWER_WRITE;

	pwr_irq = platform_get_irq_byname(pdev, "pwr_irq");
	if (pwr_irq < 0) {
		dev_err(&pdev->dev, "Failed to get pwr_irq by name (%d)\n",
				pwr_irq);
		goto vreg_deinit;
	}
	ret = devm_request_threaded_irq(&pdev->dev, pwr_irq, NULL,
					sdhci_msm_pwr_irq, IRQF_ONESHOT,
					dev_name(&pdev->dev), msm_host);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq(%d) failed (%d)\n",
				pwr_irq, ret);
		goto vreg_deinit;
	}

	/* Enable pwr irq interrupts */
	writel_relaxed(INT_MASK, (msm_host->core_mem + CORE_PWRCTL_MASK));

	/* Set host capabilities */
	msm_host->mmc->caps |= msm_host->pdata->mmc_bus_width;
	msm_host->mmc->caps |= msm_host->pdata->caps;

	msm_host->mmc->caps2 |= msm_host->pdata->caps2;
	msm_host->mmc->caps2 |= MMC_CAP2_PACKED_WR;
	msm_host->mmc->caps2 |= MMC_CAP2_PACKED_WR_CONTROL;

	if (msm_host->pdata->nonremovable)
		msm_host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(&pdev->dev, "Add host failed (%d)\n", ret);
		goto vreg_deinit;
	}

	 /* Set core clk rate, optionally override from dts */
	if (msm_host->pdata->max_clk)
		host->max_clk = msm_host->pdata->max_clk;
	ret = clk_set_rate(msm_host->clk, host->max_clk);
	if (ret) {
		dev_err(&pdev->dev, "MClk rate set failed (%d)\n", ret);
		goto remove_host;
	}

	/* Successful initialization */
	goto out;

remove_host:
	dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);
	sdhci_remove_host(host, dead);
vreg_deinit:
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
clk_disable:
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
pclk_disable:
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
bus_clk_disable:
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
pltfm_free:
	sdhci_pltfm_free(pdev);
out:
	pr_debug("%s: Exit %s\n", dev_name(&pdev->dev), __func__);
	return ret;
}

static int sdhci_msm_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pltfm_data *pdata = msm_host->pdata;
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) ==
			0xffffffff);

	pr_debug("%s: %s\n", dev_name(&pdev->dev), __func__);
	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
	if (pdata->pin_data)
		sdhci_msm_setup_gpio(pdata, false);
	return 0;
}

static const struct of_device_id sdhci_msm_dt_match[] = {
	{.compatible = "qcom,sdhci-msm"},
};
MODULE_DEVICE_TABLE(of, sdhci_msm_dt_match);

static struct platform_driver sdhci_msm_driver = {
	.probe		= sdhci_msm_probe,
	.remove		= sdhci_msm_remove,
	.driver		= {
		.name	= "sdhci_msm",
		.owner	= THIS_MODULE,
		.of_match_table = sdhci_msm_dt_match,
	},
};

module_platform_driver(sdhci_msm_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Secure Digital Host Controller Interface driver");
MODULE_LICENSE("GPL v2");
