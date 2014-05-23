/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/wcnss_wlan.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/clk.h>


static void __iomem *msm_wcnss_base;
static LIST_HEAD(power_on_lock_list);
static DEFINE_MUTEX(list_lock);
static DEFINE_SEMAPHORE(wcnss_power_on_lock);
static int auto_detect;
static int is_power_on;

#define RIVA_PMU_OFFSET         0x28
#define PRONTO_PMU_OFFSET       0x1004

#define RIVA_SPARE_OFFSET       0x0b4
#define PRONTO_SPARE_OFFSET     0x1088
#define NVBIN_DLND_BIT          BIT(25)

#define PRONTO_IRIS_REG_READ_OFFSET       0x1134
#define PRONTO_IRIS_REG_CHIP_ID           0x04

#define WCNSS_PMU_CFG_IRIS_XO_CFG          BIT(3)
#define WCNSS_PMU_CFG_IRIS_XO_EN           BIT(4)
#define WCNSS_PMU_CFG_GC_BUS_MUX_SEL_TOP   BIT(5)
#define WCNSS_PMU_CFG_IRIS_XO_CFG_STS      BIT(6) /* 1: in progress, 0: done */

#define WCNSS_PMU_CFG_IRIS_RESET           BIT(7)
#define WCNSS_PMU_CFG_IRIS_RESET_STS       BIT(8) /* 1: in progress, 0: done */
#define WCNSS_PMU_CFG_IRIS_XO_READ         BIT(9)
#define WCNSS_PMU_CFG_IRIS_XO_READ_STS     BIT(10)

#define WCNSS_PMU_CFG_IRIS_XO_MODE         0x6
#define WCNSS_PMU_CFG_IRIS_XO_MODE_48      (3 << 1)

#define VREG_NULL_CONFIG            0x0000
#define VREG_GET_REGULATOR_MASK     0x0001
#define VREG_SET_VOLTAGE_MASK       0x0002
#define VREG_OPTIMUM_MODE_MASK      0x0004
#define VREG_ENABLE_MASK            0x0008

#define WCNSS_INVALID_IRIS_REG      0xbaadbaad

struct vregs_info {
	const char * const name;
	int state;
	const int nominal_min;
	const int low_power_min;
	const int max_voltage;
	const int uA_load;
	struct regulator *regulator;
};

/* IRIS regulators for Riva hardware */
static struct vregs_info iris_vregs_riva[] = {
	{"iris_vddxo",  VREG_NULL_CONFIG, 1800000, 0, 1800000, 10000,  NULL},
	{"iris_vddrfa", VREG_NULL_CONFIG, 1300000, 0, 1300000, 100000, NULL},
	{"iris_vddpa",  VREG_NULL_CONFIG, 2900000, 0, 3000000, 515000, NULL},
	{"iris_vdddig", VREG_NULL_CONFIG, 1200000, 0, 1225000, 10000,  NULL},
};

/* WCNSS regulators for Riva hardware */
static struct vregs_info riva_vregs[] = {
	/* Riva */
	{"riva_vddmx",  VREG_NULL_CONFIG, 1050000, 0, 1150000, 0,      NULL},
	{"riva_vddcx",  VREG_NULL_CONFIG, 1050000, 0, 1150000, 0,      NULL},
	{"riva_vddpx",  VREG_NULL_CONFIG, 1800000, 0, 1800000, 0,      NULL},
};

/* IRIS regulators for Pronto hardware */
static struct vregs_info iris_vregs_pronto[] = {
	{"qcom,iris-vddxo",  VREG_NULL_CONFIG, 1800000, 0,
		1800000, 10000,  NULL},
	{"qcom,iris-vddrfa", VREG_NULL_CONFIG, 1300000, 0,
		1300000, 100000, NULL},
	{"qcom,iris-vddpa",  VREG_NULL_CONFIG, 2900000, 0,
		3000000, 515000, NULL},
	{"qcom,iris-vdddig", VREG_NULL_CONFIG, 1225000, 0,
		1800000, 10000,  NULL},
};

