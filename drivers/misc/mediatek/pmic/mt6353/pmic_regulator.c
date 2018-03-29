/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include <mach/mt_pmic_wrap.h>
#include "include/pmic_regulator.h"

/*****************************************************************************
 * Global variable
 ******************************************************************************/
unsigned int g_pmic_pad_vbif28_vol = 1;

unsigned int pmic_read_vbif28_volt(unsigned int *val)
{
	if (g_pmic_pad_vbif28_vol != 0x1) {
		*val = g_pmic_pad_vbif28_vol;
		return 1;
	} else
		return 0;
}

unsigned int pmic_get_vbif28_volt(void)
{
	return g_pmic_pad_vbif28_vol;
}

int en_buck_vsleep_dbg = 0;
unsigned int pmic_buck_vsleep_to_swctrl(unsigned int buck_num, unsigned int vsleep_addr,
	unsigned int vsleep_mask, unsigned int vsleep_shift)
{
#ifdef MT6351
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	return_value = pwrap_wacs2(0, (vsleep_addr), 0, &rdata);
	pmic_reg = rdata;
	mtk_bucks[buck_num].vsleep_en_saved = rdata;
	if (en_buck_vsleep_dbg == 1) {
		pr_err("[pmic]vsleep %s_vsleep[0x%x]=0x%x, 0x%x\n", mtk_bucks[buck_num].desc.name,
		vsleep_addr, mtk_bucks[buck_num].vsleep_en_saved, rdata);
	}
	pmic_reg &= ~(vsleep_mask << vsleep_shift);
	pmic_reg |= (0 << vsleep_shift);

	return_value = pwrap_wacs2(1, (vsleep_addr), pmic_reg, &rdata);
	if (en_buck_vsleep_dbg == 1) {
		udelay(1000);
		return_value = pwrap_wacs2(0, (vsleep_addr), 0, &rdata);
		pr_err("[pmic]vsleep.b %s_vsleep[0x%x]=0x%x, 0x%x\n", mtk_bucks[buck_num].desc.name,
			vsleep_addr, mtk_bucks[buck_num].vsleep_en_saved, pmic_reg);
	}
#endif /*--MT6351--*/
	return 0;
}
unsigned int pmic_buck_vsleep_restore(unsigned int buck_num, unsigned int vsleep_addr,
	unsigned int vsleep_mask, unsigned int vsleep_shift)
{
#ifdef MT6351
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata = 0;

	if (mtk_bucks[buck_num].vsleep_en_saved != 0x0fffffff) {
		#ifdef MT6351
		if (vsleep_addr == PMIC_BUCK_VGPU_VSLEEP_EN_ADDR)
			mtk_bucks[buck_num].vsleep_en_saved |= 0x13;
		#endif
		pmic_reg &= ~(vsleep_mask << vsleep_shift);
		pmic_reg |= (mtk_bucks[buck_num].vsleep_en_saved);
		return_value = pwrap_wacs2(1, (vsleep_addr), pmic_reg, &rdata);
		if (en_buck_vsleep_dbg == 1) {
			pr_err("[pmic]restore %s_vsleep[0x%x]=0x%x, 0x%x\n", mtk_bucks[buck_num].desc.name,
			vsleep_addr, mtk_bucks[buck_num].vsleep_en_saved, rdata);
		}
		mtk_bucks[buck_num].vsleep_en_saved = 0x0fffffff;
	} else {
		if (en_buck_vsleep_dbg == 1) {
			pr_err("[pmic]bypass %s_vsleep[0x%x]=0x%x, 0x%x\n", mtk_bucks[buck_num].desc.name,
			vsleep_addr, mtk_bucks[buck_num].vsleep_en_saved, rdata);
		}
	}
#endif /*--MT6351--*/
	return 0;
}
unsigned int pmic_config_interface_buck_vsleep_check(unsigned int RegNum,
	unsigned int val, unsigned int MASK, unsigned int SHIFT)
{

#ifdef MT6351
	switch (RegNum) {
	case PMIC_BUCK_VPROC_EN_ADDR:
		if ((val & 0x1) == 0x0) {
			pmic_buck_vsleep_to_swctrl(8,
				PMIC_BUCK_VPROC_VSLEEP_EN_ADDR,
				PMIC_BUCK_VPROC_VSLEEP_EN_MASK,
				PMIC_BUCK_VPROC_VSLEEP_EN_SHIFT);

		} else if ((val & 0x1) == 0x1) {
			pmic_buck_vsleep_restore(8,
				PMIC_BUCK_VPROC_VSLEEP_EN_ADDR,
				PMIC_BUCK_VPROC_VSLEEP_EN_MASK,
				PMIC_BUCK_VPROC_VSLEEP_EN_SHIFT);
		}
		break;
	case PMIC_BUCK_VCORE_EN_ADDR:
		if ((val & 0x1) == 0x0) { /* buck off */
			pmic_buck_vsleep_to_swctrl(0,
				PMIC_BUCK_VCORE_VSLEEP_EN_ADDR,
				PMIC_BUCK_VCORE_VSLEEP_EN_MASK,
				PMIC_BUCK_VCORE_VSLEEP_EN_SHIFT);
		} else if ((val & 0x1) == 0x1) { /* buck on */
			pmic_buck_vsleep_restore(0,
				PMIC_BUCK_VCORE_VSLEEP_EN_ADDR,
				PMIC_BUCK_VCORE_VSLEEP_EN_MASK,
				PMIC_BUCK_VCORE_VSLEEP_EN_SHIFT);
		}
		break;
	case PMIC_BUCK_VCORE2_EN_ADDR:
		if ((val & 0x1) == 0x0) { /* buck off */
			pmic_buck_vsleep_to_swctrl(0,
				PMIC_BUCK_VCORE2_VSLEEP_EN_ADDR,
				PMIC_BUCK_VCORE2_VSLEEP_EN_MASK,
				PMIC_BUCK_VCORE2_VSLEEP_EN_SHIFT);
		} else if ((val & 0x1) == 0x1) { /* buck on */
			pmic_buck_vsleep_restore(0,
				PMIC_BUCK_VCORE2_VSLEEP_EN_ADDR,
				PMIC_BUCK_VCORE2_VSLEEP_EN_MASK,
				PMIC_BUCK_VCORE2_VSLEEP_EN_SHIFT);
		}
		break;
	case PMIC_BUCK_VS1_EN_ADDR:
		if ((val & 0x1) == 0x0) {
			pmic_buck_vsleep_to_swctrl(5,
				PMIC_BUCK_VS1_VSLEEP_EN_ADDR,
				PMIC_BUCK_VS1_VSLEEP_EN_MASK,
				PMIC_BUCK_VS1_VSLEEP_EN_SHIFT);

		} else if ((val & 0x1) == 0x1) {
			pmic_buck_vsleep_restore(5,
				PMIC_BUCK_VS1_VSLEEP_EN_ADDR,
				PMIC_BUCK_VS1_VSLEEP_EN_MASK,
				PMIC_BUCK_VS1_VSLEEP_EN_SHIFT);
		}
		break;
	case PMIC_BUCK_VPA_EN_ADDR:
    #ifdef MT6351
		if ((val & 0x1) == 0x0) {
			pmic_buck_vsleep_to_swctrl(7,
				PMIC_BUCK_VPA_VSLEEP_EN_ADDR,
				PMIC_BUCK_VPA_VSLEEP_EN_MASK,
				PMIC_BUCK_VPA_VSLEEP_EN_SHIFT);

		} else if ((val & 0x1) == 0x1) {
			pmic_buck_vsleep_restore(7,
				PMIC_BUCK_VPA_VSLEEP_EN_ADDR,
				PMIC_BUCK_VPA_VSLEEP_EN_MASK,
				PMIC_BUCK_VPA_VSLEEP_EN_SHIFT);
		}
    #endif
		break;
	default:
		/* bypass buck vsleep en check */
		break;
	}
#endif /*--MT6351--*/
	return 0;
}

