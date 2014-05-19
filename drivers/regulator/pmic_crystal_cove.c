/*
 * pmic_crystal_cove.c - Merrifield regulator driver
 * Copyright (c) 2013, Intel Corporation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/intel_crystal_cove_pmic.h>
#include <linux/regulator/machine.h>

#include <linux/mfd/intel_soc_pmic.h>

/* Intel Voltage cntrl register parameters*/
#define REG_ENA_STATUS_MASK	0x01
#define REG_VSEL_MASK		0xe0
#define VSEL_SHIFT		5

#define REG_ON			0x01
#define REG_OFF			0xfe
#define REG_CNT_ENBL		0x02

#define ON			1
#define OFF			0

const u16 reg_addr_offset[] = {
	V2P85SCNT_ADDR, V2P85SXCNT_ADDR, V3P3SXCNT_ADDR,
	V1P8SCNT_ADDR, V1P8SXCNT_ADDR, V1P0ACNT_ADDR,
	V1P8ACNT_ADDR, VSYS_SCNT_ADDR
};

static ATOMIC_NOTIFIER_HEAD(vrf_notifier);

void vrf_notifier_register(struct notifier_block *n)
{
	atomic_notifier_chain_register(&vrf_notifier, n);
}
EXPORT_SYMBOL(vrf_notifier_register);

void vrf_notifier_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&vrf_notifier, n);
}
EXPORT_SYMBOL(vrf_notifier_unregister);

void vrf_notifier_call_chain(unsigned int val)
{
	atomic_notifier_call_chain(&vrf_notifier, val, NULL);
}
EXPORT_SYMBOL(vrf_notifier_call_chain);

/**
* intel_pmic_reg_is_enabled - To check if the regulator is enabled
* @rdev:    regulator_dev structure
* @return value : 1 - Regulator is ON
*		:0 - Regulator is OFF
*		:< 0 - Error
*/
static int intel_pmic_reg_is_enabled(struct regulator_dev *rdev)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_value;

	reg_value = intel_soc_pmic_readb(pmic_info->pmic_reg);
	if (reg_value < 0) {
		dev_err(&rdev->dev,
			"intel_soc_pmic_readb returns error %08x\n", reg_value);
		return reg_value;
	}

	if (!(reg_value & REG_CNT_ENBL))
		return -EINVAL;

	return reg_value & REG_ENA_STATUS_MASK;
}
/**
* intel_pmic_reg_enable - To enable the regulator
* @rdev:    regulator_dev structure
* @return value :0 - Regulator enabling success
*		:nonzero - Regulator enabling failed
*/
static int intel_pmic_reg_enable(struct regulator_dev *rdev)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_value;

	reg_value = intel_soc_pmic_readb(pmic_info->pmic_reg);
	if (reg_value < 0) {
		dev_err(&rdev->dev,
			"intel_soc_pmic_readb returns error %08x\n", reg_value);
		return reg_value;
	}

	return intel_soc_pmic_writeb(pmic_info->pmic_reg,
				(reg_value | REG_ON | REG_CNT_ENBL));
}
/**
* intel_pmic_reg_disable - To disable the regulator
* @rdev:    regulator_dev structure
* @return value :0 - Regulator disabling success
*		:nonzero - Regulator disabling failed
*/
static int intel_pmic_reg_disable(struct regulator_dev *rdev)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_value;

	reg_value = intel_soc_pmic_readb(pmic_info->pmic_reg);
	if (reg_value < 0) {
		dev_err(&rdev->dev,
			"intel_soc_pmic_readb returns error %08x\n", reg_value);
		return reg_value;
	}

	return intel_soc_pmic_writeb(pmic_info->pmic_reg,
				((reg_value | REG_CNT_ENBL) & REG_OFF));
}
/**
* intel_pmic_reg_listvoltage - Return the voltage value,this is called
*                                   from core framework
* @rdev: regulator source
* @index : passed on from core
* @return value : Returns the value in micro volts.
 */
