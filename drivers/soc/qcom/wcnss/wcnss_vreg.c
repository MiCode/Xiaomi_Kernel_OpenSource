/* Copyright (c) 2011-2015, 2018-2019 The Linux Foundation. All rights reserved.
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

#define RIVA_SPARE_OFFSET       0x0b4
#define PRONTO_SPARE_OFFSET     0x1088
#define NVBIN_DLND_BIT          BIT(25)

#define PRONTO_IRIS_REG_READ_OFFSET       0x1134
#define PRONTO_IRIS_REG_CHIP_ID           0x04
#define PRONTO_IRIS_REG_CHIP_ID_MASK      0xffff
/* IRIS card chip ID's */
#define WCN3660       0x0200
#define WCN3660A      0x0300
#define WCN3660B      0x0400
#define WCN3620       0x5111
#define WCN3620A      0x5112
#define WCN3610       0x9101
#define WCN3610V1     0x9110
#define WCN3615       0x8110

#define WCNSS_PMU_CFG_IRIS_XO_CFG          BIT(3)
#define WCNSS_PMU_CFG_IRIS_XO_EN           BIT(4)
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
#define VDD_PA                      "qcom,iris-vddpa"

#define WCNSS_INVALID_IRIS_REG      0xbaadbaad

struct vregs_info {
	const char * const name;
	const char * const curr;
	const char * const volt;
	int state;
	bool required;
	struct regulator *regulator;
};

/* IRIS regulators for Pronto hardware */
static struct vregs_info iris_vregs[] = {
	{"qcom,iris-vddxo", "qcom,iris-vddxo-current",
	"qcom,iris-vddxo-voltage-level", VREG_NULL_CONFIG, true, NULL},
	{"qcom,iris-vddrfa", "qcom,iris-vddrfa-current",
	"qcom,iris-vddrfa-voltage-level", VREG_NULL_CONFIG, true, NULL},
	{"qcom,iris-vddpa", "qcom,iris-vddpa-current",
	"qcom,iris-vddpa-voltage-level", VREG_NULL_CONFIG, false, NULL},
	{"qcom,iris-vdddig", "qcom,iris-vdddig-current",
	"qcom,iris-vdddig-voltage-level", VREG_NULL_CONFIG, true, NULL},
};

/* WCNSS regulators for Pronto hardware */
static struct vregs_info pronto_vregs[] = {
	{"qcom,pronto-vddmx", "qcom,pronto-vddmx-current",
	"qcom,vddmx-voltage-level", VREG_NULL_CONFIG, true, NULL},
	{"qcom,pronto-vddcx", "qcom,pronto-vddcx-current",
	"qcom,vddcx-voltage-level", VREG_NULL_CONFIG, true, NULL},
	{"qcom,pronto-vddpx", "qcom,pronto-vddpx-current",
	"qcom,vddpx-voltage-level", VREG_NULL_CONFIG, true, NULL},
};

struct host_driver {
	char name[20];
	struct list_head list;
};

enum {
	IRIS_3660, /* also 3660A and 3680 */
	IRIS_3620,
	IRIS_3610,
	IRIS_3615
};

int xo_auto_detect(u32 reg)
{
	reg >>= 30;

	switch (reg) {
	case IRIS_3660:
		return WCNSS_XO_48MHZ;

	case IRIS_3620:
		return WCNSS_XO_19MHZ;

	case IRIS_3610:
		return WCNSS_XO_19MHZ;

	case IRIS_3615:
		return WCNSS_XO_19MHZ;

	default:
		return WCNSS_XO_INVALID;
	}
}