/*****************************************************************************
 * workaround for vio18 drop issue in E1
 ******************************************************************************/

static const unsigned char mt6353_VIO[] = {
	12, 13, 14, 15, 0, 1, 2, 3, 8, 8, 8, 8, 8, 9, 10, 11
};

static unsigned char vio18_cal;

void upmu_set_rg_vio18_cal(unsigned int en)
{
#if defined MT6328
	unsigned int chip_version = 0;

	chip_version = pmic_get_register_value(PMIC_SWCID);

	if (chip_version == PMIC6353_E1_CID_CODE) {
		if (en == 1)
			pmic_set_register_value(PMIC_RG_VIO18_CAL, mt6353_VIO[vio18_cal]);
		else
			pmic_set_register_value(PMIC_RG_VIO18_CAL, vio18_cal);
	}

#endif
}
EXPORT_SYMBOL(upmu_set_rg_vio18_cal);


static const unsigned char mt6353_VIO_1_84[] = {
	14, 15, 0, 1, 2, 3, 4, 5, 8, 8, 8, 9, 10, 11, 12, 13
};



void upmu_set_rg_vio18_184(void)
{
	PMICLOG("[upmu_set_rg_vio18_184] old cal=%d new cal=%d.\r\n", vio18_cal,
		mt6353_VIO_1_84[vio18_cal]);
	pmic_set_register_value(PMIC_RG_VIO18_CAL, mt6353_VIO_1_84[vio18_cal]);

}


static const unsigned char mt6353_VMC_1_86[] = {
	14, 15, 0, 1, 2, 3, 4, 5, 8, 8, 8, 9, 10, 11, 12, 13
};


static unsigned char vmc_cal;

void upmu_set_rg_vmc_184(unsigned char x)
{

	PMICLOG("[upmu_set_rg_vio18_184] old cal=%d new cal=%d.\r\n", vmc_cal,
		mt6353_VMC_1_86[vmc_cal]);
	if (x == 0) {
		pmic_set_register_value(PMIC_RG_VMC_CAL, vmc_cal);
		PMICLOG("[upmu_set_rg_vio18_184]:0 old cal=%d new cal=%d.\r\n", vmc_cal, vmc_cal);
	} else {
		pmic_set_register_value(PMIC_RG_VMC_CAL, mt6353_VMC_1_86[vmc_cal]);
		PMICLOG("[upmu_set_rg_vio18_184]:1 old cal=%d new cal=%d.\r\n", vmc_cal,
			mt6353_VMC_1_86[vmc_cal]);
	}
}


static unsigned char vcamd_cal;

static const unsigned char mt6353_vcamd[] = {
	1, 2, 3, 4, 5, 6, 7, 7, 9, 10, 11, 12, 13, 14, 15, 0
};


void upmu_set_rg_vcamd(unsigned char x)
{

	PMICLOG("[upmu_set_rg_vcamd] old cal=%d new cal=%d.\r\n", vcamd_cal,
		mt6353_vcamd[vcamd_cal]);
	if (x == 0) {
		pmic_set_register_value(PMIC_RG_VCAMD_CAL, vcamd_cal);
		PMICLOG("[upmu_set_rg_vcamd]:0 old cal=%d new cal=%d.\r\n", vcamd_cal, vcamd_cal);
	} else {
		pmic_set_register_value(PMIC_RG_VCAMD_CAL, mt6353_vcamd[vcamd_cal]);
		PMICLOG("[upmu_set_rg_vcamd]:1 old cal=%d new cal=%d.\r\n", vcamd_cal,
			mt6353_vcamd[vcamd_cal]);
	}
}


/*****************************************************************************
 * PMIC6353 linux reguplator driver
 ******************************************************************************/