static int intel_pmic_reg_listvoltage(struct regulator_dev *rdev,
								unsigned index)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);

	if (index >= pmic_info->table_len) {
		dev_err(&rdev->dev, "Index out of range in listvoltage\n");
		return -EINVAL;
	}
	return pmic_info->table[index] * 1000;
}
/**
* intel_pmic_reg_getvoltage - Return the current voltage value in  uV
* @rdev:    regulator_dev structure
*  @return value : Returns the voltage value.
*/
static int intel_pmic_reg_getvoltage(struct regulator_dev *rdev)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);
	u8  vsel;
	int reg_value;

	reg_value = intel_soc_pmic_readb(pmic_info->pmic_reg);
	if (reg_value < 0) {
		dev_err(&rdev->dev,
			"intel_soc_pmic_readb returns error %08x\n", reg_value);
		return reg_value;
	}
	vsel = (reg_value & REG_VSEL_MASK) >> VSEL_SHIFT;
	if (vsel >= pmic_info->table_len) {
		dev_err(&rdev->dev, "vsel value is out of range\n");
		return -EINVAL;
	}
	dev_dbg(&rdev->dev, "Voltage value is %d mV\n",
		pmic_info->table[vsel]);
	return pmic_info->table[vsel] * 1000;
}

/**
* intel_pmic_reg_setvoltage - Set voltage to the regulator
* @rdev:    regulator_dev structure
* @min_uV: Minimum required voltage in uV
* @max_uV: Maximum acceptable voltage in uV
* @selector: Voltage value passed back to core layer
* Sets a voltage regulator to the desired output voltage
* @return value : Returns 0 if success
*			: Return error value on failure
*/
static int intel_pmic_reg_setvoltage(struct regulator_dev *rdev, int min_uV,
					int max_uV, unsigned *selector)
{
	struct intel_pmic_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_value;
	u8 vsel;

	for (vsel = 0; vsel < pmic_info->table_len; vsel++) {
		int mV = pmic_info->table[vsel];
		int uV = mV * 1000;
		if (min_uV > uV || uV > max_uV)
			continue;

		*selector = vsel;
		reg_value = intel_soc_pmic_readb(pmic_info->pmic_reg);
		if (reg_value < 0) {
			dev_err(&rdev->dev,
			"intel_soc_pmic_readb returns error %08x\n", reg_value);
			return reg_value;
		}
		reg_value &= ~REG_VSEL_MASK;
		reg_value |= vsel << VSEL_SHIFT;
		dev_dbg(&rdev->dev,
			"intel_pmic_reg_setvoltage voltage: %u uV\n", uV);
		return intel_soc_pmic_writeb(pmic_info->pmic_reg, reg_value);
	}
	return -EINVAL;
}

/* regulator_ops registration */
static struct regulator_ops intel_pmic_ops_voltage_changeable = {
	.is_enabled = intel_pmic_reg_is_enabled,
	.enable = intel_pmic_reg_enable,
	.disable = intel_pmic_reg_disable,
	.get_voltage = intel_pmic_reg_getvoltage,
	.set_voltage = intel_pmic_reg_setvoltage,
	.list_voltage = intel_pmic_reg_listvoltage,
};