/* WCNSS regulators for Pronto hardware */
static struct vregs_info pronto_vregs[] = {
	{"qcom,pronto-vddmx",  VREG_NULL_CONFIG, 950000,  0,
		1150000, 0,    NULL},
	{"qcom,pronto-vddcx",  VREG_NULL_CONFIG, RPM_REGULATOR_CORNER_NORMAL,
		RPM_REGULATOR_CORNER_NONE, RPM_REGULATOR_CORNER_SUPER_TURBO,
		0,             NULL},
	{"qcom,pronto-vddpx",  VREG_NULL_CONFIG, 1800000, 0,
		1800000, 0,    NULL},
};

/* IRIS regulators for Pronto v2 hardware */
static struct vregs_info iris_vregs_pronto_v2[] = {
	{"qcom,iris-vddxo",  VREG_NULL_CONFIG, 1800000, 0,
		1800000, 10000,  NULL},
	{"qcom,iris-vddrfa", VREG_NULL_CONFIG, 1300000, 0,
		1300000, 100000, NULL},
	{"qcom,iris-vddpa",  VREG_NULL_CONFIG, 3300000, 0,
		3300000, 515000, NULL},
	{"qcom,iris-vdddig", VREG_NULL_CONFIG, 1800000, 0,
		1800000, 10000,  NULL},
};

/* WCNSS regulators for Pronto v2 hardware */
static struct vregs_info pronto_vregs_pronto_v2[] = {
	{"qcom,pronto-vddmx",  VREG_NULL_CONFIG, 1287500,  0,
		1287500, 0,    NULL},
	{"qcom,pronto-vddcx",  VREG_NULL_CONFIG, RPM_REGULATOR_CORNER_NORMAL,
		RPM_REGULATOR_CORNER_NONE, RPM_REGULATOR_CORNER_SUPER_TURBO,
		0,             NULL},
	{"qcom,pronto-vddpx",  VREG_NULL_CONFIG, 1800000, 0,
		1800000, 0,    NULL},
};


struct host_driver {
	char name[20];
	struct list_head list;
};

enum {
	IRIS_3660, /* also 3660A and 3680 */
	IRIS_3620
};


int xo_auto_detect(u32 reg)
{
	reg >>= 30;

	switch (reg) {
	case IRIS_3660:
		return WCNSS_XO_48MHZ;

	case IRIS_3620:
		return WCNSS_XO_19MHZ;

	default:
		return WCNSS_XO_INVALID;
	}
}

