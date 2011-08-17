/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <mach/msm_xo.h>
#include <mach/msm_iomap.h>

#include "wcnss_riva.h"

static void __iomem *msm_riva_base;
static struct msm_xo_voter *wlan_clock;
static const char *id = "WLAN";

#define MSM_RIVA_PHYS                     0x03204000
#define RIVA_PMU_CFG                      (msm_riva_base + 0x28)
#define RIVA_PMU_CFG_IRIS_XO_CFG          BIT(3)
#define RIVA_PMU_CFG_IRIS_XO_EN           BIT(4)
#define RIVA_PMU_CFG_GC_BUS_MUX_SEL_TOP   BIT(5)
#define RIVA_PMU_CFG_IRIS_XO_CFG_STS      BIT(6) /* 1: in progress, 0: done */

#define RIVA_PMU_CFG_IRIS_XO_MODE         0x6
#define RIVA_PMU_CFG_IRIS_XO_MODE_48      (3 << 1)

#define VREG_NULL_CONFIG            0x0000
#define VREG_GET_REGULATOR_MASK     0x0001
#define VREG_SET_VOLTAGE_MASK       0x0002
#define VREG_OPTIMUM_MODE_MASK      0x0004
#define VREG_ENABLE_MASK            0x0008

struct vregs_info {
	const char * const name;
	int state;
	const int nominal_min;
	const int low_power_min;
	const int max_voltage;
	const int uA_load;
	struct regulator *regulator;
};

static struct vregs_info iris_vregs[] = {
	{"iris_vddio",  VREG_NULL_CONFIG, 0000000, 0, 0000000, 0,      NULL},
	{"iris_vddxo",  VREG_NULL_CONFIG, 1800000, 0, 1800000, 10000,  NULL},
	{"iris_vddrfa", VREG_NULL_CONFIG, 1300000, 0, 1300000, 100000, NULL},
	{"iris_vddpa",  VREG_NULL_CONFIG, 2900000, 0, 2900000, 515000, NULL},
	{"iris_vdddig", VREG_NULL_CONFIG, 0000000, 0, 0000000, 0,      NULL},
};

static struct vregs_info riva_vregs[] = {
	{"riva_vddmx",  VREG_NULL_CONFIG, 1050000, 0, 1150000, 0,      NULL},
	{"riva_vddcx",  VREG_NULL_CONFIG, 1050000, 0, 1150000, 0,      NULL},
	{"riva_vddpx",  VREG_NULL_CONFIG, 1800000, 0, 1800000, 0,      NULL},
};

