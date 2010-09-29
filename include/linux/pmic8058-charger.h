/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef __PMIC8058_CHARGER_H__
#define __PMIC8058_CHARGER_H__
/**
 * enum pmic8058_chg_state - pmic8058 charging states
 * @PMIC8058_CHG_STATE_NONE:		Initial off state
 * @PMIC8058_CHG_STATE_PWR_CHG:		Device powered from charger
 * @PMIC8058_CHG_STATE_ATC:		Device is Auto Tricke Charged (ATC)
 * @PMIC8058_CHG_STATE_PWR_BAT:		Device powered from Battery
 * @PMIC8058_CHG_STATE_ATC_FAIL:	ATC failed
 * @PMIC8058_CHG_STATE_AUX_EN:		Transient state
 * @PMIC8058_CHG_STATE_PON_AFTER_ATC:	Power on from battery and chg with limit
 *					of 90mA
 * @PMIC8058_CHG_STATE_FAST_CHG:	pmic is fast charging the battery
 * @PMIC8058_CHG_STATE_TRKL_CHG:	pmic is trck charging the battery
 * @PMIC8058_CHG_STATE_CHG_FAIL:	charging failed
 * @PMIC8058_CHG_STATE_EOC:		end of charging reached
 * @PMIC8058_CHG_STATE_INRUSH_LIMIT:	Brings up Vdd with 90mA max drawn from
 *					VBUS
 * @PMIC8058_CHG_STATE_USB_SUSPENDED:	USB supended, no current drawn from VBUS
 * @PMIC8058_CHG_STATE_PAUSE_ATC:	ATC paused
 * @PMIC8058_CHG_STATE_PAUSE_FAST_CHG:	FAST charging paused
 * @PMIC8058_CHG_STATE_PAUSE_TRKL_CHG:	TRLK charging paused
 *
 * The paused states happen when a unfavourable condition for charging is
 * detected. The most common one being the battery gets too hot ot gets
 * too cold for charging.
 */
enum pmic8058_chg_state {
	PMIC8058_CHG_STATE_NONE,
	PMIC8058_CHG_STATE_PWR_CHG,
	PMIC8058_CHG_STATE_ATC,
	PMIC8058_CHG_STATE_PWR_BAT,
	PMIC8058_CHG_STATE_ATC_FAIL,
	PMIC8058_CHG_STATE_AUX_EN,
	PMIC8058_CHG_STATE_PON_AFTER_ATC,
	PMIC8058_CHG_STATE_FAST_CHG,
	PMIC8058_CHG_STATE_TRKL_CHG,
	PMIC8058_CHG_STATE_CHG_FAIL,
	PMIC8058_CHG_STATE_EOC,
	PMIC8058_CHG_STATE_INRUSH_LIMIT,
	PMIC8058_CHG_STATE_USB_SUSPENDED,
	PMIC8058_CHG_STATE_PAUSE_ATC,
	PMIC8058_CHG_STATE_PAUSE_FAST_CHG,
	PMIC8058_CHG_STATE_PAUSE_TRKL_CHG
};

#if defined(CONFIG_BATTERY_MSM8X60) || defined(CONFIG_BATTERY_MSM8X60_MODULE)
int pmic8058_get_charge_batt(void);
int pmic8058_set_charge_batt(int);
/**
 * pmic8058_get_fsm_state -
 *
 * CONTEXT: may sleep - should not be called from non-atomic context
 *
 * RETURNS: The pmic internal state, or error otherwise
 */
enum pmic8058_chg_state pmic8058_get_fsm_state(void);
#else
int pmic8058_get_charge_batt(void)
{
	return -ENXIO;
}
int pmic8058_set_charge_batt(int)
{
	return -ENXIO;
}
enum pmic8058_chg_state pmic8058_get_fsm_state(void)
{
	return -ENXIO;
}
#endif
#endif /* __PMIC8058_CHARGER_H__ */