static int
configure_iris_xo(struct device *dev,
			struct wcnss_wlan_config *cfg,
			int on, int *iris_xo_set)
{
	u32 reg = 0;
	u32 iris_reg = WCNSS_INVALID_IRIS_REG;
	int rc = 0;
	int pmu_offset = 0;
	int spare_offset = 0;
	void __iomem *pmu_conf_reg;
	void __iomem *spare_reg;
	void __iomem *iris_read_reg;
	struct clk *clk;
	struct clk *clk_rf = NULL;
	bool use_48mhz_xo;

	use_48mhz_xo = cfg->use_48mhz_xo;

	if (wcnss_hardware_type() == WCNSS_PRONTO_HW) {
		pmu_offset = PRONTO_PMU_OFFSET;
		spare_offset = PRONTO_SPARE_OFFSET;

		clk = clk_get(dev, "xo");
		if (IS_ERR(clk)) {
			pr_err("Couldn't get xo clock\n");
			return PTR_ERR(clk);
		}

	} else {
		pmu_offset = RIVA_PMU_OFFSET;
		spare_offset = RIVA_SPARE_OFFSET;

		clk = clk_get(dev, "cxo");
		if (IS_ERR(clk)) {
			pr_err("Couldn't get cxo clock\n");
			return PTR_ERR(clk);
		}
	}

	if (on) {
		msm_wcnss_base = cfg->msm_wcnss_base;
		if (!msm_wcnss_base) {
			pr_err("ioremap wcnss physical failed\n");
			goto fail;
		}

		/* Enable IRIS XO */
		rc = clk_prepare_enable(clk);
		if (rc) {
			pr_err("clk enable failed\n");
			goto fail;
		}

		/* NV bit is set to indicate that platform driver is capable
		 * of doing NV download.
		 */
		pr_debug("wcnss: Indicate NV bin download\n");
		spare_reg = msm_wcnss_base + spare_offset;
		reg = readl_relaxed(spare_reg);
		reg |= NVBIN_DLND_BIT;
		writel_relaxed(reg, spare_reg);

		pmu_conf_reg = msm_wcnss_base + pmu_offset;
		writel_relaxed(0, pmu_conf_reg);
		reg = readl_relaxed(pmu_conf_reg);
		reg |= WCNSS_PMU_CFG_GC_BUS_MUX_SEL_TOP |
				WCNSS_PMU_CFG_IRIS_XO_EN;
		writel_relaxed(reg, pmu_conf_reg);

		if (wcnss_xo_auto_detect_enabled()) {
			iris_read_reg = msm_wcnss_base +
				PRONTO_IRIS_REG_READ_OFFSET;
			iris_reg = readl_relaxed(iris_read_reg);
		}

		if (iris_reg != WCNSS_INVALID_IRIS_REG) {
			iris_reg &= 0xffff;
			iris_reg |= PRONTO_IRIS_REG_CHIP_ID;
			writel_relaxed(iris_reg, iris_read_reg);

			/* Iris read */
			reg = readl_relaxed(pmu_conf_reg);
			reg |= WCNSS_PMU_CFG_IRIS_XO_READ;
			writel_relaxed(reg, pmu_conf_reg);

			/* Wait for PMU_CFG.iris_reg_read_sts */
			while (readl_relaxed(pmu_conf_reg) &
					WCNSS_PMU_CFG_IRIS_XO_READ_STS)
				cpu_relax();

			iris_reg = readl_relaxed(iris_read_reg);
			auto_detect = xo_auto_detect(iris_reg);

			/* Reset iris read bit */
			reg &= ~WCNSS_PMU_CFG_IRIS_XO_READ;

		} else if (wcnss_xo_auto_detect_enabled())
			/* Default to 48 MHZ */
			auto_detect = WCNSS_XO_48MHZ;
		else
			auto_detect = WCNSS_XO_INVALID;

		/* Clear XO_MODE[b2:b1] bits. Clear implies 19.2 MHz TCXO */
		reg &= ~(WCNSS_PMU_CFG_IRIS_XO_MODE);

		if ((use_48mhz_xo && auto_detect == WCNSS_XO_INVALID)
				|| auto_detect ==  WCNSS_XO_48MHZ) {
			reg |= WCNSS_PMU_CFG_IRIS_XO_MODE_48;

			if (iris_xo_set)
				*iris_xo_set = WCNSS_XO_48MHZ;
		}

		writel_relaxed(reg, pmu_conf_reg);

		/* Reset IRIS */
		reg |= WCNSS_PMU_CFG_IRIS_RESET;
		writel_relaxed(reg, pmu_conf_reg);

		/* Wait for PMU_CFG.iris_reg_reset_sts */
		while (readl_relaxed(pmu_conf_reg) &
				WCNSS_PMU_CFG_IRIS_RESET_STS)
			cpu_relax();

		/* Reset iris reset bit */
		reg &= ~WCNSS_PMU_CFG_IRIS_RESET;
		writel_relaxed(reg, pmu_conf_reg);

		/* Start IRIS XO configuration */
		reg |= WCNSS_PMU_CFG_IRIS_XO_CFG;
		writel_relaxed(reg, pmu_conf_reg);

		/* Wait for XO configuration to finish */
		while (readl_relaxed(pmu_conf_reg) &
						WCNSS_PMU_CFG_IRIS_XO_CFG_STS)
			cpu_relax();

		/* Stop IRIS XO configuration */
		reg &= ~(WCNSS_PMU_CFG_GC_BUS_MUX_SEL_TOP |
				WCNSS_PMU_CFG_IRIS_XO_CFG);
		writel_relaxed(reg, pmu_conf_reg);
		clk_disable_unprepare(clk);

		if ((!use_48mhz_xo && auto_detect == WCNSS_XO_INVALID)
				|| auto_detect ==  WCNSS_XO_19MHZ) {

			clk_rf = clk_get(dev, "rf_clk");
			if (IS_ERR(clk_rf)) {
				pr_err("Couldn't get rf_clk\n");
				goto fail;
			}

			rc = clk_prepare_enable(clk_rf);
			if (rc) {
				pr_err("clk_rf enable failed\n");
				goto fail;
			}
			if (iris_xo_set)
				*iris_xo_set = WCNSS_XO_19MHZ;
		}

	}  else if ((!use_48mhz_xo && auto_detect == WCNSS_XO_INVALID)
			|| auto_detect ==  WCNSS_XO_19MHZ) {
		clk_rf = clk_get(dev, "rf_clk");
		if (IS_ERR(clk_rf)) {
			pr_err("Couldn't get rf_clk\n");
			goto fail;
		}
		clk_disable_unprepare(clk_rf);
	}

	/* Add some delay for XO to settle */
	msleep(20);

fail:
	clk_put(clk);

	if (clk_rf != NULL)
		clk_put(clk_rf);

	return rc;
}

