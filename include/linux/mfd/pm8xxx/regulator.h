/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __MFD_PM8XXX_REGULATOR_H__
#define __MFD_PM8XXX_REGULATOR_H__

#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/pm8xxx-regulator.h>

/**
 * enum pm8xxx_regulator_type - possible PM8XXX voltage regulator types
 * %PM8XXX_REGULATOR_TYPE_PLDO:		PMOS low drop-out linear regulator
 * %PM8XXX_REGULATOR_TYPE_NLDO:		NMOS low drop-out linear regulator
 * %PM8XXX_REGULATOR_TYPE_NLDO1200:	NMOS low drop-out linear regulator
 *					capable of supplying up to 1200 mA
 * %PM8XXX_REGULATOR_TYPE_SMPS:		switched-mode power supply (buck)
 * %PM8XXX_REGULATOR_TYPE_FTSMPS:	fast transient switched-mode power
 *					supply (buck)
 * %PM8XXX_REGULATOR_TYPE_VS:		voltage switch capable of sourcing 100mA
 * %PM8XXX_REGULATOR_TYPE_VS300:	voltage switch capable of sourcing 300mA
 * %PM8XXX_REGULATOR_TYPE_NCP:		negative charge pump
 * %PM8XXX_REGULATOR_TYPE_MAX:		used internally for error checking; not
 *					a valid regulator type.
 *
 * Each of these has a different register control interface.
 */
enum pm8xxx_regulator_type {
	PM8XXX_REGULATOR_TYPE_PLDO,
	PM8XXX_REGULATOR_TYPE_NLDO,
	PM8XXX_REGULATOR_TYPE_NLDO1200,
	PM8XXX_REGULATOR_TYPE_SMPS,
	PM8XXX_REGULATOR_TYPE_FTSMPS,
	PM8XXX_REGULATOR_TYPE_VS,
	PM8XXX_REGULATOR_TYPE_VS300,
	PM8XXX_REGULATOR_TYPE_NCP,
	PM8XXX_REGULATOR_TYPE_MAX,
};

/**
 * struct pm8xxx_vreg - regulator configuration and state data used by the
 *		pm8xxx-regulator driver
 * @rdesc:		regulator description
 * @rdesc_pc:		pin control regulator description. rdesc_pc.name == NULL
 *			implies that there is no pin control version of this
 *			regulator.
 * @type:		regulator type
 * @hpm_min_load:	minimum load in uA that will result in the regulator
 *			being set to high power mode
 * @ctrl_addr:		control register SSBI address
 * @test_addr:		test register SSBI address (not needed for all types)
 * @clk_ctrl_addr:	clock control register SSBI address (only used by SMPS
 *			type regulators)
 * @sleep_ctrl_addr:	sleep control register SSBI address (only used by SMPS
 *			type regulators)
 * @pfm_ctrl_addr:	pulse-frequency modulation control register SSBI address
 *			(only used by FTSMPS type regulators)
 * @pwr_cnfg_addr:	power configuration register SSBI address (only used by
 *			FTSMPS type regulators)
 * @pdata:		this platform data struct is filled based using the
 *			platform data pointed to in a core platform data struct
 * @rdev:		pointer to regulator device which is created with
 *			regulator_register
 * @rdev_pc:		pointer to pin controlled regulator device which is
 *			created with regulator_register
 * @dev:		pointer to pm8xxx-regulator device
 * @dev_pc:		pointer to pin control pm8xxx-regulator device
 * @pc_lock:		mutex lock to handle sharing between pin controlled and
 *			non-pin controlled versions of a given regulator.  Note,
 *			this lock must be initialized in the PMIC core driver.)
 * @save_uV:		current regulator voltage in uV
 * @mode:		current mode of the regulator
 * @write_count:	number of SSBI writes that have taken place for this
 *			regulator. This is used for debug printing to determine
 *			if a given operation is redundant.
 * @prev_write_count:	number of SSBI writes that have taken place for this
 *			regulator at the start of an operation. This is used for
 *			debug printing to determine if a given operation is
 *			redundant.
 * @is_enabled:		true if the regulator is currently enabled, false if not
 * @is_enabled_pc:	true if the pin controlled version of the regulator is
 *			currently enabled (i.e. pin control is active), false if
 *			not
 * @test_reg:		last value read from or written to each of the banks of
 *			the test register
 * @ctrl_reg:		last value read from or written to the control register
 * @clk_ctrl_reg:	last value read from or written to the clock control
 *			register
 * @sleep_ctrl_reg:	last value read from or written to the sleep control
 *			register
 * @pfm_ctrl_reg:	last value read from or written to the PFM control
 *			register
 * @pwr_cnfg_reg:	last value read from or written to the power
 *			configuration register
 *
 * This data structure should only need to be instantiated in a PMIC core driver
 * It is used to specify PMIC specific as opposed to board specific
 * configuration data.  It is also used to hold all state variables needed by
 * the pm8xxx-regulator driver as these variables need to be shared between
 * pin controlled and non-pin controlled versions of a given regulator, which
 * are probed separately.
 */
