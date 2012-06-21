/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/regulator.h>

#define REG_HWREV		0x002  /* PMIC4 revision */
#define REG_HWREV_2		0x0E8  /* PMIC4 revision 2 */

#define REG_MPP_BASE		0x050
#define REG_RTC_BASE		0x11D
#define REG_IRQ_BASE            0x1BB

#define REG_SPK_BASE		0x253
#define REG_SPK_REGISTERS	3

#define REG_TEMP_ALARM_CTRL	0x01B
#define REG_TEMP_ALARM_PWM	0x09B

#define PM8038_VERSION_MASK	0xFFF0
#define PM8038_VERSION_VALUE	0x09F0
#define PM8038_REVISION_MASK	0x000F

#define REG_PM8038_PON_CNTRL_3	0x01D
#define PM8038_RESTART_REASON_MASK	0x07

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8038 {
	struct device					*dev;
	struct pm_irq_chip				*irq_chip;
	struct mfd_cell					*mfd_regulators;
	struct pm8xxx_regulator_core_platform_data	*regulator_cdata;
	u32						rev_registers;
};

static int pm8038_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8038_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8038_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8038_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8038_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);
}

static enum pm8xxx_version pm8038_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->rev_registers & PM8038_VERSION_MASK) == PM8038_VERSION_VALUE)
		version = PM8XXX_VERSION_8038;

	return version;
}

static int pm8038_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return pmic->rev_registers & PM8038_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8038_drvdata = {
	.pmic_readb		= pm8038_readb,
	.pmic_writeb		= pm8038_writeb,
	.pmic_read_buf		= pm8038_read_buf,
	.pmic_write_buf		= pm8038_write_buf,
	.pmic_read_irq_stat	= pm8038_read_irq_stat,
	.pmic_get_version	= pm8038_get_version,
	.pmic_get_revision	= pm8038_get_revision,
};

static const struct resource gpio_cell_resources[] __devinitconst = {
	[0] = {
		.start = PM8038_IRQ_BLOCK_BIT(PM8038_GPIO_BLOCK_START, 0),
		.end   = PM8038_IRQ_BLOCK_BIT(PM8038_GPIO_BLOCK_START, 0)
			+ PM8038_NR_GPIOS - 1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell gpio_cell __devinitdata = {
	.name		= PM8XXX_GPIO_DEV_NAME,
	.id		= -1,
	.resources	= gpio_cell_resources,
	.num_resources	= ARRAY_SIZE(gpio_cell_resources),
};

static const struct resource adc_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8038_ADC_EOC_USR_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8038_ADC_BATT_TEMP_WARM_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8038_ADC_BATT_TEMP_COLD_IRQ),
};

static struct mfd_cell adc_cell __devinitdata = {
	.name		= PM8XXX_ADC_DEV_NAME,
	.id		= -1,
	.resources	= adc_cell_resources,
	.num_resources	= ARRAY_SIZE(adc_cell_resources),
};

