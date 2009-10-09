/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __QCOM_WLAN_PWRIF_H__
#define __QCOM_WLAN_PWRIF_H__

/*
 * Headers for WLAN Power Interface Functions
 */
#include <linux/err.h>
#include <mach/mpp.h>
#include <linux/device.h>
#include <mach/vreg.h>
#include <linux/delay.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <mach/msm_xo.h>
#include <asm/mach-types.h>
#include <mach/rpm-regulator.h>

#define CHIP_POWER_ON         1
#define CHIP_POWER_OFF        0

int vos_chip_power_qrf8615(int on);
int qcomwlan_pmic_xo_core_force_enable(int on);
int qcomwlan_freq_change_1p3v_supply(enum rpm_vreg_freq freq);

#endif /* __QCOM_WLAN_PWRIF_H__ */
