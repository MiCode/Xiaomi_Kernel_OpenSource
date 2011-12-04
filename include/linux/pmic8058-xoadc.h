/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm XOADC Driver header file
 */

#ifndef _PMIC8058_XOADC_H_
#define _PMIC8058_XOADC_H_

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/workqueue.h>

struct xoadc_conv_state {
	struct adc_conv_slot	*context;
	struct list_head	slots;
	struct mutex		list_lock;
};

#define CHANNEL_VCOIN		0
#define CHANNEL_VBAT		1
#define CHANNEL_VCHG		2
#define CHANNEL_CHG_MONITOR	3
#define CHANNEL_VPH_PWR		4
#define CHANNEL_MPP5		5
#define CHANNEL_MPP6		6
#define CHANNEL_MPP7		7
#define CHANNEL_MPP8		8
#define CHANNEL_MPP9		9
#define CHANNEL_USB_VBUS	0Xa
#define CHANNEL_DIE_TEMP	0Xb
#define CHANNEL_INTERNAL	0xc
#define CHANNEL_125V		0xd
#define CHANNEL_INTERNAL_2	0Xe
#define CHANNEL_MUXOFF		0xf

#define XOADC_MPP_3		0x2
#define XOADC_MPP_4             0X3
#define XOADC_MPP_5             0x4
#define XOADC_MPP_7             0x6
#define XOADC_MPP_8             0x7
#define XOADC_MPP_10		0X9

#define XOADC_PMIC_0		0x0

#define CHANNEL_ADC_625_MV      625

struct xoadc_platform_data {
	struct adc_properties *xoadc_prop;
	u32 (*xoadc_setup) (void);
	void (*xoadc_shutdown) (void);
	void (*xoadc_mpp_config) (void);
	int (*xoadc_vreg_set) (int);
	int (*xoadc_vreg_setup) (void);
	void (*xoadc_vreg_shutdown) (void);
	u32 xoadc_num;
	u32 xoadc_wakeup;
};

#ifdef CONFIG_PMIC8058_XOADC
int32_t pm8058_xoadc_read_adc_code(uint32_t adc_instance, int32_t *data);

int32_t pm8058_xoadc_select_chan_and_start_conv(uint32_t adc_instance,
						struct adc_conv_slot *slot);

void pm8058_xoadc_slot_request(uint32_t adc_instance,
		struct adc_conv_slot **slot);

void pm8058_xoadc_restore_slot(uint32_t adc_instance,
		struct adc_conv_slot *slot);

struct adc_properties *pm8058_xoadc_get_properties(uint32_t dev_instance);

int32_t pm8058_xoadc_calibrate(uint32_t dev_instance,
		struct adc_conv_slot *slot, int * calib_status);

int32_t pm8058_xoadc_registered(void);

int32_t pm8058_xoadc_calib_device(uint32_t adc_instance);

#else

static inline int32_t pm8058_xoadc_read_adc_code(uint32_t adc_instance,
		int32_t *data)
{ return -ENXIO; }

static inline int32_t pm8058_xoadc_select_chan_and_start_conv(
		uint32_t adc_instance, struct adc_conv_slot *slot)
{ return -ENXIO; }

static inline void pm8058_xoadc_slot_request(uint32_t adc_instance,
		struct adc_conv_slot **slot)
{ return; }

static inline void pm8058_xoadc_restore_slot(uint32_t adc_instance,
		struct adc_conv_slot *slot)
{ return; }

static inline struct adc_properties *pm8058_xoadc_get_properties(
		uint32_t dev_instance)
{ return NULL; }

static inline int32_t pm8058_xoadc_calibrate(uint32_t dev_instance,
		struct adc_conv_slot *slot, int *calib_status)
{ return -ENXIO; }

static inline int32_t pm8058_xoadc_registered(void)
{ return -ENXIO; }

static inline int32_t pm8058_xoadc_calib_device(uint32_t adc_instance)
{ return -ENXIO; }
#endif
#endif