/* regulator_ops registration */
static struct regulator_ops intel_pmic_ops_voltage_notchangeable = {
	.is_enabled = intel_pmic_reg_is_enabled,
	.enable = intel_pmic_reg_enable,
	.disable = intel_pmic_reg_disable,
	.get_voltage = intel_pmic_reg_getvoltage,
};
/**
* struct regulator_desc - Regulator descriptor
* Each regulator registered with the core is described with a structure of
* this type.
* @name: Identifying name for the regulator.
* @id: Numerical identifier for the regulator.
* @n_voltages: Number of selectors available for ops.list_voltage().
* @ops: Regulator operations table.
* @irq: Interrupt number for the regulator.
* @type: Indicates if the regulator is a voltage or current regulator.
* @owner: Module providing the regulator, used for refcounting.
*/
static struct regulator_desc intel_pmic_desc[] = {
	{
		.name = "v2p85s",
		.id = V2P85S,
		.ops = &intel_pmic_ops_voltage_changeable,
		.n_voltages = ARRAY_SIZE(V2P85S_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v2p85sx",
		.supply_name = "v2p85s",
		.id = V2P85SX,
		.ops = &intel_pmic_ops_voltage_notchangeable,
		.n_voltages = ARRAY_SIZE(V2P85SX_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v3p3sx",
		.id = V3P3SX,
		.ops = &intel_pmic_ops_voltage_notchangeable,
		.n_voltages = ARRAY_SIZE(V3P3SX_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v1p8s",
		.id = V1P8S,
		.ops = &intel_pmic_ops_voltage_notchangeable,
		.n_voltages = ARRAY_SIZE(V1P8S_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v1p8sx",
		.id = V1P8SX,
		.ops = &intel_pmic_ops_voltage_notchangeable,
		.n_voltages = ARRAY_SIZE(V1P8SX_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v1p0a",
		.id = V1P0A,
		.ops = &intel_pmic_ops_voltage_changeable,
		.n_voltages = ARRAY_SIZE(V1P0A_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "v1p8a",
		.id = V1P8A,
		.ops = &intel_pmic_ops_voltage_changeable,
		.n_voltages = ARRAY_SIZE(V1P8A_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},

	{
		.name = "vsys_s",
		.id = VSYS_S,
		.ops = &intel_pmic_ops_voltage_notchangeable,
		.n_voltages = ARRAY_SIZE(VSYS_S_VSEL_TABLE),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

static int crystal_cove_pmic_probe(struct platform_device *pdev)
{
	struct intel_pmic_info *pdata = dev_get_platdata(&pdev->dev);
	unsigned int i;
	static int no_of_regulator_probed;
	struct regulator_config config = { };

	if (!pdata || !pdata->pmic_reg)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(reg_addr_offset); i++) {
		if (reg_addr_offset[i] == pdata->pmic_reg)
			break;
	}

	if (i == (ARRAY_SIZE(reg_addr_offset)))
		return -EINVAL;

	/* set the initial setting for the gpio */
	if (pdata->en_pin) {
		config.ena_gpio = pdata->en_pin->gpio;
		config.ena_gpio_flags = pdata->en_pin->init_gpio_state;
	}

	config.dev = &pdev->dev;
	config.init_data = pdata->init_data;
	config.driver_data = pdata;

	pdata->intel_pmic_rdev = regulator_register(&intel_pmic_desc[i],
								&config);
	if (IS_ERR(pdata->intel_pmic_rdev)) {
		dev_err(&pdev->dev, "can't register regulator..error %ld\n",
				PTR_ERR(pdata->intel_pmic_rdev));
		return PTR_ERR(pdata->intel_pmic_rdev);
	}
	platform_set_drvdata(pdev, pdata->intel_pmic_rdev);
	dev_dbg(&pdev->dev, "registered regulator\n");

	if (++no_of_regulator_probed == ARRAY_SIZE(reg_addr_offset))
		vrf_notifier_call_chain(0);

	return 0;
}

static int crystal_cove_pmic_remove(struct platform_device *pdev)
{
	regulator_unregister(platform_get_drvdata(pdev));
	return 0;
}

static const struct platform_device_id crystal_cove_id_table[] = {
	{ "intel_regulator", 0 },
	{ },
};

MODULE_DEVICE_TABLE(platform, crystal_cove_id_table);

static struct platform_driver crystal_cove_pmic_driver = {
	.driver		= {
		.name = "intel_regulator",
		.owner = THIS_MODULE,
	},
	.probe = crystal_cove_pmic_probe,
	.remove = crystal_cove_pmic_remove,
	.id_table = crystal_cove_id_table,
};
static int __init crystal_cove_pmic_init(void)
{
	return platform_driver_register(&crystal_cove_pmic_driver);
}
late_initcall(crystal_cove_pmic_init);

static void __exit crystal_cove_pmic_exit(void)
{
	platform_driver_unregister(&crystal_cove_pmic_driver);
}
module_exit(crystal_cove_pmic_exit);

MODULE_DESCRIPTION("Crystal Cove voltage regulator driver");
MODULE_AUTHOR("Dyut/Sudarshan");
MODULE_LICENSE("GPL v2");