static const struct resource charger_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("USBIN_VALID_IRQ", PM8921_USBIN_VALID_IRQ),
	SINGLE_IRQ_RESOURCE("USBIN_OV_IRQ", PM8921_USBIN_OV_IRQ),
	SINGLE_IRQ_RESOURCE("BATT_INSERTED_IRQ", PM8921_BATT_INSERTED_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET_LOW_IRQ", PM8921_VBATDET_LOW_IRQ),
	SINGLE_IRQ_RESOURCE("USBIN_UV_IRQ", PM8921_USBIN_UV_IRQ),
	SINGLE_IRQ_RESOURCE("VBAT_OV_IRQ", PM8921_VBAT_OV_IRQ),
	SINGLE_IRQ_RESOURCE("CHGWDOG_IRQ", PM8921_CHGWDOG_IRQ),
	SINGLE_IRQ_RESOURCE("VCP_IRQ", PM8921_VCP_IRQ),
	SINGLE_IRQ_RESOURCE("ATCDONE_IRQ", PM8921_ATCDONE_IRQ),
	SINGLE_IRQ_RESOURCE("ATCFAIL_IRQ", PM8921_ATCFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGDONE_IRQ", PM8921_CHGDONE_IRQ),
	SINGLE_IRQ_RESOURCE("CHGFAIL_IRQ", PM8921_CHGFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGSTATE_IRQ", PM8921_CHGSTATE_IRQ),
	SINGLE_IRQ_RESOURCE("LOOP_CHANGE_IRQ", PM8921_LOOP_CHANGE_IRQ),
	SINGLE_IRQ_RESOURCE("FASTCHG_IRQ", PM8921_FASTCHG_IRQ),
	SINGLE_IRQ_RESOURCE("TRKLCHG_IRQ", PM8921_TRKLCHG_IRQ),
	SINGLE_IRQ_RESOURCE("BATT_REMOVED_IRQ", PM8921_BATT_REMOVED_IRQ),
	SINGLE_IRQ_RESOURCE("BATTTEMP_HOT_IRQ", PM8921_BATTTEMP_HOT_IRQ),
	SINGLE_IRQ_RESOURCE("CHGHOT_IRQ", PM8921_CHGHOT_IRQ),
	SINGLE_IRQ_RESOURCE("BATTTEMP_COLD_IRQ", PM8921_BATTTEMP_COLD_IRQ),
	SINGLE_IRQ_RESOURCE("CHG_GONE_IRQ", PM8921_CHG_GONE_IRQ),
	SINGLE_IRQ_RESOURCE("BAT_TEMP_OK_IRQ", PM8921_BAT_TEMP_OK_IRQ),
	SINGLE_IRQ_RESOURCE("COARSE_DET_LOW_IRQ", PM8921_COARSE_DET_LOW_IRQ),
	SINGLE_IRQ_RESOURCE("VDD_LOOP_IRQ", PM8921_VDD_LOOP_IRQ),
	SINGLE_IRQ_RESOURCE("VREG_OV_IRQ", PM8921_VREG_OV_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET_IRQ", PM8921_VBATDET_IRQ),
	SINGLE_IRQ_RESOURCE("BATFET_IRQ", PM8921_BATFET_IRQ),
	SINGLE_IRQ_RESOURCE("PSI_IRQ", PM8921_PSI_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_VALID_IRQ", PM8921_DCIN_VALID_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_OV_IRQ", PM8921_DCIN_OV_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_UV_IRQ", PM8921_DCIN_UV_IRQ),
};

static const struct resource bms_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("PM8921_BMS_SBI_WRITE_OK", PM8921_BMS_SBI_WRITE_OK),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_CC_THR", PM8921_BMS_CC_THR),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_THR", PM8921_BMS_VSENSE_THR),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_FOR_R", PM8921_BMS_VSENSE_FOR_R),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_OCV_FOR_R", PM8921_BMS_OCV_FOR_R),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_GOOD_OCV", PM8921_BMS_GOOD_OCV),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_AVG", PM8921_BMS_VSENSE_AVG),
};

static struct mfd_cell charger_cell __devinitdata = {
	.name		= PM8921_CHARGER_DEV_NAME,
	.id		= -1,
	.resources	= charger_cell_resources,
	.num_resources	= ARRAY_SIZE(charger_cell_resources),
};

static struct mfd_cell bms_cell __devinitdata = {
	.name		= PM8921_BMS_DEV_NAME,
	.id		= -1,
	.resources	= bms_cell_resources,
	.num_resources	= ARRAY_SIZE(bms_cell_resources),
};
static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8038_IRQ_BLOCK_BIT(PM8038_MPP_BLOCK_START, 0),
		.end	= PM8038_IRQ_BLOCK_BIT(PM8038_MPP_BLOCK_START, 0)
			  + PM8038_NR_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 1,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static const struct resource rtc_cell_resources[] __devinitconst = {
	[0] = SINGLE_IRQ_RESOURCE(NULL, PM8038_RTC_ALARM_IRQ),
	[1] = {
		.name   = "pmic_rtc_base",
		.start  = REG_RTC_BASE,
		.end    = REG_RTC_BASE,
		.flags  = IORESOURCE_IO,
	},
};