int mtk_regulator_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int add = 0, val = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	if (mreg->en_reg != 0) {
		pmic_set_register_value(mreg->en_reg, 1);
		add = pmu_flags_table[mreg->en_reg].offset;
		val = upmu_get_reg_value(add);
	}

	PMICLOG("regulator_enable(name=%s id=%d en_reg=%x vol_reg=%x) [%x]=0x%x\n", rdesc->name,
		rdesc->id, mreg->en_reg, mreg->vol_reg, add, val);

	return 0;
}

int mtk_regulator_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int add = 0, val = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	if (rdev->use_count == 0) {
		PMICLOG("regulator name=%s should not disable( use_count=%d)\n", rdesc->name,
			rdev->use_count);
		return -1;
	}

	if (mreg->en_reg != 0) {
		pmic_set_register_value(mreg->en_reg, 0);
		add = pmu_flags_table[mreg->en_reg].offset;
		val = upmu_get_reg_value(add);
	}

	PMICLOG("regulator_disable(name=%s id=%d en_reg=%x vol_reg=%x use_count=%d) [%x]=0x%x\n",
		rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg, rdev->use_count, add, val);

	return 0;
}

int mtk_regulator_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int en;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	en = pmic_get_register_value(mreg->en_reg);

	PMICLOG("[PMIC]regulator_is_enabled(name=%s id=%d en_reg=%x vol_reg=%x en=%d)\n",
		rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg, en);

	return en;
}

int mtk_regulator_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	const int *pVoltage;
	int voltage = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

		if (mreg->desc.n_voltages != 1) {
			if (mreg->vol_reg != 0) {
				if (mreg->pvoltages != NULL) {
					pVoltage = (const int *)mreg->pvoltages;
					voltage = pVoltage[selector];
				} else {
					voltage = mreg->desc.min_uV + mreg->desc.uV_step * selector;
				}
			} else {
				PMICLOG
				("mtk_regulator_list_voltage bugl(name=%s id=%d en_reg=%x vol_reg=%x)\n",
				rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg);
			}
		} else {
			pVoltage = (const int *)mreg->pvoltages;
			voltage = pVoltage[0];
		}

	return voltage;
}

int mtk_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned char regVal = 0;
	const int *pVoltage;
	int voltage = 0;
	unsigned int add = 0, val = 9;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	/*mreg = container_of(rdev, struct mtk_regulator, rdev);*/

	if (mreg->desc.n_voltages != 1) {
		if (mreg->vol_reg != 0) {
			regVal = pmic_get_register_value(mreg->vol_reg);
			if (mreg->pvoltages != NULL) {
				pVoltage = (const int *)mreg->pvoltages;
				voltage = pVoltage[regVal];
				add = pmu_flags_table[mreg->en_reg].offset;
				val = upmu_get_reg_value(add);
			} else {
				voltage = mreg->desc.min_uV + mreg->desc.uV_step * regVal;
			}
		} else {
			PMICLOG
			    ("regulator_get_voltage_sel bugl(name=%s id=%d en_reg=%x vol_reg=%x)\n",
			     rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg);
		}
	} else {
		if (mreg->vol_reg != 0) {
			regVal = 0;
			pVoltage = (const int *)mreg->pvoltages;
			voltage = pVoltage[regVal];
		} else {
			PMICLOG
			    ("regulator_get_voltage_sel bugl(name=%s id=%d en_reg=%x vol_reg=%x)\n",
			     rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg);
		}
	}
	PMICLOG
	    ("regulator_get_voltage_sel(name=%s id=%d en_reg=%x vol_reg=%x reg/sel:%d voltage:%d [0x%x]=0x%x)\n",
	     rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg, regVal, voltage, add, val);

	return regVal;
}

int mtk_regulator_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("regulator_set_voltage_sel(name=%s id=%d en_reg=%x vol_reg=%x selector=%d)\n",
		rdesc->name, rdesc->id, mreg->en_reg, mreg->vol_reg, selector);

	/* record status that ldo regulator have been modified */
	mreg->vosel.cur_sel = selector;

	/*pr_err("regulator_set_voltage_sel(name=%s selector=%d)\n",
		rdesc->name, selector);*/
	if (mreg->vol_reg != 0)
		pmic_set_register_value(mreg->vol_reg, selector);

	return 0;
}

#if 0
static struct regulator_ops mtk_regulator_ops = {
	.enable = mtk_regulator_enable,
	.disable = mtk_regulator_disable,
	.is_enabled = mtk_regulator_is_enabled,
	.get_voltage_sel = mtk_regulator_get_voltage_sel,
	.set_voltage_sel = mtk_regulator_set_voltage_sel,
	.list_voltage = mtk_regulator_list_voltage,
};
#endif