int wcnss_get_iris_name(char *iris_name)
{
	struct wcnss_wlan_config *cfg = NULL;
	u32  iris_id;

	cfg = wcnss_get_wlan_config();

	if (cfg) {
		iris_id = cfg->iris_id;
		iris_id = PRONTO_IRIS_REG_CHIP_ID_MASK & (iris_id >> 16);
	} else {
		return 1;
	}

	switch (iris_id) {
	case WCN3660:
		memcpy(iris_name, "WCN3660", sizeof("WCN3660"));
		break;
	case WCN3660A:
		memcpy(iris_name, "WCN3660A", sizeof("WCN3660A"));
		break;
	case WCN3660B:
		memcpy(iris_name, "WCN3660B", sizeof("WCN3660B"));
		break;
	case WCN3620:
		memcpy(iris_name, "WCN3620", sizeof("WCN3620"));
		break;
	case WCN3620A:
		memcpy(iris_name, "WCN3620A", sizeof("WCN3620A"));
		break;
	case WCN3610:
		memcpy(iris_name, "WCN3610", sizeof("WCN3610"));
		break;
	case WCN3610V1:
		memcpy(iris_name, "WCN3610V1", sizeof("WCN3610V1"));
		break;
	case WCN3615:
		memcpy(iris_name, "WCN3615", sizeof("WCN3615"));
		break;
	default:
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(wcnss_get_iris_name);

int validate_iris_chip_id(u32 reg)
{
	u32 iris_id;

	iris_id = PRONTO_IRIS_REG_CHIP_ID_MASK & (reg >> 16);

	switch (iris_id) {
	case WCN3660:
	case WCN3660A:
	case WCN3660B:
	case WCN3620:
	case WCN3620A:
	case WCN3610:
	case WCN3610V1:
	case WCN3615:
		return 0;
	default:
		return 1;
	}
}

static int
wcnss_dt_parse_vreg_level(struct device *dev, int index,
			  const char *current_vreg_name, const char *vreg_name,
			  struct vregs_level *vlevel)
{
	int ret = 0;
	/* array used to store nominal, low and high voltage values */
	u32 voltage_levels[3], current_vreg;

	ret = of_property_read_u32_array(dev->of_node, vreg_name,
					 voltage_levels,
					 ARRAY_SIZE(voltage_levels));
	if (ret) {
		wcnss_log(ERR, "error reading %s property\n", vreg_name);
		return ret;
	}

	vlevel[index].nominal_min = voltage_levels[0];
	vlevel[index].low_power_min = voltage_levels[1];
	vlevel[index].max_voltage = voltage_levels[2];

	ret = of_property_read_u32(dev->of_node, current_vreg_name,
				   &current_vreg);
	if (ret) {
		wcnss_log(ERR, "error reading %s property\n",
			  current_vreg_name);
		return ret;
	}

	vlevel[index].uA_load = current_vreg;

	return ret;
}

int
wcnss_parse_voltage_regulator(struct wcnss_wlan_config *wlan_config,
			      struct device *dev)
{
	int rc, vreg_i;

	/* Parse pronto voltage regulators from device node */
	for (vreg_i = 0; vreg_i < PRONTO_REGULATORS; vreg_i++) {
		pronto_vregs[vreg_i].regulator =
			devm_regulator_get_optional(dev,
						    pronto_vregs[vreg_i].name);
		if (IS_ERR(pronto_vregs[vreg_i].regulator)) {
			if (pronto_vregs[vreg_i].required) {
				rc = PTR_ERR(pronto_vregs[vreg_i].regulator);
				wcnss_log(ERR,
					"regulator get of %s failed (%d)\n",
					pronto_vregs[vreg_i].name, rc);
				return rc;
			} else {
				wcnss_log(DBG,
				"Skip optional regulator configuration: %s\n",
				pronto_vregs[vreg_i].name);
				continue;
			}
		}

		rc = wcnss_dt_parse_vreg_level(dev, vreg_i,
					       pronto_vregs[vreg_i].curr,
					       pronto_vregs[vreg_i].volt,
					       wlan_config->pronto_vlevel);
		if (rc) {
			wcnss_log(ERR,
				  "error reading voltage-level property\n");
			return rc;
		}
		pronto_vregs[vreg_i].state |= VREG_GET_REGULATOR_MASK;
	}

	/* Parse iris voltage regulators from device node */
	for (vreg_i = 0; vreg_i < IRIS_REGULATORS; vreg_i++) {
		iris_vregs[vreg_i].regulator =
			devm_regulator_get_optional(dev,
						    iris_vregs[vreg_i].name);
		if (IS_ERR(iris_vregs[vreg_i].regulator)) {
			if (iris_vregs[vreg_i].required) {
				rc = PTR_ERR(iris_vregs[vreg_i].regulator);
				wcnss_log(ERR,
					"regulator get of %s failed (%d)\n",
					iris_vregs[vreg_i].name, rc);
				return rc;
			} else {
				wcnss_log(DBG,
				"Skip optional regulator configuration: %s\n",
				iris_vregs[vreg_i].name);
				continue;
			}
		}

		rc = wcnss_dt_parse_vreg_level(dev, vreg_i,
					       iris_vregs[vreg_i].curr,
					       iris_vregs[vreg_i].volt,
					       wlan_config->iris_vlevel);
		if (rc) {
			wcnss_log(ERR,
				  "error reading voltage-level property\n");
			return rc;
		}
		iris_vregs[vreg_i].state |= VREG_GET_REGULATOR_MASK;
	}

	return 0;
}

void  wcnss_iris_reset(u32 reg, void __iomem *pmu_conf_reg)
{
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
}

static int
configure_iris_xo(struct device *dev,
		  struct wcnss_wlan_config *cfg,
		  int on, int *iris_xo_set)
{
	u32 reg = 0, i = 0;
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
			wcnss_log(ERR, "Couldn't get xo clock\n");
			return PTR_ERR(clk);
		}

	} else {
		pmu_offset = RIVA_PMU_OFFSET;
		spare_offset = RIVA_SPARE_OFFSET;

		clk = clk_get(dev, "cxo");
		if (IS_ERR(clk)) {
			wcnss_log(ERR, "Couldn't get cxo clock\n");
			return PTR_ERR(clk);
		}
	}

	if (on) {
		msm_wcnss_base = cfg->msm_wcnss_base;
		if (!msm_wcnss_base) {
			wcnss_log(ERR, "ioremap wcnss physical failed\n");
			goto fail;
		}

		/* Enable IRIS XO */
		rc = clk_prepare_enable(clk);
		if (rc) {
			wcnss_log(ERR, "clk enable failed\n");
			goto fail;
		}

		/* NV bit is set to indicate that platform driver is capable
		 * of doing NV download.
		 */
		wcnss_log(DBG, "Indicate NV bin download\n");
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

		wcnss_iris_reset(reg, pmu_conf_reg);

		if (iris_reg != WCNSS_INVALID_IRIS_REG) {
			iris_reg &= 0xffff;
			iris_reg |= PRONTO_IRIS_REG_CHIP_ID;
			writel_relaxed(iris_reg, iris_read_reg);
			do {
				/* Iris read */
				reg = readl_relaxed(pmu_conf_reg);
				reg |= WCNSS_PMU_CFG_IRIS_XO_READ;
				writel_relaxed(reg, pmu_conf_reg);

				/* Wait for PMU_CFG.iris_reg_read_sts */
				while (readl_relaxed(pmu_conf_reg) &
						WCNSS_PMU_CFG_IRIS_XO_READ_STS)
					cpu_relax();

				iris_reg = readl_relaxed(iris_read_reg);
				wcnss_log(INFO, "IRIS Reg: %08x\n", iris_reg);

				if (validate_iris_chip_id(iris_reg) && i >= 4) {
					wcnss_log(INFO,
						"IRIS Card absent/invalid\n");
					auto_detect = WCNSS_XO_INVALID;
					/* Reset iris read bit */
					reg &= ~WCNSS_PMU_CFG_IRIS_XO_READ;
					/* Clear XO_MODE[b2:b1] bits.
					 * Clear implies 19.2 MHz TCXO
					 */
					reg &= ~(WCNSS_PMU_CFG_IRIS_XO_MODE);
					goto xo_configure;
				} else if (!validate_iris_chip_id(iris_reg)) {
					wcnss_log(DBG,
						  "IRIS Card is present\n");
					break;
				}
				reg &= ~WCNSS_PMU_CFG_IRIS_XO_READ;
				writel_relaxed(reg, pmu_conf_reg);
				wcnss_iris_reset(reg, pmu_conf_reg);
			} while (i++ < 5);
			auto_detect = xo_auto_detect(iris_reg);

			/* Reset iris read bit */
			reg &= ~WCNSS_PMU_CFG_IRIS_XO_READ;

		} else if (wcnss_xo_auto_detect_enabled()) {
			/* Default to 48 MHZ */
			auto_detect = WCNSS_XO_48MHZ;
		} else {
			auto_detect = WCNSS_XO_INVALID;
		}

		cfg->iris_id = iris_reg;

		/* Clear XO_MODE[b2:b1] bits. Clear implies 19.2 MHz TCXO */
		reg &= ~(WCNSS_PMU_CFG_IRIS_XO_MODE);

		if ((use_48mhz_xo && auto_detect == WCNSS_XO_INVALID) ||
		    auto_detect ==  WCNSS_XO_48MHZ) {
			reg |= WCNSS_PMU_CFG_IRIS_XO_MODE_48;

			if (iris_xo_set)
				*iris_xo_set = WCNSS_XO_48MHZ;
		}

xo_configure:
		writel_relaxed(reg, pmu_conf_reg);

		wcnss_iris_reset(reg, pmu_conf_reg);

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

		if ((!use_48mhz_xo && auto_detect == WCNSS_XO_INVALID) ||
		    auto_detect ==  WCNSS_XO_19MHZ) {
			clk_rf = clk_get(dev, "rf_clk");
			if (IS_ERR(clk_rf)) {
				wcnss_log(ERR, "Couldn't get rf_clk\n");
				goto fail;
			}

			rc = clk_prepare_enable(clk_rf);
			if (rc) {
				wcnss_log(ERR, "clk_rf enable failed\n");
				goto fail;
			}
			if (iris_xo_set)
				*iris_xo_set = WCNSS_XO_19MHZ;
		}

	}  else if ((!use_48mhz_xo && auto_detect == WCNSS_XO_INVALID) ||
		    auto_detect ==  WCNSS_XO_19MHZ) {
		clk_rf = clk_get(dev, "rf_clk");
		if (IS_ERR(clk_rf)) {
			wcnss_log(ERR, "Couldn't get rf_clk\n");
			goto fail;
		}
		clk_disable_unprepare(clk_rf);
	}

	/* Add some delay for XO to settle */
	msleep(20);

fail:
	clk_put(clk);

	if (clk_rf)
		clk_put(clk_rf);

	return rc;
}