static struct mfd_cell rtc_cell __devinitdata = {
	.name           = PM8XXX_RTC_DEV_NAME,
	.id             = -1,
	.resources      = rtc_cell_resources,
	.num_resources  = ARRAY_SIZE(rtc_cell_resources),
};

static const struct resource resources_pwrkey[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8038_PWRKEY_REL_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8038_PWRKEY_PRESS_IRQ),
};

static struct mfd_cell pwrkey_cell __devinitdata = {
	.name		= PM8XXX_PWRKEY_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_pwrkey),
	.resources	= resources_pwrkey,
};

static struct mfd_cell pwm_cell __devinitdata = {
	.name           = PM8XXX_PWM_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell misc_cell __devinitdata = {
	.name           = PM8XXX_MISC_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell leds_cell __devinitdata = {
	.name		= PM8XXX_LEDS_DEV_NAME,
	.id		= -1,
};

static const struct resource resources_spk[] __devinitconst = {
	[0] = {
		.name   = PM8XXX_SPK_DEV_NAME,
		.start  = REG_SPK_BASE,
		.end    = REG_SPK_BASE + REG_SPK_REGISTERS,
		.flags  = IORESOURCE_IO,
	},
};

static struct mfd_cell spk_cell __devinitdata = {
	.name           = PM8XXX_SPK_DEV_NAME,
	.id             = -1,
	.num_resources	= ARRAY_SIZE(resources_spk),
	.resources	= resources_spk,
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= 0,
	.platform_data	= "pm8038-dbg",
	.pdata_size	= sizeof("pm8038-dbg"),
};

static const struct resource thermal_alarm_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8038_tempstat_irq", PM8038_TEMPSTAT_IRQ),
	SINGLE_IRQ_RESOURCE("pm8038_overtemp_irq", PM8038_OVERTEMP_IRQ),
};

static struct pm8xxx_tm_core_data thermal_alarm_cdata = {
	.adc_channel			= CHANNEL_DIE_TEMP,
	.adc_type			= PM8XXX_TM_ADC_PM8XXX_ADC,
	.reg_addr_temp_alarm_ctrl	= REG_TEMP_ALARM_CTRL,
	.reg_addr_temp_alarm_pwm	= REG_TEMP_ALARM_PWM,
	.tm_name			= "pm8038_tz",
	.irq_name_temp_stat		= "pm8038_tempstat_irq",
	.irq_name_over_temp		= "pm8038_overtemp_irq",
};

static struct mfd_cell thermal_alarm_cell __devinitdata = {
	.name		= PM8XXX_TM_DEV_NAME,
	.id		= -1,
	.resources	= thermal_alarm_cell_resources,
	.num_resources	= ARRAY_SIZE(thermal_alarm_cell_resources),
	.platform_data	= &thermal_alarm_cdata,
	.pdata_size	= sizeof(struct pm8xxx_tm_core_data),
};

static const struct resource ccadc_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("PM8921_BMS_CCADC_EOC", PM8921_BMS_CCADC_EOC),
};

static struct mfd_cell ccadc_cell __devinitdata = {
	.name		= PM8XXX_CCADC_DEV_NAME,
	.id		= -1,
	.resources	= ccadc_cell_resources,
	.num_resources	= ARRAY_SIZE(ccadc_cell_resources),
};