static ssize_t show_BUCK_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_regulator *mreg;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, en_att);

	ret_value = pmic_get_register_value(mreg->qi_en_reg);

	pr_debug("[EM] BUCK_%s_STATUS : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_STATUS(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

static ssize_t show_BUCK_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_regulator *mreg;
	const int *pVoltage;

	unsigned short regVal;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, voltage_att);

	if (mreg->desc.n_voltages != 1) {
		if (mreg->qi_vol_reg != 0) {
			regVal = pmic_get_register_value(mreg->qi_vol_reg);
			if (mreg->pvoltages != NULL) {
				pVoltage = (const int *)mreg->pvoltages;
				ret_value = pVoltage[regVal];
			} else {
				ret_value = mreg->desc.min_uV + mreg->desc.uV_step * regVal;
			}
		} else {
			pr_debug("[EM][ERROR] buck_%s_VOLTAGE : voltage=0 vol_reg=0\n",
				mreg->desc.name);
		}
	} else {
		pVoltage = (const int *)mreg->pvoltages;
		ret_value = pVoltage[0];
	}

	ret_value = ret_value / 1000;
	pr_debug("[EM] BUCK_%s_VOLTAGE : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VOLTAGE(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

/*****************************************************************************
 * PMIC extern variable
 ******************************************************************************/
static struct mtk_regulator mtk_bucks[] = {
	PMIC_BUCK_GEN(VPROC_BUCK, PMIC_BUCK_VPROC_EN,
		      PMIC_BUCK_VPROC_VOSEL, 600000, 1393750, 6250),
	PMIC_BUCK_GEN(VCORE, PMIC_BUCK_VCORE_EN, PMIC_BUCK_VCORE_VOSEL, 600000,
		      1393750, 6250),
	PMIC_BUCK_GEN(VCORE2, PMIC_BUCK_VCORE2_EN, PMIC_BUCK_VCORE2_VOSEL, 600000,
		      1393750, 6250),
	PMIC_BUCK_GEN(VS1, PMIC_BUCK_VS1_EN, PMIC_BUCK_VS1_VOSEL, 600000, 1393750,
		      6250),
	PMIC_BUCK_GEN(VPA, PMIC_BUCK_VPA_EN, PMIC_BUCK_VPA_VOSEL, 600000, 1393750,
		      6250),
};

static int mtk_bucks_size = ARRAY_SIZE(mtk_bucks);


/*#if !defined CONFIG_MTK_LEGACY*//*Jimmy*/
#if !defined CONFIG_MTK_LEGACY
#ifdef CONFIG_OF
struct platform_device mt_pmic_device = {
	.name = "pmic_regulator",
	.id = -1,
};

static const struct platform_device_id pmic_regulator_id[] = {
	{"pmic_regulator", 0},
	{},
};

static const struct of_device_id pmic_cust_of_ids[] = {
	{.compatible = "mediatek,mt6353",},
	{},
};

MODULE_DEVICE_TABLE(of, pmic_cust_of_ids);

static int pmic_regulator_init(struct platform_device *pdev)
{
	struct device_node *np, *regulators;
	int matched, i = 0, ret;

	pdev->dev.of_node = of_find_compatible_node(NULL, NULL, "mediatek,mt_pmic");
	np = of_node_get(pdev->dev.of_node);
	if (!np)
		return -EINVAL;

	regulators = of_get_child_by_name(np, "ldo_regulators");
	if (!regulators) {
		PMICLOG("[PMIC]regulators node not found\n");
		ret = -EINVAL;
		goto out;
	}
	/*matched = of_regulator_match(&pdev->dev, regulators,
				     pmic_regulator_matches,
				     ARRAY_SIZE(pmic_regulator_matches));*/
	matched = of_regulator_match(&pdev->dev, regulators,
				     pmic_regulator_matches,
				     pmic_regulator_matches_size);
	if ((matched < 0) || (matched != MT65XX_POWER_COUNT_END)) {
		pr_err("[PMIC]Error parsing regulator init data: %d %d\n", matched,
			MT65XX_POWER_COUNT_END);
		return matched;
	}

	/*for (i = 0; i < ARRAY_SIZE(pmic_regulator_matches); i++) {*/
	for (i = 0; i < pmic_regulator_matches_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			mtk_ldos[i].config.dev = &(pdev->dev);
			mtk_ldos[i].config.init_data = pmic_regulator_matches[i].init_data;
			mtk_ldos[i].config.of_node = pmic_regulator_matches[i].of_node;
			mtk_ldos[i].config.driver_data = pmic_regulator_matches[i].driver_data;
			mtk_ldos[i].desc.owner = THIS_MODULE;

			mtk_ldos[i].rdev =
			    regulator_register(&mtk_ldos[i].desc, &mtk_ldos[i].config);

			if (IS_ERR(mtk_ldos[i].rdev)) {
				ret = PTR_ERR(mtk_ldos[i].rdev);
				pr_warn("[regulator_register] failed to register %s (%d)\n",
					mtk_ldos[i].desc.name, ret);
				continue;
			} else {
				PMICLOG("[regulator_register] pass to register %s\n",
					mtk_ldos[i].desc.name);
			}
			/* To initialize varriables which were used to record status, */
			/* if ldo regulator have been modified by user.               */
			/* mtk_ldos[i].vosel.ldo_user = mtk_ldos[i].rdev->use_count;  */
			mtk_ldos[i].vosel.def_sel = mtk_regulator_get_voltage_sel(mtk_ldos[i].rdev);
			mtk_ldos[i].vosel.cur_sel = mtk_ldos[i].vosel.def_sel;

			PMICLOG("[PMIC]mtk_ldos[%d].config.init_data min_uv:%d max_uv:%d\n",
				i, mtk_ldos[i].config.init_data->constraints.min_uV,
				mtk_ldos[i].config.init_data->constraints.max_uV);
		}
	}
	of_node_put(regulators);
	return 0;

out:
	of_node_put(np);
	return ret;
}