struct pm8xxx_vreg {
	/* Configuration data */
	struct regulator_desc			rdesc;
	struct regulator_desc			rdesc_pc;
	enum pm8xxx_regulator_type		type;
	const int				hpm_min_load;
	const u16				ctrl_addr;
	const u16				test_addr;
	const u16				clk_ctrl_addr;
	const u16				sleep_ctrl_addr;
	const u16				pfm_ctrl_addr;
	const u16				pwr_cnfg_addr;
	/* State data */
	struct pm8xxx_regulator_platform_data	pdata;
	struct regulator_dev			*rdev;
	struct regulator_dev			*rdev_pc;
	struct device				*dev;
	struct device				*dev_pc;
	struct mutex				pc_lock;
	int					save_uV;
	int					mode;
	u32					write_count;
	u32					prev_write_count;
	bool					is_enabled;
	bool					is_enabled_pc;
	u8				test_reg[REGULATOR_TEST_BANKS_MAX];
	u8					ctrl_reg;
	u8					clk_ctrl_reg;
	u8					sleep_ctrl_reg;
	u8					pfm_ctrl_reg;
	u8					pwr_cnfg_reg;
};

/**
 * struct pm8xxx_regulator_core_platform_data - platform data specified in a
 *		PMIC core driver and utilized in the pm8xxx-regulator driver
* @vreg:		pointer to pm8xxx_vreg data structure that may be shared
*			between pin controlled and non-pin controlled versions
*			of a given regulator.  Note that this data must persist
*			as long as the regulator device is in use.
* @pdata:		pointer to platform data passed in from a board file
* @is_pin_controlled:	true if the regulator driver represents the pin control
*			portion of a regulator, false if not.
*
* This data structure should only be needed in a PMIC core driver.
*/
struct pm8xxx_regulator_core_platform_data {
	struct pm8xxx_vreg			*vreg;
	struct pm8xxx_regulator_platform_data	*pdata;
	bool					is_pin_controlled;
};

/* Helper macros */
#define PLDO(_name, _pc_name, _ctrl_addr, _test_addr, _hpm_min_load) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_PLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8XXX_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.rdesc.name	= _name, \
		.rdesc_pc.name	= _pc_name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define NLDO(_name, _pc_name, _ctrl_addr, _test_addr, _hpm_min_load) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_NLDO, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8XXX_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.rdesc.name	= _name, \
		.rdesc_pc.name	= _pc_name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define NLDO1200(_name, _ctrl_addr, _test_addr, _hpm_min_load) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_NLDO1200, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.hpm_min_load	= PM8XXX_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.rdesc.name	= _name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define SMPS(_name, _pc_name, _ctrl_addr, _test_addr, _clk_ctrl_addr, \
	     _sleep_ctrl_addr, _hpm_min_load) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_SMPS, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.clk_ctrl_addr	= _clk_ctrl_addr, \
		.sleep_ctrl_addr = _sleep_ctrl_addr, \
		.hpm_min_load	= PM8XXX_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.rdesc.name	= _name, \
		.rdesc_pc.name	= _pc_name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define FTSMPS(_name, _pwm_ctrl_addr, _fts_cnfg1_addr, _pfm_ctrl_addr, \
	       _pwr_cnfg_addr, _hpm_min_load) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_FTSMPS, \
		.ctrl_addr	= _pwm_ctrl_addr, \
		.test_addr	= _fts_cnfg1_addr, \
		.pfm_ctrl_addr = _pfm_ctrl_addr, \
		.pwr_cnfg_addr = _pwr_cnfg_addr, \
		.hpm_min_load	= PM8XXX_VREG_##_hpm_min_load##_HPM_MIN_LOAD, \
		.rdesc.name	= _name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define VS(_name, _pc_name, _ctrl_addr, _test_addr) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_VS, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.rdesc.name	= _name, \
		.rdesc_pc.name	= _pc_name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define VS300(_name, _ctrl_addr, _test_addr) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_VS300, \
		.ctrl_addr	= _ctrl_addr, \
		.test_addr	= _test_addr, \
		.rdesc.name	= _name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#define NCP(_name, _ctrl_addr) \
	{ \
		.type		= PM8XXX_REGULATOR_TYPE_NCP, \
		.ctrl_addr	= _ctrl_addr, \
		.rdesc.name	= _name, \
		.write_count	= 0, \
		.prev_write_count = -1, \
	}

#endif