static struct pm8xxx_vreg regulator_data[] = {
	/*   name	     pc_name	    ctrl   test   hpm_min */
	NLDO1200("8038_l1",		    0x0AE, 0x0AF, LDO_1200),
	NLDO("8038_l2",      "8038_l2_pc",  0x0B0, 0x0B1, LDO_150),
	PLDO("8038_l3",      "8038_l3_pc",  0x0B2, 0x0B3, LDO_50),
	PLDO("8038_l4",      "8038_l4_pc",  0x0B4, 0x0B5, LDO_50),
	PLDO("8038_l5",      "8038_l5_pc",  0x0B6, 0x0B7, LDO_600),
	PLDO("8038_l6",      "8038_l6_pc",  0x0B8, 0x0B9, LDO_600),
	PLDO("8038_l7",      "8038_l7_pc",  0x0BA, 0x0BB, LDO_600),
	PLDO("8038_l8",      "8038_l8_pc",  0x0BC, 0x0BD, LDO_300),
	PLDO("8038_l9",      "8038_l9_pc",  0x0BE, 0x0BF, LDO_300),
	PLDO("8038_l10",     "8038_l10_pc", 0x0C0, 0x0C1, LDO_600),
	PLDO("8038_l11",     "8038_l11_pc", 0x0C2, 0x0C3, LDO_600),
	NLDO("8038_l12",     "8038_l12_pc", 0x0C4, 0x0C5, LDO_300),
	PLDO("8038_l14",     "8038_l14_pc", 0x0C8, 0x0C9, LDO_50),
	PLDO("8038_l15",     "8038_l15_pc", 0x0CA, 0x0CB, LDO_150),
	NLDO1200("8038_l16",		    0x0CC, 0x0CD, LDO_1200),
	PLDO("8038_l17",     "8038_l17_pc", 0x0CE, 0x0CF, LDO_150),
	PLDO("8038_l18",     "8038_l18_pc", 0x0D0, 0x0D1, LDO_50),
	NLDO1200("8038_l19",		    0x0D2, 0x0D3, LDO_1200),
	NLDO1200("8038_l20",		    0x0D4, 0x0D5, LDO_1200),
	PLDO("8038_l21",     "8038_l21_pc", 0x0D6, 0x0D7, LDO_150),
	PLDO("8038_l22",     "8038_l22_pc", 0x0D8, 0x0D9, LDO_50),
	PLDO("8038_l23",     "8038_l23_pc", 0x0DA, 0x0DB, LDO_50),
	NLDO1200("8038_l24",		    0x0DC, 0x0DD, LDO_1200),
	NLDO("8038_l26",     "8038_l26_pc", 0x0E0, 0x0E1, LDO_150),
	NLDO1200("8038_l27",		    0x0E2, 0x0E3, LDO_1200),

	/*   name	pc_name       ctrl   test2  clk    sleep  hpm_min */
	SMPS("8038_s1", "8038_s1_pc", 0x1E0, 0x1E5, 0x009, 0x1E2, SMPS_1500),
	SMPS("8038_s2", "8038_s2_pc", 0x1D8, 0x1DD, 0x00A, 0x1DA, SMPS_1500),
	SMPS("8038_s3", "8038_s3_pc", 0x1D0, 0x1D5, 0x00B, 0x1D2, SMPS_1500),
	SMPS("8038_s4", "8038_s4_pc", 0x1E8, 0x1ED, 0x00C, 0x1EA, SMPS_1500),

	/*     name	  ctrl fts_cnfg1 pfm  pwr_cnfg  hpm_min */
	FTSMPS("8038_s5", 0x025, 0x02E, 0x026, 0x032, SMPS_2000),
	FTSMPS("8038_s6", 0x036, 0x03F, 0x037, 0x043, SMPS_2000),

	/* name		       pc_name	       ctrl   test */
	VS("8038_lvs1",        "8038_lvs1_pc", 0x060, 0x061),
	VS("8038_lvs2",        "8038_lvs2_pc", 0x062, 0x063),
};

#define MAX_NAME_COMPARISON_LEN 32

static int __devinit match_regulator(
	struct pm8xxx_regulator_core_platform_data *core_data, const char *name)
{
	int found = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(regulator_data); i++) {
		if (regulator_data[i].rdesc.name
		    && strncmp(regulator_data[i].rdesc.name, name,
				MAX_NAME_COMPARISON_LEN) == 0) {
			core_data->is_pin_controlled = false;
			core_data->vreg = &regulator_data[i];
			found = 1;
			break;
		} else if (regulator_data[i].rdesc_pc.name
			   && strncmp(regulator_data[i].rdesc_pc.name, name,
				MAX_NAME_COMPARISON_LEN) == 0) {
			core_data->is_pin_controlled = true;
			core_data->vreg = &regulator_data[i];
			found = 1;
			break;
		}
	}

	if (!found)
		pr_err("could not find a match for regulator: %s\n", name);

	return found;
}