static int configure_iris_xo(bool use_48mhz_xo, int on)
{
	u32 reg = 0;
	int rc = 0;

	if (on) {
		msm_riva_base = ioremap(MSM_RIVA_PHYS, SZ_256);
		if (!msm_riva_base) {
			pr_err("ioremap MSM_RIVA_PHYS failed\n");
			goto fail;
		}

		/* Enable IRIS XO */
		writel_relaxed(0, RIVA_PMU_CFG);
		reg = readl_relaxed(RIVA_PMU_CFG);
		reg |= RIVA_PMU_CFG_GC_BUS_MUX_SEL_TOP |
				RIVA_PMU_CFG_IRIS_XO_EN;
		writel_relaxed(reg, RIVA_PMU_CFG);

		/* Clear XO_MODE[b2:b1] bits. Clear implies 19.2 MHz TCXO */
		reg &= ~(RIVA_PMU_CFG_IRIS_XO_MODE);

		if (use_48mhz_xo)
			reg |= RIVA_PMU_CFG_IRIS_XO_MODE_48;

		writel_relaxed(reg, RIVA_PMU_CFG);

		/* Start IRIS XO configuration */
		reg |= RIVA_PMU_CFG_IRIS_XO_CFG;
		writel_relaxed(reg, RIVA_PMU_CFG);

		/* Wait for XO configuration to finish */
		while (readl_relaxed(RIVA_PMU_CFG) &
						RIVA_PMU_CFG_IRIS_XO_CFG_STS)
			cpu_relax();

		/* Stop IRIS XO configuration */
		reg &= ~(RIVA_PMU_CFG_GC_BUS_MUX_SEL_TOP |
				RIVA_PMU_CFG_IRIS_XO_CFG);
		writel_relaxed(reg, RIVA_PMU_CFG);

		if (!use_48mhz_xo) {
			wlan_clock = msm_xo_get(MSM_XO_TCXO_A0, id);
			if (IS_ERR(wlan_clock)) {
				rc = PTR_ERR(wlan_clock);
				pr_err("Failed to get MSM_XO_TCXO_A0 voter"
							" (%d)\n", rc);
				goto fail;
			}

			rc = msm_xo_mode_vote(wlan_clock, MSM_XO_MODE_ON);
			if (rc < 0) {
				pr_err("Configuring MSM_XO_MODE_ON failed"
							" (%d)\n", rc);
				goto msm_xo_vote_fail;
			}
		}
	}  else {
		if (wlan_clock != NULL && !use_48mhz_xo) {
			rc = msm_xo_mode_vote(wlan_clock, MSM_XO_MODE_OFF);
			if (rc < 0)
				pr_err("Configuring MSM_XO_MODE_OFF failed"
							" (%d)\n", rc);
		}
	}

	/* Add some delay for XO to settle */
	msleep(20);

	return rc;

msm_xo_vote_fail:
	msm_xo_put(wlan_clock);

fail:
	return rc;
}

/* Helper routine to turn off all WCNSS vregs e.g. IRIS, Riva */
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

/* Common helper routine to turn on all WCNSS vregs e.g. IRIS, Riva */
static int wcnss_vregs_on(struct device *dev,
		struct vregs_info regulators[], uint size)
{
	int i, rc = 0;

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

		/* Set voltage to nominal. Exclude swtiches e.g. LVS */
		if (regulators[i].nominal_min || regulators[i].max_voltage) {
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
		if (regulators[i].uA_load) {
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

static void wcnss_iris_vregs_off(void)
{
	wcnss_vregs_off(iris_vregs, ARRAY_SIZE(iris_vregs));
}

static int wcnss_iris_vregs_on(struct device *dev)
{
	return wcnss_vregs_on(dev, iris_vregs, ARRAY_SIZE(iris_vregs));
}

static void wcnss_riva_vregs_off(void)
{
	wcnss_vregs_off(riva_vregs, ARRAY_SIZE(riva_vregs));
}

static int wcnss_riva_vregs_on(struct device *dev)
{
	return wcnss_vregs_on(dev, riva_vregs, ARRAY_SIZE(riva_vregs));
}

int wcnss_wlan_power(struct device *dev,
		struct wcnss_wlan_config *cfg,
		enum wcnss_opcode on)
{
	int rc = 0;

	if (on) {
		/* RIVA regulator settings */
		rc = wcnss_riva_vregs_on(dev);
		if (rc)
			goto fail_riva_on;

		/* IRIS regulator settings */
		rc = wcnss_iris_vregs_on(dev);
		if (rc)
			goto fail_iris_on;

		/* Configure IRIS XO */
		rc = configure_iris_xo(cfg->use_48mhz_xo, WCNSS_WLAN_SWITCH_ON);
		if (rc)
			goto fail_iris_xo;

	} else {
		configure_iris_xo(cfg->use_48mhz_xo, WCNSS_WLAN_SWITCH_OFF);
		wcnss_iris_vregs_off();
		wcnss_riva_vregs_off();
	}

	return rc;

fail_iris_xo:
	wcnss_iris_vregs_off();

fail_iris_on:
	wcnss_riva_vregs_off();

fail_riva_on:
	return rc;
}
EXPORT_SYMBOL(wcnss_wlan_power);

