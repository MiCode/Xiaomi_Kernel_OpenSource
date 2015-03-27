/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __REGULATOR_QPNP_LABIBB_REGULATOR_H__
#define __REGULATOR_QPNP_LABIBB_REGULATOR_H__

#ifdef CONFIG_REGULATOR_QPNP_LABIBB
#include <linux/regulator/driver.h>

/** This API is used to set the pull down strength of LAB regulator
 * regulator: the reglator device
 * strength: if strengh is 0, LAB regulator will be set to half strength.
 * otherwise, LAB regulator will be set to full strengh
 */
int qpnp_lab_set_pd_strength(struct regulator *regulator, u32 strength);

/** This API is used to enable pull down of LAB regulator
 * regulator: the reglator device
 * enable: if enable is true, this API will enable pull down of LAB regulator.
 * otherwise, it will disable pull down for LAB regulator
 */
int qpnp_lab_pd_enable_ctl(struct regulator *regulator, bool enable);

/** This API is used to set the power up delay for IBB regulator
 * regulator: the reglator device
 * val: the delay in us for power up of IBB regulator
 */
int qpnp_ibb_set_pwrup_dly(struct regulator *regulator, u32 val);

/** This API is used to set the power down delay for IBB regulator
 * regulator: the reglator device
 * val: the delay in us for power down of IBB regulator
 */
int qpnp_ibb_set_pwrdn_dly(struct regulator *regulator, u32 val);

/** This API is used to set the pull down strength of IBB regulator
 * regulator: the reglator device
 * strength: if strengh is 0, IBB regulator will be set to half strength.
 * otherwise, IBB regulator will be set to full strengh
 */
int qpnp_ibb_set_pd_strength(struct regulator *regulator, u32 strength);

/** This API is used to enable pull down of IBB regulator
 * regulator: the reglator device
 * enable: if enable is true, this API will enable pull down of IBB regulator.
 * otherwise, it will disable pull down for IBB regulator
 */
int qpnp_ibb_pd_enable_ctl(struct regulator *regulator, bool enable);
#else

static int qpnp_lab_set_pd_strength(struct regulator *regulator, u32 strength)
{
	return -ENODEV;
}

static int qpnp_lab_pd_enable_ctl(struct regulator *regulator, bool enable)
{
	return -ENODEV;
}

static inline int qpnp_ibb_set_pwrup_dly(struct regulator *regulator, u32 val)
{
	return -ENODEV;
}

static inline int qpnp_ibb_set_pwrdn_dly(struct regulator *regulator, u32 val)
{
	return -ENODEV;
}

static int qpnp_ibb_set_pd_strength(struct regulator *regulator, u32 strength)
{
	return -ENODEV;
}

static int qpnp_ibb_pd_enable_ctl(struct regulator *regulator, bool enable)
{
	return -ENODEV;
}
#endif /* CONFIG_REGULATOR_QPNP_LABIBB */

#endif /* __REGULATOR_QPNP_LABIBB_REGULATOR_H__ */