static int __devinit
pm8038_add_regulators(const struct pm8038_platform_data *pdata,
		      struct pm8038 *pmic, int irq_base)
{
	int ret = 0;
	struct mfd_cell *mfd_regulators;
	struct pm8xxx_regulator_core_platform_data *cdata;
	int i;

	/* Add one device for each regulator used by the board. */
	mfd_regulators = kzalloc(sizeof(struct mfd_cell)
				 * (pdata->num_regulators), GFP_KERNEL);
	if (!mfd_regulators) {
		pr_err("Cannot allocate %d bytes for pm8038 regulator "
			"mfd cells\n", sizeof(struct mfd_cell)
					* (pdata->num_regulators));
		return -ENOMEM;
	}
	cdata = kzalloc(sizeof(struct pm8xxx_regulator_core_platform_data)
			* pdata->num_regulators, GFP_KERNEL);
	if (!cdata) {
		pr_err("Cannot allocate %d bytes for pm8038 regulator "
			"core data\n", pdata->num_regulators
			  * sizeof(struct pm8xxx_regulator_core_platform_data));
		kfree(mfd_regulators);
		return -ENOMEM;
	}
	for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
		mutex_init(&regulator_data[i].pc_lock);

	for (i = 0; i < pdata->num_regulators; i++) {
		if (!pdata->regulator_pdatas[i].init_data.constraints.name) {
			pr_err("name missing for regulator %d\n", i);
			ret = -EINVAL;
			goto bail;
		}
		if (!match_regulator(&cdata[i],
		      pdata->regulator_pdatas[i].init_data.constraints.name)) {
			ret = -ENODEV;
			goto bail;
		}
		cdata[i].pdata = &(pdata->regulator_pdatas[i]);
		mfd_regulators[i].name = PM8XXX_REGULATOR_DEV_NAME;
		mfd_regulators[i].id = cdata[i].pdata->id;
		mfd_regulators[i].platform_data = &cdata[i];
		mfd_regulators[i].pdata_size =
			sizeof(struct pm8xxx_regulator_core_platform_data);
	}
	ret = mfd_add_devices(pmic->dev, 0, mfd_regulators,
			pdata->num_regulators, NULL, irq_base);
	if (ret)
		goto bail;

	pmic->mfd_regulators = mfd_regulators;
	pmic->regulator_cdata = cdata;
	return ret;

bail:
	for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
		mutex_destroy(&regulator_data[i].pc_lock);
	kfree(mfd_regulators);
	kfree(cdata);
	return ret;
}

static int __devinit
pm8038_add_subdevices(const struct pm8038_platform_data *pdata,
		      struct pm8038 *pmic)
{
	int ret = 0, irq_base = 0;
	struct pm_irq_chip *irq_chip;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8038_NR_IRQS;
		pdata->irq_pdata->irq_cdata.base_addr = REG_IRQ_BASE;
		irq_base = pdata->irq_pdata->irq_base;
		irq_chip = pm8xxx_irq_init(pmic->dev, pdata->irq_pdata);

