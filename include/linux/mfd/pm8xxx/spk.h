/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __SPK_PM8XXX_H__
#define __SPK_PM8XXX_H__

#define PM8XXX_SPK_DEV_NAME     "pm8xxx-spk"

/**
 * struct pm8xxx_spk_pdata - SPK driver platform data
 * @spk_add_enable: variable stating SPK secondary input adding capability
 */
struct pm8xxx_spk_platform_data {
	bool spk_add_enable;
};

/*
 * pm8xxx_spk_mute - mute/unmute speaker pamp
 *
 * @mute: bool value for mute
 */
int pm8xxx_spk_mute(bool mute);

/*
 * pm8xxx_spk_gain - Set Speaker gain
 *
 * @gain: Speaker gain
 */
int pm8xxx_spk_gain(u8 gain);

/*
 * pm8xxx_spk_enable - Enable/Disable Speaker
 *
 * @enable: bool enable/disable Speaker
 */
int pm8xxx_spk_enable(int enable);

#endif /* __SPK_PM8XXX_H__ */