static int pmic_mt_cust_probe(struct platform_device *pdev)
{
	struct device_node *np, *nproot, *regulators, *child;
#ifdef non_ks
	const struct of_device_id *match;
#endif
	int ret;
	unsigned int i = 0, default_on;

	PMICLOG("[PMIC]pmic_mt_cust_probe %s %s\n", pdev->name, pdev->id_entry->name);
#ifdef non_ks
	/* check if device_id is matched */
	match = of_match_device(pmic_cust_of_ids, &pdev->dev);
	if (!match) {
		pr_warn("[PMIC]pmic_cust_of_ids do not matched\n");
		return -EINVAL;
	}

	/* check customer setting */
	nproot = of_find_compatible_node(NULL, NULL, "mediatek,mt6328");
	if (nproot == NULL) {
		pr_info("[PMIC]pmic_mt_cust_probe get node failed\n");
		return -ENOMEM;
	}

	np = of_node_get(nproot);
	if (!np) {
		pr_info("[PMIC]pmic_mt_cust_probe of_node_get fail\n");
		return -ENODEV;
	}

	regulators = of_find_node_by_name(np, "regulators");
	if (!regulators) {
		pr_info("[PMIC]failed to find regulators node\n");
		ret = -ENODEV;
		goto out;
	}
	for_each_child_of_node(regulators, child) {
		/* check ldo regualtors and set it */
		/*for (i = 0; i < ARRAY_SIZE(pmic_regulator_matches); i++) {*/
		for (i = 0; i < pmic_regulator_matches_size; i++) {
			/* compare dt name & ldos name */
			if (!of_node_cmp(child->name, pmic_regulator_matches[i].name)) {
				PMICLOG("[PMIC]%s regulator_matches %s\n", child->name,
					(char *)of_get_property(child, "regulator-name", NULL));
				break;
			}
		}
		/*if (i == ARRAY_SIZE(pmic_regulator_matches))*/
		if (i == pmic_regulator_matches_size)
			continue;
		if (!of_property_read_u32(child, "regulator-default-on", &default_on)) {
			switch (default_on) {
			case 0:
				/* skip */
				PMICLOG("[PMIC]%s regulator_skip %s\n", child->name,
					pmic_regulator_matches[i].name);
				break;
			case 1:
				/* turn ldo off */
				pmic_set_register_value(mtk_ldos[i].en_reg, false);
				PMICLOG("[PMIC]%s default is off\n",
					(char *)of_get_property(child, "regulator-name", NULL));
				break;
			case 2:
				/* turn ldo on */
				pmic_set_register_value(mtk_ldos[i].en_reg, true);
				PMICLOG("[PMIC]%s default is on\n",
					(char *)of_get_property(child, "regulator-name", NULL));
				break;
			default:
				break;
			}
		}
	}
	of_node_put(regulators);
	PMICLOG("[PMIC]pmic_mt_cust_probe done\n");
	return 0;
#else
	nproot = of_find_compatible_node(NULL, NULL, "mediatek,mt_pmic");
	if (nproot == NULL) {
		pr_info("[PMIC]pmic_mt_cust_probe get node failed\n");
		return -ENOMEM;
	}

	np = of_node_get(nproot);
	if (!np) {
		pr_info("[PMIC]pmic_mt_cust_probe of_node_get fail\n");
		return -ENODEV;
	}

	regulators = of_get_child_by_name(np, "ldo_regulators");
	if (!regulators) {
		PMICLOG("[PMIC]pmic_mt_cust_probe ldo regulators node not found\n");
		ret = -ENODEV;
		goto out;
	}

	for_each_child_of_node(regulators, child) {
		/* check ldo regualtors and set it */
		if (!of_property_read_u32(child, "regulator-default-on", &default_on)) {
			switch (default_on) {
			case 0:
				/* skip */
				PMICLOG("[PMIC]%s regulator_skip %s\n", child->name,
					pmic_regulator_matches[i].name);
				break;
			case 1:
				/* turn ldo off */
				pmic_set_register_value(mtk_ldos[i].en_reg, false);
				PMICLOG("[PMIC]%s default is off\n",
					(char *)of_get_property(child, "regulator-name", NULL));
				break;
			case 2:
				/* turn ldo on */
				pmic_set_register_value(mtk_ldos[i].en_reg, true);
				PMICLOG("[PMIC]%s default is on\n",
					(char *)of_get_property(child, "regulator-name", NULL));
				break;
			default:
				break;
			}
		}
	}
	of_node_put(regulators);
	pr_err("[PMIC]pmic_mt_cust_probe done\n");
	return 0;
#endif
out:
	of_node_put(np);
	return ret;
}

static int pmic_mt_cust_remove(struct platform_device *pdev)
{
       /*platform_driver_unregister(&mt_pmic_driver);*/
	return 0;
}

static struct platform_driver mt_pmic_driver = {
	.driver = {
		   .name = "pmic_regulator",
		   .owner = THIS_MODULE,
		   .of_match_table = pmic_cust_of_ids,
		   },
	.probe = pmic_mt_cust_probe,
	.remove = pmic_mt_cust_remove,
/*      .id_table = pmic_regulator_id,*/
};
#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */

void pmic_regulator_suspend(void)
{
	int i;

	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			if (mtk_ldos[i].vol_reg != 0) {
				mtk_ldos[i].vosel.cur_sel = mtk_regulator_get_voltage_sel(mtk_ldos[i].rdev);
				if (mtk_ldos[i].vosel.cur_sel != mtk_ldos[i].vosel.def_sel) {
					mtk_ldos[i].vosel.restore = true;
					pr_err("pmic_regulator_suspend(name=%s id=%d default_sel=%d current_sel=%d)\n",
							mtk_ldos[i].rdev->desc->name, mtk_ldos[i].rdev->desc->id,
							mtk_ldos[i].vosel.def_sel, mtk_ldos[i].vosel.cur_sel);
				} else
					mtk_ldos[i].vosel.restore = false;
			}
		}
	}

}

void pmic_regulator_resume(void)
{
	int i, selector;

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			if (mtk_ldos[i].vol_reg != 0) {
				if (mtk_ldos[i].vosel.restore == true) {
					/*-- regulator voltage changed? --*/
						selector = mtk_ldos[i].vosel.cur_sel;
						pmic_set_register_value(mtk_ldos[i].vol_reg, selector);
						pr_err("pmic_regulator_resume(name=%s id=%d default_sel=%d current_sel=%d)\n",
							mtk_ldos[i].rdev->desc->name, mtk_ldos[i].rdev->desc->id,
							mtk_ldos[i].vosel.def_sel, mtk_ldos[i].vosel.cur_sel);
				}
			}
		}
	}
}

static int pmic_regulator_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
		pr_warn("[%s] pm_event %lu (IPOH)\n", __func__, pm_event);
		return NOTIFY_DONE;

	case PM_POST_HIBERNATION:	/* Hibernation finished */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		pmic_regulator_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block pmic_regulator_pm_notifier_block = {
	.notifier_call = pmic_regulator_pm_event,
	.priority = 0,
};