		if (IS_ERR(irq_chip)) {
			pr_err("Failed to init interrupts ret=%ld\n",
					PTR_ERR(irq_chip));
			return PTR_ERR(irq_chip);
		}
		pmic->irq_chip = irq_chip;
	}

	if (pdata->gpio_pdata) {
		pdata->gpio_pdata->gpio_cdata.ngpios = PM8038_NR_GPIOS;
		gpio_cell.platform_data = pdata->gpio_pdata;
		gpio_cell.pdata_size = sizeof(struct pm8xxx_gpio_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &gpio_cell, 1,
					NULL, irq_base);
		if (ret) {
			pr_err("Failed to add  gpio subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->mpp_pdata) {
		pdata->mpp_pdata->core_data.nmpps = PM8038_NR_MPPS;
		pdata->mpp_pdata->core_data.base_addr = REG_MPP_BASE;
		mpp_cell.platform_data = pdata->mpp_pdata;
		mpp_cell.pdata_size = sizeof(struct pm8xxx_mpp_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &mpp_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add mpp subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->rtc_pdata) {
		rtc_cell.platform_data = pdata->rtc_pdata;
		rtc_cell.pdata_size = sizeof(struct pm8xxx_rtc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &rtc_cell, 1, NULL,
				irq_base);
		if (ret) {
			pr_err("Failed to add rtc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->pwrkey_pdata) {
		pwrkey_cell.platform_data = pdata->pwrkey_pdata;
		pwrkey_cell.pdata_size =
			sizeof(struct pm8xxx_pwrkey_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &pwrkey_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add pwrkey subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	ret = mfd_add_devices(pmic->dev, 0, &pwm_cell, 1, NULL, 0);
	if (ret) {
		pr_err("Failed to add pwm subdevice ret=%d\n", ret);
		goto bail;
	}

	if (pdata->misc_pdata) {
		misc_cell.platform_data = pdata->misc_pdata;
		misc_cell.pdata_size = sizeof(struct pm8xxx_misc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &misc_cell, 1, NULL,
				      irq_base);
		if (ret) {
			pr_err("Failed to add  misc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->leds_pdata) {
		leds_cell.platform_data = pdata->leds_pdata;
		leds_cell.pdata_size = sizeof(struct pm8xxx_led_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &leds_cell, 1, NULL, 0);
		if (ret) {
			pr_err("Failed to add leds subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->spk_pdata) {
		spk_cell.platform_data = pdata->spk_pdata;
		spk_cell.pdata_size = sizeof(struct pm8xxx_spk_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &spk_cell, 1, NULL, 0);
		if (ret) {
			pr_err("Failed to add spk subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->num_regulators > 0 && pdata->regulator_pdatas) {
		ret = pm8038_add_regulators(pdata, pmic, irq_base);
		if (ret) {
			pr_err("Failed to add regulator subdevices ret=%d\n",
				ret);
			goto bail;
		}
	}

	ret = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (ret) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", ret);
		goto bail;
	}

	if (pdata->adc_pdata) {
		adc_cell.platform_data = pdata->adc_pdata;
		adc_cell.pdata_size =
			sizeof(struct pm8xxx_adc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &adc_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add adc subdevices ret=%d\n",
					ret);
		}
	}

	if (pdata->charger_pdata) {
		pdata->charger_pdata->charger_cdata.vbat_channel = CHANNEL_VBAT;
		pdata->charger_pdata->charger_cdata.batt_temp_channel
						= CHANNEL_BATT_THERM;
		pdata->charger_pdata->charger_cdata.batt_id_channel
						= CHANNEL_BATT_ID;
		charger_cell.platform_data = pdata->charger_pdata;
		charger_cell.pdata_size =
				sizeof(struct pm8921_charger_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &charger_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add charger subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->bms_pdata) {
		pdata->bms_pdata->bms_cdata.batt_temp_channel
						= CHANNEL_BATT_THERM;
		pdata->bms_pdata->bms_cdata.vbat_channel = CHANNEL_VBAT;
		pdata->bms_pdata->bms_cdata.ref625mv_channel = CHANNEL_625MV;
		pdata->bms_pdata->bms_cdata.ref1p25v_channel = CHANNEL_125V;
		pdata->bms_pdata->bms_cdata.batt_id_channel = CHANNEL_BATT_ID;
		bms_cell.platform_data = pdata->bms_pdata;
		bms_cell.pdata_size = sizeof(struct pm8921_bms_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &bms_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add bms subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	ret = mfd_add_devices(pmic->dev, 0, &thermal_alarm_cell, 1, NULL,
				irq_base);
	if (ret) {
		pr_err("Failed to add thermal alarm subdevice ret=%d\n", ret);
		goto bail;
	}

	if (pdata->ccadc_pdata) {
		ccadc_cell.platform_data = pdata->ccadc_pdata;
		ccadc_cell.pdata_size =
				sizeof(struct pm8xxx_ccadc_platform_data);

		ret = mfd_add_devices(pmic->dev, 0, &ccadc_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add ccadc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	return 0;
bail:
	if (pmic->irq_chip) {
		pm8xxx_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return ret;
}

static const char * const pm8038_restart_reason[] = {
	[0] = "Unknown",
	[1] = "Triggered from CBL (external charger)",
	[2] = "Triggered from KPD (power key press)",
	[3] = "Triggered from CHG (usb charger insertion)",
	[4] = "Triggered from SMPL (sudden momentary power loss)",
	[5] = "Triggered from RTC (real time clock)",
	[6] = "Triggered by Hard Reset",
	[7] = "Triggered by General Purpose Trigger",
};

static const char * const pm8038_rev_names[] = {
	[PM8XXX_REVISION_8038_TEST]	= "test",
	[PM8XXX_REVISION_8038_1p0]	= "1.0",
	[PM8XXX_REVISION_8038_2p0]	= "2.0",
	[PM8XXX_REVISION_8038_2p1]	= "2.1",
};

static int __devinit pm8038_probe(struct platform_device *pdev)
{
	const struct pm8038_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8038 *pmic;
	enum pm8xxx_version version;
	int revision;
	int rc;
	u8 val;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pmic = kzalloc(sizeof(struct pm8038), GFP_KERNEL);
	if (!pmic) {
		pr_err("Cannot alloc pm8038 struct\n");
		return -ENOMEM;
	}

	/* Read PMIC chip revision */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev reg %d:rc=%d\n", REG_HWREV, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 1: PM8038 rev %02X\n", val);
	pmic->rev_registers = val;

	/* Read PMIC chip revision 2 */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV_2, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 2 reg %d:rc=%d\n",
			REG_HWREV_2, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 2: PM8038 rev %02X\n", val);
	pmic->rev_registers |= val << BITS_PER_BYTE;

	pmic->dev = &pdev->dev;
	pm8038_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8038_drvdata);

	/* Print out human readable version and revision names. */
	version = pm8xxx_get_version(pmic->dev);
	if (version == PM8XXX_VERSION_8038) {
		revision = pm8xxx_get_revision(pmic->dev);
		if (revision >= 0 && revision < ARRAY_SIZE(pm8038_rev_names))
			revision_name = pm8038_rev_names[revision];
		pr_info("PMIC version: PM8038 ver %s\n", revision_name);
	} else {
		WARN_ON(version != PM8XXX_VERSION_8038);
	}

	/* Log human readable restart reason */
	rc = msm_ssbi_read(pdev->dev.parent, REG_PM8038_PON_CNTRL_3, &val, 1);
	if (rc) {
		pr_err("Cannot read restart reason rc=%d\n", rc);
		goto err_read_rev;
	}
	val &= PM8038_RESTART_REASON_MASK;
	pr_info("PMIC Restart Reason: %s\n", pm8038_restart_reason[val]);

	rc = pm8038_add_subdevices(pdata, pmic);
	if (rc) {
		pr_err("Cannot add subdevices rc=%d\n", rc);
		goto err;
	}

	return 0;

err:
	mfd_remove_devices(pmic->dev);
	platform_set_drvdata(pdev, NULL);
	kfree(pmic->mfd_regulators);
	kfree(pmic->regulator_cdata);
err_read_rev:
	kfree(pmic);
	return rc;
}

static int __devexit pm8038_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8038 *pmic = NULL;
	int i;

	drvdata = platform_get_drvdata(pdev);

	if (drvdata)
		pmic = drvdata->pm_chip_data;

	if (pmic) {
		if (pmic->dev)
			mfd_remove_devices(pmic->dev);

		if (pmic->irq_chip) {
			pm8xxx_irq_exit(pmic->irq_chip);
			pmic->irq_chip = NULL;
		}
		if (pmic->mfd_regulators) {
			for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
				mutex_destroy(&regulator_data[i].pc_lock);
		}
		kfree(pmic->mfd_regulators);
		kfree(pmic->regulator_cdata);
		kfree(pmic);
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pm8038_driver = {
	.probe		= pm8038_probe,
	.remove		= __devexit_p(pm8038_remove),
	.driver		= {
		.name	= PM8038_CORE_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8038_init(void)
{
	return platform_driver_register(&pm8038_driver);
}
postcore_initcall(pm8038_init);

static void __exit pm8038_exit(void)
{
	platform_driver_unregister(&pm8038_driver);
}
module_exit(pm8038_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8038 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8038-core");