/* Helper routine to turn off all WCNSS & IRIS vregs */
static void wcnss_vregs_off(struct vregs_info regulators[], uint size)
{
	int i, rc = 0;

	/* Regulators need to be turned off in the reverse order */
	for (i = (size-1); i >= 0; i--) {
		if (regulators[i].state == VREG_NULL_CONFIG)
			continue;

		/* Remove PWM mode */
		if (regulators[i].state & VREG_OPTIMUM_MODE_MASK) {
			rc = regulator_set_optimum_mode(
					regulators[i].regulator, 0);
			if (rc < 0)
				pr_err("regulator_set_optimum_mode(%s) failed (%d)\n",
						regulators[i].name, rc);
		}

		/* Set voltage to lowest level */
		if (regulators[i].state & VREG_SET_VOLTAGE_MASK) {
			rc = regulator_set_voltage(regulators[i].regulator,
					regulators[i].low_power_min,
					regulators[i].max_voltage);
			if (rc)
				pr_err("regulator_set_voltage(%s) failed (%d)\n",
						regulators[i].name, rc);
		}

		/* Disable regulator */
		if (regulators[i].state & VREG_ENABLE_MASK) {
			rc = regulator_disable(regulators[i].regulator);
			if (rc < 0)
				pr_err("vreg %s disable failed (%d)\n",
						regulators[i].name, rc);
		}

		/* Free the regulator source */
		if (regulators[i].state & VREG_GET_REGULATOR_MASK)
			regulator_put(regulators[i].regulator);

		regulators[i].state = VREG_NULL_CONFIG;
	}
}

/* Common helper routine to turn on all WCNSS & IRIS vregs */
static int wcnss_vregs_on(struct device *dev,
		struct vregs_info regulators[], uint size)
{
	int i, rc = 0, reg_cnt;