int mtk_regulator_init(struct platform_device *dev)
{
#if defined CONFIG_MTK_LEGACY
	int i = 0;
	/*int ret = 0;*/
	int isEn = 0;
#endif
	int ret = 0;
	/*workaround for VMC voltage */
/*
	if (pmic_get_register_value(PMIC_SWCID) == PMIC6353_E1_CID_CODE) {
		if (pmic_read_VMC_efuse() != 0) {
			PMICLOG("VMC voltage use E1_2 voltage table\n");
			mtk_ldos[MT6353_PMIC_POWER_LDO_VMC].pvoltages = (void *)mt6353_VMC_E1_2_voltages;
		} else {
			PMICLOG("VMC voltage use E1_1 voltage table\n");
			mtk_ldos[MT6353_PMIC_POWER_LDO_VMC].pvoltages = (void *)mt6353_VMC_E1_1_voltages;
		}
	}
*/
/*workaround for VIO18*/
	vio18_cal = pmic_get_register_value(PMIC_RG_VIO18_CAL);
	vmc_cal = pmic_get_register_value(PMIC_RG_VMC_CAL);
	vcamd_cal = pmic_get_register_value(PMIC_RG_VCAMD_CAL);

/*#if !defined CONFIG_MTK_LEGACY*//*Jimmy*/
#if !defined CONFIG_MTK_LEGACY
#ifdef CONFIG_OF
	pmic_regulator_init(dev);
#endif
#else
	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			mtk_ldos[i].config.dev = &(dev->dev);
			mtk_ldos[i].config.init_data = &mtk_ldos[i].init_data;
			if (mtk_ldos[i].desc.n_voltages != 1) {
				const int *pVoltage;

				if (mtk_ldos[i].vol_reg != 0) {
					if (mtk_ldos[i].pvoltages != NULL) {
						pVoltage = (const int *)mtk_ldos[i].pvoltages;
						mtk_ldos[i].init_data.constraints.max_uV =
						    pVoltage[mtk_ldos[i].desc.n_voltages - 1];
						mtk_ldos[i].init_data.constraints.min_uV =
						    pVoltage[0];
					} else {
						mtk_ldos[i].init_data.constraints.max_uV =
						    (mtk_ldos[i].desc.n_voltages -
						     1) * mtk_ldos[i].desc.uV_step +
						    mtk_ldos[i].desc.min_uV;
						mtk_ldos[i].init_data.constraints.min_uV =
						    mtk_ldos[i].desc.min_uV;
						PMICLOG("test man_uv:%d min_uv:%d\n",
							(mtk_ldos[i].desc.n_voltages -
							 1) * mtk_ldos[i].desc.uV_step +
							mtk_ldos[i].desc.min_uV,
							mtk_ldos[i].desc.min_uV);
					}
				}
				PMICLOG("min_uv:%d max_uv:%d\n",
					mtk_ldos[i].init_data.constraints.min_uV,
					mtk_ldos[i].init_data.constraints.max_uV);
			}

			mtk_ldos[i].desc.owner = THIS_MODULE;

			mtk_ldos[i].rdev =
			    regulator_register(&mtk_ldos[i].desc, &mtk_ldos[i].config);
			if (IS_ERR(mtk_ldos[i].rdev)) {
				ret = PTR_ERR(mtk_ldos[i].rdev);
				PMICLOG("[regulator_register] failed to register %s (%d)\n",
					mtk_ldos[i].desc.name, ret);
			} else {
				PMICLOG("[regulator_register] pass to register %s\n",
					mtk_ldos[i].desc.name);
			}
			mtk_ldos[i].reg = regulator_get(&(dev->dev), mtk_ldos[i].desc.name);
			isEn = regulator_is_enabled(mtk_ldos[i].reg);
			if (isEn != 0) {
				PMICLOG("[regulator] %s is default on\n", mtk_ldos[i].desc.name);
				/*ret=regulator_enable(mtk_ldos[i].reg);*/
			}
		}
	}
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
/*#if !defined CONFIG_MTK_LEGACY*//*Jimmy*/
#if !defined CONFIG_MTK_LEGACY
#ifdef CONFIG_OF
	ret = platform_driver_register(&mt_pmic_driver);
	if (ret) {
		PMICLOG("****[pmic_mt_init] Unable to register driver by DT(%d)\n", ret);
		return ret;
	}
#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */

	ret = register_pm_notifier(&pmic_regulator_pm_notifier_block);
	if (ret) {
		PMICLOG("****failed to register PM notifier %d\n", ret);
		return ret;
	}

	return ret;
}



void PMIC6353_regulator_test(void)
{
	int i = 0, j;
	int ret1, ret2;
	struct regulator *reg;

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].isUsedable == 1) {
			reg = mtk_ldos[i].reg;
			PMICLOG("[regulator enable test] %s\n", mtk_ldos[i].desc.name);

			ret1 = regulator_enable(reg);
			ret2 = regulator_is_enabled(reg);

			if (ret2 == pmic_get_register_value(mtk_ldos[i].en_reg)) {
				PMICLOG("[enable test pass]\n");
			} else {
				PMICLOG("[enable test fail] ret = %d enable = %d addr:0x%x reg:0x%x\n",
				ret1, ret2, pmu_flags_table[mtk_ldos[i].en_reg].offset,
				pmic_get_register_value(mtk_ldos[i].en_reg));
			}

			ret1 = regulator_disable(reg);
			ret2 = regulator_is_enabled(reg);

			if (ret2 == pmic_get_register_value(mtk_ldos[i].en_reg)) {
				PMICLOG("[disable test pass]\n");
			} else {
				PMICLOG("[disable test fail] ret = %d enable = %d addr:0x%x reg:0x%x\n",
					ret1, ret2, pmu_flags_table[mtk_ldos[i].en_reg].offset,
					pmic_get_register_value(mtk_ldos[i].en_reg));
			}

		}
	}

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		const int *pVoltage;

		reg = mtk_ldos[i].reg;
		if (mtk_ldos[i].isUsedable == 1) {
			PMICLOG("[regulator voltage test] %s voltage:%d\n",
				mtk_ldos[i].desc.name, mtk_ldos[i].desc.n_voltages);

			if (mtk_ldos[i].pvoltages != NULL) {
				pVoltage = (const int *)mtk_ldos[i].pvoltages;

				for (j = 0; j < mtk_ldos[i].desc.n_voltages; j++) {
					int rvoltage;

					regulator_set_voltage(reg, pVoltage[j], pVoltage[j]);
					rvoltage = regulator_get_voltage(reg);


					if ((j == pmic_get_register_value(mtk_ldos[i].vol_reg))
					    && (pVoltage[j] == rvoltage)) {
						PMICLOG
						    ("[%d:%d]:pass  set_voltage:%d  rvoltage:%d\n",
						     j,
						     pmic_get_register_value(mtk_ldos
									     [i].vol_reg),
						     pVoltage[j], rvoltage);

					} else {
						PMICLOG
						    ("[%d:%d]:fail  set_voltage:%d  rvoltage:%d\n",
						     j,
						     pmic_get_register_value(mtk_ldos
									     [i].vol_reg),
						     pVoltage[j], rvoltage);
					}
				}
			}
		}
	}


}