/* Helper routine to turn off all WCNSS & IRIS vregs */
static void wcnss_vregs_off(struct vregs_info regulators[], uint size,
			    struct vregs_level *voltage_level)
{
	int i, rc = 0;
	struct wcnss_wlan_config *cfg;

	cfg = wcnss_get_wlan_config();

	if (!cfg) {
		wcnss_log(ERR, "Failed to get WLAN configuration\n");
		return;
	}

	/* Regulators need to be turned off in the reverse order */
	for (i = (size - 1); i >= 0; i--) {
		if (regulators[i].state == VREG_NULL_CONFIG)
			continue;

		/* Remove PWM mode */
		if (regulators[i].state & VREG_OPTIMUM_MODE_MASK) {
			rc = regulator_set_load(regulators[i].regulator, 0);
			if (rc < 0) {
				wcnss_log(ERR,
					"regulator set load(%s) failed (%d)\n",
				       regulators[i].name, rc);
			}
		}

		/* Set voltage to lowest level */
		if (regulators[i].state & VREG_SET_VOLTAGE_MASK) {
			if (cfg->is_pronto_vadc) {
				if (cfg->vbatt < WCNSS_VBATT_THRESHOLD &&
				    !memcmp(regulators[i].name,
					    VDD_PA, sizeof(VDD_PA))) {
					voltage_level[i].max_voltage =
						WCNSS_VBATT_LOW;
				}
			}

			rc = regulator_set_voltage(regulators[i].regulator,
						   voltage_level[i].
						   low_power_min,
						   voltage_level[i].
						   max_voltage);

			if (rc)
				wcnss_log(ERR,
				"regulator_set_voltage(%s) failed (%d)\n",
				regulators[i].name, rc);
		}

		/* Disable regulator */
		if (regulators[i].state & VREG_ENABLE_MASK) {
			rc = regulator_disable(regulators[i].regulator);
			if (rc < 0)
				wcnss_log(ERR, "vreg %s disable failed (%d)\n",
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
			  struct vregs_info regulators[], uint size,
			  struct vregs_level *voltage_level)
{
	int i, rc = 0, reg_cnt;
	struct wcnss_wlan_config *cfg;

	cfg = wcnss_get_wlan_config();

	if (!cfg) {
		wcnss_log(ERR, "Failed to get WLAN configuration\n");
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		if (regulators[i].state == VREG_NULL_CONFIG)
			continue;

		reg_cnt = regulator_count_voltages(regulators[i].regulator);
		/* Set voltage to nominal. Exclude swtiches e.g. LVS */
		if ((voltage_level[i].nominal_min ||
		     voltage_level[i].max_voltage) && (reg_cnt > 0)) {
			if (cfg->is_pronto_vadc) {
				if (cfg->vbatt < WCNSS_VBATT_THRESHOLD &&
				    !memcmp(regulators[i].name,
				    VDD_PA, sizeof(VDD_PA))) {
					voltage_level[i].nominal_min =
						WCNSS_VBATT_INITIAL;
					voltage_level[i].max_voltage =
						WCNSS_VBATT_LOW;
				}
			}

			rc = regulator_set_voltage(regulators[i].regulator,
						   voltage_level[i].nominal_min,
						   voltage_level[i].
						   max_voltage);

			if (rc) {
				wcnss_log(ERR,
				"regulator_set_voltage(%s) failed (%d)\n",
				       regulators[i].name, rc);
				goto fail;
			}
			regulators[i].state |= VREG_SET_VOLTAGE_MASK;
		}

		/* Vote for PWM/PFM mode if needed */
		if (voltage_level[i].uA_load && (reg_cnt > 0)) {
			rc = regulator_set_load(regulators[i].regulator,
						voltage_level[i].uA_load);
			if (rc < 0) {
				wcnss_log(ERR,
				"regulator set load(%s) failed (%d)\n",
				       regulators[i].name, rc);
				goto fail;
			}
			regulators[i].state |= VREG_OPTIMUM_MODE_MASK;
		}

		/* Enable the regulator */
		rc = regulator_enable(regulators[i].regulator);
		if (rc) {
			wcnss_log(ERR, "vreg %s enable failed (%d)\n",
			       regulators[i].name, rc);
			goto fail;
		}
		regulators[i].state |= VREG_ENABLE_MASK;
	}

	return rc;

fail:
	wcnss_vregs_off(regulators, size, voltage_level);
	return rc;
}

static void wcnss_iris_vregs_off(enum wcnss_hw_type hw_type,
				 struct wcnss_wlan_config *cfg)
{
	switch (hw_type) {
	case WCNSS_PRONTO_HW:
		wcnss_vregs_off(iris_vregs, ARRAY_SIZE(iris_vregs),
				cfg->iris_vlevel);
		break;
	default:
		wcnss_log(ERR, "%s invalid hardware %d\n", __func__, hw_type);
	}
}

static int wcnss_iris_vregs_on(struct device *dev,
			       enum wcnss_hw_type hw_type,
			       struct wcnss_wlan_config *cfg)
{
	int ret = -1;

	switch (hw_type) {
	case WCNSS_PRONTO_HW:
		ret = wcnss_vregs_on(dev, iris_vregs, ARRAY_SIZE(iris_vregs),
				     cfg->iris_vlevel);
		break;
	default:
		wcnss_log(ERR, "%s invalid hardware %d\n", __func__, hw_type);
	}
	return ret;
}

static void wcnss_core_vregs_off(enum wcnss_hw_type hw_type,
				 struct wcnss_wlan_config *cfg)
{
	switch (hw_type) {
	case WCNSS_PRONTO_HW:
		wcnss_vregs_off(pronto_vregs,
				ARRAY_SIZE(pronto_vregs), cfg->pronto_vlevel);
		break;
	default:
		wcnss_log(ERR, "%s invalid hardware %d\n", __func__, hw_type);
	}
}

static int wcnss_core_vregs_on(struct device *dev,
			       enum wcnss_hw_type hw_type,
			       struct wcnss_wlan_config *cfg)
{
	int ret = -1;

	switch (hw_type) {
	case WCNSS_PRONTO_HW:
		ret = wcnss_vregs_on(dev, pronto_vregs,
				     ARRAY_SIZE(pronto_vregs),
				     cfg->pronto_vlevel);
		break;
	default:
		wcnss_log(ERR, "%s invalid hardware %d\n", __func__, hw_type);
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
					 cfg);
		if (rc)
			goto fail_wcnss_on;

		/* IRIS regulator settings */
		rc = wcnss_iris_vregs_on(dev, hw_type,
					 cfg);
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
		wcnss_iris_vregs_off(hw_type, cfg);
		wcnss_core_vregs_off(hw_type, cfg);
	}

	up(&wcnss_power_on_lock);
	return rc;

fail_iris_xo:
	wcnss_iris_vregs_off(hw_type, cfg);

fail_iris_on:
	wcnss_core_vregs_off(hw_type, cfg);

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

	node = kmalloc(sizeof(*node), GFP_KERNEL);
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
		if (!strcmp(node->name, driver_name)) {
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