	for (i = 0; i < size; i++) {
			/* Get regulator source */
		regulators[i].regulator =
			regulator_get(dev, regulators[i].name);
		if (IS_ERR(regulators[i].regulator)) {
			rc = PTR_ERR(regulators[i].regulator);
				pr_err("regulator get of %s failed (%d)\n",
					regulators[i].name, rc);
				goto fail;
		}
		regulators[i].state |= VREG_GET_REGULATOR_MASK;
		reg_cnt = regulator_count_voltages(regulators[i].regulator);
		/* Set voltage to nominal. Exclude swtiches e.g. LVS */
		if ((regulators[i].nominal_min || regulators[i].max_voltage)
				&& (reg_cnt > 0)) {
			rc = regulator_set_voltage(regulators[i].regulator,
					regulators[i].nominal_min,
					regulators[i].max_voltage);
			if (rc) {
				pr_err("regulator_set_voltage(%s) failed (%d)\n",
						regulators[i].name, rc);
				goto fail;
			}
			regulators[i].state |= VREG_SET_VOLTAGE_MASK;
		}

		/* Vote for PWM/PFM mode if needed */
		if (regulators[i].uA_load && (reg_cnt > 0)) {
			rc = regulator_set_optimum_mode(regulators[i].regulator,
					regulators[i].uA_load);
			if (rc < 0) {
				pr_err("regulator_set_optimum_mode(%s) failed (%d)\n",
						regulators[i].name, rc);
				goto fail;
			}
			regulators[i].state |= VREG_OPTIMUM_MODE_MASK;
		}

		/* Enable the regulator */
		rc = regulator_enable(regulators[i].regulator);
		if (rc) {
			pr_err("vreg %s enable failed (%d)\n",
				regulators[i].name, rc);
			goto fail;
		}
		regulators[i].state |= VREG_ENABLE_MASK;
	}

	return rc;

fail:
	wcnss_vregs_off(regulators, size);
	return rc;

}

static void wcnss_iris_vregs_off(enum wcnss_hw_type hw_type,
					int is_pronto_vt)
{
	switch (hw_type) {
	case WCNSS_RIVA_HW:
		wcnss_vregs_off(iris_vregs_riva, ARRAY_SIZE(iris_vregs_riva));
		break;
	case WCNSS_PRONTO_HW:
		if (is_pronto_vt) {
			wcnss_vregs_off(iris_vregs_pronto_v2,
				ARRAY_SIZE(iris_vregs_pronto_v2));
		} else {
			wcnss_vregs_off(iris_vregs_pronto,
				ARRAY_SIZE(iris_vregs_pronto));
		}
		break;
	default:
		pr_err("%s invalid hardware %d\n", __func__, hw_type);

	}
}

static int wcnss_iris_vregs_on(struct device *dev,
				enum wcnss_hw_type hw_type,
				int is_pronto_vt)
{
	int ret = -1;

	switch (hw_type) {
	case WCNSS_RIVA_HW:
		ret = wcnss_vregs_on(dev, iris_vregs_riva,
				ARRAY_SIZE(iris_vregs_riva));
		break;
	case WCNSS_PRONTO_HW:
		if (is_pronto_vt) {
			ret = wcnss_vregs_on(dev, iris_vregs_pronto_v2,
					ARRAY_SIZE(iris_vregs_pronto_v2));
		} else {
			ret = wcnss_vregs_on(dev, iris_vregs_pronto,
					ARRAY_SIZE(iris_vregs_pronto));
		}
		break;
	default:
		pr_err("%s invalid hardware %d\n", __func__, hw_type);
	}
	return ret;
}

static void wcnss_core_vregs_off(enum wcnss_hw_type hw_type,
					int is_pronto_vt)
{
	switch (hw_type) {
	case WCNSS_RIVA_HW:
		wcnss_vregs_off(riva_vregs, ARRAY_SIZE(riva_vregs));
		break;
	case WCNSS_PRONTO_HW:
		if (is_pronto_vt) {
			wcnss_vregs_off(pronto_vregs_pronto_v2,
				ARRAY_SIZE(pronto_vregs_pronto_v2));
		} else {
			wcnss_vregs_off(pronto_vregs,
				ARRAY_SIZE(pronto_vregs));
		}
		break;
	default:
		pr_err("%s invalid hardware %d\n", __func__, hw_type);
	}

}