/*#if defined CONFIG_MTK_LEGACY*/
int getHwVoltage(MT65XX_POWER powerId)
{
#if defined CONFIG_MTK_LEGACY
	struct regulator *reg;

	if (powerId == MT65XX_POWER_NONE) {
		pr_err("[PMIC]getHwVoltage failed: plz check the powerId, since it's none\n");
		return -1;
	}

	/*if (powerId >= ARRAY_SIZE(mtk_ldos))*/
	if (powerId >= mtk_ldos_size)
		return -100;

	if (mtk_ldos[powerId].isUsedable != true)
		return -101;

	reg = mtk_ldos[powerId].reg;

	return regulator_get_voltage(reg);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(getHwVoltage);

int isHwPowerOn(MT65XX_POWER powerId)
{
#if defined CONFIG_MTK_LEGACY
	struct regulator *reg;

	if (powerId == MT65XX_POWER_NONE) {
		pr_err("[PMIC]isHwPowerOn failed: plz check the powerId, since it's none\n");
		return -1;
	}

	/*if (powerId >= ARRAY_SIZE(mtk_ldos))*/
	if (powerId >= mtk_ldos_size)
		return -100;

	if (mtk_ldos[powerId].isUsedable != true)
		return -101;

	reg = mtk_ldos[powerId].reg;

	return regulator_is_enabled(reg);
#else
	return -1;
#endif

}
EXPORT_SYMBOL(isHwPowerOn);

bool hwPowerOn(MT65XX_POWER powerId, int powerVolt, char *mode_name)
{
#if defined CONFIG_MTK_LEGACY
	struct regulator *reg;
	int ret1, ret2;

	if (powerId == MT65XX_POWER_NONE) {
		pr_err("[PMIC]hwPowerOn failed:plz check the powerId, since it's none\n");
		return -1;
	}

	/*if (powerId >= ARRAY_SIZE(mtk_ldos))*/
	if (powerId >= mtk_ldos_size)
		return false;

	if (mtk_ldos[powerId].isUsedable != true)
		return false;

	reg = mtk_ldos[powerId].reg;

	ret2 = regulator_set_voltage(reg, powerVolt, powerVolt);

	if (ret2 != 0) {
		PMICLOG("hwPowerOn:%s can't set the same volt %d again.", mtk_ldos[powerId].desc.name, powerVolt);
		PMICLOG("Or please check:%s volt %d is correct.", mtk_ldos[powerId].desc.name, powerVolt);
	}

	ret1 = regulator_enable(reg);

	if (ret1 != 0)
		PMICLOG("hwPowerOn:%s enable fail", mtk_ldos[powerId].desc.name);

	PMICLOG("hwPowerOn:%d:%s volt:%d name:%s cnt:%d", powerId, mtk_ldos[powerId].desc.name,
		powerVolt, mode_name, mtk_ldos[powerId].rdev->use_count);
	return true;
#else
	return false;
#endif
}
EXPORT_SYMBOL(hwPowerOn);

bool hwPowerDown(MT65XX_POWER powerId, char *mode_name)
{
#if defined CONFIG_MTK_LEGACY
	struct regulator *reg;
	int ret1;

	if (powerId == MT65XX_POWER_NONE) {
		pr_err("[PMIC]hwPowerDown failed:plz check the powerId, since it's none\n");
		return -1;
	}

	/*if (powerId >= ARRAY_SIZE(mtk_ldos))*/
	if (powerId >= mtk_ldos_size)
		return false;

	if (mtk_ldos[powerId].isUsedable != true)
		return false;
	reg = mtk_ldos[powerId].reg;
	ret1 = regulator_disable(reg);

	if (ret1 != 0)
		PMICLOG("hwPowerOn err:ret1:%d ", ret1);

	PMICLOG("hwPowerDown:%d:%s name:%s cnt:%d", powerId, mtk_ldos[powerId].desc.name, mode_name,
		mtk_ldos[powerId].rdev->use_count);
	return true;
#else
	return false;
#endif
}
EXPORT_SYMBOL(hwPowerDown);

bool hwPowerSetVoltage(MT65XX_POWER powerId, int powerVolt, char *mode_name)
{
#if defined CONFIG_MTK_LEGACY
	struct regulator *reg;
	int ret1;

	if (powerId == MT65XX_POWER_NONE) {
		pr_err("[PMIC]hwPowerSetVoltage failed:plz check the powerId, since it's none\n");
		return -1;
	}

	/*if (powerId >= ARRAY_SIZE(mtk_ldos))*/
	if (powerId >= mtk_ldos_size)
		return false;

	reg = mtk_ldos[powerId].reg;

	ret1 = regulator_set_voltage(reg, powerVolt, powerVolt);

	if (ret1 != 0) {
		PMICLOG("hwPowerSetVoltage:%s can't set the same voltage %d", mtk_ldos[powerId].desc.name,
			powerVolt);
	}


	PMICLOG("hwPowerSetVoltage:%d:%s name:%s cnt:%d", powerId, mtk_ldos[powerId].desc.name,
		mode_name, mtk_ldos[powerId].rdev->use_count);
	return true;
#else
	return false;
#endif
}
EXPORT_SYMBOL(hwPowerSetVoltage);

/*#endif*/ /* End of #if defined CONFIG_MTK_LEGACY */

/*****************************************************************************
 * Dump all LDO status
 ******************************************************************************/
void dump_ldo_status_read_debug(void)
{
	int i;
	int en = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage;

	pr_debug("********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		if (mtk_bucks[i].qi_en_reg != 0)
			en = pmic_get_register_value(mtk_bucks[i].qi_en_reg);
		else
			en = -1;

		if (mtk_bucks[i].qi_vol_reg != 0) {
			voltage_reg = pmic_get_register_value(mtk_bucks[i].qi_vol_reg);
			voltage =
			    mtk_bucks[i].desc.min_uV + mtk_bucks[i].desc.uV_step * voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		pr_err("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mtk_bucks[i].desc.name, en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].en_reg != 0)
			en = pmic_get_register_value(mtk_ldos[i].en_reg);
		else
			en = -1;

		if (mtk_ldos[i].desc.n_voltages != 1) {
			if (mtk_ldos[i].vol_reg != 0) {
				voltage_reg = pmic_get_register_value(mtk_ldos[i].vol_reg);
				if (mtk_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)mtk_ldos[i].pvoltages;
					/*HW LDO sequence issue, we need to change it */
					voltage = pVoltage[voltage_reg];
				} else {
					voltage =
					    mtk_ldos[i].desc.min_uV +
					    mtk_ldos[i].desc.uV_step * voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else {
			pVoltage = (const int *)mtk_ldos[i].pvoltages;
			voltage = pVoltage[0];
		}

		pr_err("%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			mtk_ldos[i].desc.name, en, voltage, voltage_reg);
	}

	PMICLOG("Power Good Status=0x%x.\n", upmu_get_reg_value(0x21c));
	PMICLOG("OC Status=0x%x.\n", upmu_get_reg_value(0x214));
	PMICLOG("Thermal Status=0x%x.\n", upmu_get_reg_value(0x21e));
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	int i;
	int en = 0;
	int voltage_reg = 0;
	int voltage = 0;
	const int *pVoltage;

	seq_puts(m, "********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		if (mtk_bucks[i].qi_en_reg != 0)
			en = pmic_get_register_value(mtk_bucks[i].qi_en_reg);
		else
			en = -1;

		if (mtk_bucks[i].qi_vol_reg != 0) {
			voltage_reg = pmic_get_register_value(mtk_bucks[i].qi_vol_reg);
			voltage =
			    mtk_bucks[i].desc.min_uV + mtk_bucks[i].desc.uV_step * voltage_reg;
		} else {
			voltage_reg = -1;
			voltage = -1;
		}
		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			   mtk_bucks[i].desc.name, en, voltage, voltage_reg);
	}

	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		if (mtk_ldos[i].en_reg != 0)
			en = pmic_get_register_value(mtk_ldos[i].en_reg);
		else
			en = -1;

		if (mtk_ldos[i].desc.n_voltages != 1) {
			if (mtk_ldos[i].vol_reg != 0) {
				voltage_reg = pmic_get_register_value(mtk_ldos[i].vol_reg);
				if (mtk_ldos[i].pvoltages != NULL) {
					pVoltage = (const int *)mtk_ldos[i].pvoltages;
					/*HW LDO sequence issue, we need to change it */
					voltage = pVoltage[voltage_reg];
				} else {
					voltage =
					    mtk_ldos[i].desc.min_uV +
					    mtk_ldos[i].desc.uV_step * voltage_reg;
				}
			} else {
				voltage_reg = -1;
				voltage = -1;
			}
		} else {
			pVoltage = (const int *)mtk_ldos[i].pvoltages;
			voltage = pVoltage[0];
		}
		seq_printf(m, "%s   status:%d     voltage:%duv    voltage_reg:%d\n",
			   mtk_ldos[i].desc.name, en, voltage, voltage_reg);
	}

	seq_printf(m, "Power Good Status=0x%x.\n", upmu_get_reg_value(0x21c));
	seq_printf(m, "OC Status=0x%x.\n", upmu_get_reg_value(0x214));
	seq_printf(m, "Thermal Status=0x%x.\n", upmu_get_reg_value(0x21e));

	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations pmic_debug_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
};

void pmic_regulator_debug_init(struct platform_device *dev, struct dentry *debug_dir)
{
	/* /sys/class/regulator/.../ */
	int ret_device_file = 0, i;
	struct dentry *mt_pmic_dir = debug_dir;

	/* /sys/class/regulator/.../ */
	/*EM BUCK voltage & Status*/
	/*for (i = 0; i < ARRAY_SIZE(mtk_bucks); i++) {*/
	for (i = 0; i < mtk_bucks_size; i++) {
		/*PMICLOG("[PMIC] register buck id=%d\n",i);*/
		ret_device_file = device_create_file(&(dev->dev), &mtk_bucks[i].en_att);
		ret_device_file = device_create_file(&(dev->dev), &mtk_bucks[i].voltage_att);
	}
	/*EM ldo voltage & Status*/
	/*for (i = 0; i < ARRAY_SIZE(mtk_ldos); i++) {*/
	for (i = 0; i < mtk_ldos_size; i++) {
		/*PMICLOG("[PMIC] register ldo id=%d\n",i);*/
		ret_device_file = device_create_file(&(dev->dev), &mtk_ldos[i].en_att);
		ret_device_file = device_create_file(&(dev->dev), &mtk_ldos[i].voltage_att);
	}

	debugfs_create_file("dump_ldo_status", S_IRUGO | S_IWUSR, mt_pmic_dir, NULL, &pmic_debug_proc_fops);
	PMICLOG("proc_create pmic_debug_proc_fops\n");

}

MODULE_AUTHOR("Argus Lin");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