static int wcnss_core_vregs_on(struct device *dev,
				enum wcnss_hw_type hw_type,
				int is_pronto_vt)
{
	int ret = -1;

	switch (hw_type) {
	case WCNSS_RIVA_HW:
		ret = wcnss_vregs_on(dev, riva_vregs, ARRAY_SIZE(riva_vregs));
		break;
	case WCNSS_PRONTO_HW:
		if (is_pronto_vt) {
			ret = wcnss_vregs_on(dev, pronto_vregs_pronto_v2,
					ARRAY_SIZE(pronto_vregs_pronto_v2));
		} else {
			ret = wcnss_vregs_on(dev, pronto_vregs,
					ARRAY_SIZE(pronto_vregs));
		}
		break;
	default:
		pr_err("%s invalid hardware %d\n", __func__, hw_type);
	}

	return ret;

}

int wcnss_wlan_power(struct device *dev,
		struct wcnss_wlan_config *cfg,
		enum wcnss_opcode on, int *iris_xo_set)
{
	int rc = 0;
	enum wcnss_hw_type hw_type = wcnss_hardware_type();

	down(&wcnss_power_on_lock);
	if (on) {
		/* RIVA regulator settings */
		rc = wcnss_core_vregs_on(dev, hw_type,
			cfg->is_pronto_vt);
		if (rc)
			goto fail_wcnss_on;

		/* IRIS regulator settings */
		rc = wcnss_iris_vregs_on(dev, hw_type,
			cfg->is_pronto_vt);
		if (rc)
			goto fail_iris_on;

		/* Configure IRIS XO */
		rc = configure_iris_xo(dev, cfg,
				WCNSS_WLAN_SWITCH_ON, iris_xo_set);
		if (rc)
			goto fail_iris_xo;

		is_power_on = true;

	}  else if (is_power_on) {
		is_power_on = false;
		configure_iris_xo(dev, cfg,
				WCNSS_WLAN_SWITCH_OFF, NULL);
		wcnss_iris_vregs_off(hw_type, cfg->is_pronto_vt);
		wcnss_core_vregs_off(hw_type, cfg->is_pronto_vt);
	}

	up(&wcnss_power_on_lock);
	return rc;

fail_iris_xo:
	wcnss_iris_vregs_off(hw_type, cfg->is_pronto_vt);

fail_iris_on:
	wcnss_core_vregs_off(hw_type, cfg->is_pronto_vt);

fail_wcnss_on:
	up(&wcnss_power_on_lock);
	return rc;
}
EXPORT_SYMBOL(wcnss_wlan_power);

/*
 * During SSR WCNSS should not be 'powered on' until all the host drivers
 * finish their shutdown routines.  Host drivers use below APIs to
 * synchronize power-on. WCNSS will not be 'powered on' until all the
 * requests(to lock power-on) are freed.
 */
int wcnss_req_power_on_lock(char *driver_name)
{
	struct host_driver *node;

	if (!driver_name)
		goto err;

	node = kmalloc(sizeof(struct host_driver), GFP_KERNEL);
	if (!node)
		goto err;
	strlcpy(node->name, driver_name, sizeof(node->name));

	mutex_lock(&list_lock);
	/* Lock when the first request is added */
	if (list_empty(&power_on_lock_list))
		down(&wcnss_power_on_lock);
	list_add(&node->list, &power_on_lock_list);
	mutex_unlock(&list_lock);

	return 0;

err:
	return -EINVAL;
}
EXPORT_SYMBOL(wcnss_req_power_on_lock);

int wcnss_free_power_on_lock(char *driver_name)
{
	int ret = -1;
	struct host_driver *node;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &power_on_lock_list, list) {
		if (!strncmp(node->name, driver_name, sizeof(node->name))) {
			list_del(&node->list);
			kfree(node);
			ret = 0;
			break;
		}
	}
	/* unlock when the last host driver frees the lock */
	if (list_empty(&power_on_lock_list))
		up(&wcnss_power_on_lock);
	mutex_unlock(&list_lock);

	return ret;
}
EXPORT_SYMBOL(wcnss_free_power_on_lock);
