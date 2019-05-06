/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include "thermal_core.h"

/* QPNP VADC TM register definition */
#define QPNP_REVISION3					0x2
#define QPNP_PERPH_SUBTYPE				0x5
#define QPNP_PERPH_TYPE2				0x2
#define QPNP_REVISION_EIGHT_CHANNEL_SUPPORT		2
#define QPNP_PERPH_SUBTYPE_TWO_CHANNEL_SUPPORT		0x22
#define QPNP_STATUS1					0x8
#define QPNP_STATUS1_OP_MODE				4
#define QPNP_STATUS1_MEAS_INTERVAL_EN_STS		BIT(2)
#define QPNP_STATUS1_REQ_STS				BIT(1)
#define QPNP_STATUS1_EOC				BIT(0)
#define QPNP_STATUS2					0x9
#define QPNP_STATUS2_CONV_SEQ_STATE			6
#define QPNP_STATUS2_FIFO_NOT_EMPTY_FLAG		BIT(1)
#define QPNP_STATUS2_CONV_SEQ_TIMEOUT_STS		BIT(0)
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_MODE_CTL					0x40
#define QPNP_OP_MODE_SHIFT				3
#define QPNP_VREF_XO_THM_FORCE				BIT(2)
#define QPNP_AMUX_TRIM_EN				BIT(1)
#define QPNP_ADC_TRIM_EN				BIT(0)
#define QPNP_EN_CTL1					0x46
#define QPNP_ADC_TM_EN					BIT(7)
#define QPNP_BTM_CONV_REQ				0x47
#define QPNP_ADC_CONV_REQ_EN				BIT(7)

#define QPNP_ADC_DIG_PARAM				0x50
#define QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT		3
#define QPNP_HW_SETTLE_DELAY				0x51
#define QPNP_CONV_SEQ_CTL				0x54
#define QPNP_CONV_SEQ_HOLDOFF_SHIFT			4
#define QPNP_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL			0x57
#define QPNP_ADC_TM_MEAS_INTERVAL_TIME_SHIFT		0x3
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2			0x58
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_SHIFT		0x4
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_MASK		0xf0
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL3_MASK		0xf

#define QPNP_ADC_MEAS_INTERVAL_OP_CTL                   0x59
#define QPNP_ADC_MEAS_INTERVAL_OP                       BIT(7)

#define QPNP_OP_MODE_SHIFT				3
#define QPNP_CONV_REQ					0x52
#define QPNP_CONV_REQ_SET				BIT(7)

#define QPNP_FAST_AVG_CTL				0x5a
#define QPNP_FAST_AVG_EN				0x5b
#define QPNP_FAST_AVG_ENABLED				BIT(7)

#define QPNP_M0_LOW_THR_LSB				0x5c
#define QPNP_M0_LOW_THR_MSB				0x5d
#define QPNP_M0_HIGH_THR_LSB				0x5e
#define QPNP_M0_HIGH_THR_MSB				0x5f
#define QPNP_M1_ADC_CH_SEL_CTL				0x68
#define QPNP_M1_LOW_THR_LSB				0x69
#define QPNP_M1_LOW_THR_MSB				0x6a
#define QPNP_M1_HIGH_THR_LSB				0x6b
#define QPNP_M1_HIGH_THR_MSB				0x6c
#define QPNP_M2_ADC_CH_SEL_CTL				0x70
#define QPNP_M2_LOW_THR_LSB				0x71
#define QPNP_M2_LOW_THR_MSB				0x72
#define QPNP_M2_HIGH_THR_LSB				0x73
#define QPNP_M2_HIGH_THR_MSB				0x74
#define QPNP_M3_ADC_CH_SEL_CTL				0x78
#define QPNP_M3_LOW_THR_LSB				0x79
#define QPNP_M3_LOW_THR_MSB				0x7a
#define QPNP_M3_HIGH_THR_LSB				0x7b
#define QPNP_M3_HIGH_THR_MSB				0x7c
#define QPNP_M4_ADC_CH_SEL_CTL				0x80
#define QPNP_M4_LOW_THR_LSB				0x81
#define QPNP_M4_LOW_THR_MSB				0x82
#define QPNP_M4_HIGH_THR_LSB				0x83
#define QPNP_M4_HIGH_THR_MSB				0x84
#define QPNP_M5_ADC_CH_SEL_CTL				0x88
#define QPNP_M5_LOW_THR_LSB				0x89
#define QPNP_M5_LOW_THR_MSB				0x8a
#define QPNP_M5_HIGH_THR_LSB				0x8b
#define QPNP_M5_HIGH_THR_MSB				0x8c
#define QPNP_M6_ADC_CH_SEL_CTL				0x90
#define QPNP_M6_LOW_THR_LSB				0x91
#define QPNP_M6_LOW_THR_MSB				0x92
#define QPNP_M6_HIGH_THR_LSB				0x93
#define QPNP_M6_HIGH_THR_MSB				0x94
#define QPNP_M7_ADC_CH_SEL_CTL				0x98
#define QPNP_M7_LOW_THR_LSB				0x99
#define QPNP_M7_LOW_THR_MSB				0x9a
#define QPNP_M7_HIGH_THR_LSB				0x9b
#define QPNP_M7_HIGH_THR_MSB				0x9c

#define QPNP_ADC_TM_MULTI_MEAS_EN			0x41
#define QPNP_ADC_TM_MULTI_MEAS_EN_M0			BIT(0)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M1			BIT(1)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M2			BIT(2)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M3			BIT(3)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M4			BIT(4)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M5			BIT(5)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M6			BIT(6)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M7			BIT(7)
#define QPNP_ADC_TM_LOW_THR_INT_EN			0x42
#define QPNP_ADC_TM_LOW_THR_INT_EN_M0			BIT(0)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M1			BIT(1)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M2			BIT(2)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M3			BIT(3)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M4			BIT(4)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M5			BIT(5)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M6			BIT(6)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M7			BIT(7)
#define QPNP_ADC_TM_HIGH_THR_INT_EN			0x43
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M0			BIT(0)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M1			BIT(1)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M2			BIT(2)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M3			BIT(3)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M4			BIT(4)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M5			BIT(5)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M6			BIT(6)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M7			BIT(7)

#define QPNP_ADC_TM_M0_MEAS_INTERVAL_CTL			0x59
#define QPNP_ADC_TM_M1_MEAS_INTERVAL_CTL			0x6d
#define QPNP_ADC_TM_M2_MEAS_INTERVAL_CTL			0x75
#define QPNP_ADC_TM_M3_MEAS_INTERVAL_CTL			0x7d
#define QPNP_ADC_TM_M4_MEAS_INTERVAL_CTL			0x85
#define QPNP_ADC_TM_M5_MEAS_INTERVAL_CTL			0x8d
#define QPNP_ADC_TM_M6_MEAS_INTERVAL_CTL			0x95
#define QPNP_ADC_TM_M7_MEAS_INTERVAL_CTL			0x9d

#define QPNP_ADC_TM_STATUS1				0x8
#define QPNP_ADC_TM_STATUS_LOW				0xa
#define QPNP_ADC_TM_STATUS_HIGH				0xb

#define QPNP_ADC_TM_M0_LOW_THR				0x5d5c
#define QPNP_ADC_TM_M0_HIGH_THR				0x5f5e
#define QPNP_ADC_TM_MEAS_INTERVAL			0x0

#define QPNP_ADC_TM_THR_LSB_MASK(val)			(val & 0xff)
#define QPNP_ADC_TM_THR_MSB_MASK(val)			((val & 0xff00) >> 8)

#define QPNP_MIN_TIME			2000
#define QPNP_MAX_TIME			2100
#define QPNP_RETRY			1000

/* QPNP ADC TM HC start */
#define QPNP_BTM_HC_STATUS1				0x08
#define QPNP_BTM_HC_STATUS_LOW				0x0a
#define QPNP_BTM_HC_STATUS_HIGH				0x0b

#define QPNP_BTM_HC_ADC_DIG_PARAM			0x42
#define QPNP_BTM_HC_FAST_AVG_CTL			0x43
#define QPNP_BTM_EN_CTL1				0x46
#define QPNP_BTM_CONV_REQ				0x47

#define QPNP_BTM_MEAS_INTERVAL_CTL			0x50
#define QPNP_BTM_MEAS_INTERVAL_CTL2			0x51
#define QPNP_BTM_MEAS_INTERVAL_CTL_PM5			0x44
#define QPNP_BTM_MEAS_INTERVAL_CTL2_PM5		0x45
#define QPNP_ADC_TM_MEAS_INTERVAL_TIME_SHIFT		0x3
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_SHIFT		0x4
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_MASK		0xf0
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL3_MASK		0xf

#define QPNP_BTM_Mn_ADC_CH_SEL_CTL(n)		((n * 8) + 0x60)
#define QPNP_BTM_Mn_LOW_THR0(n)			((n * 8) + 0x61)
#define QPNP_BTM_Mn_LOW_THR1(n)			((n * 8) + 0x62)
#define QPNP_BTM_Mn_HIGH_THR0(n)		((n * 8) + 0x63)
#define QPNP_BTM_Mn_HIGH_THR1(n)		((n * 8) + 0x64)
#define QPNP_BTM_Mn_MEAS_INTERVAL_CTL(n)	((n * 8) + 0x65)
#define QPNP_BTM_Mn_CTL(n)			((n * 8) + 0x66)
#define QPNP_BTM_CTL_HW_SETTLE_DELAY_MASK	0xf
#define QPNP_BTM_CTL_CAL_SEL			0x30
#define QPNP_BTM_CTL_CAL_SEL_MASK_SHIFT		4
#define QPNP_BTM_CTL_CAL_VAL			0x40

#define QPNP_BTM_Mn_EN(n)			((n * 8) + 0x67)
#define QPNP_BTM_Mn_MEAS_EN			BIT(7)
#define QPNP_BTM_Mn_HIGH_THR_INT_EN		BIT(1)
#define QPNP_BTM_Mn_LOW_THR_INT_EN		BIT(0)

#define QPNP_BTM_Mn_DATA0(n)			((n * 2) + 0xa0)
#define QPNP_BTM_Mn_DATA1(n)			((n * 2) + 0xa1)
#define QPNP_BTM_CHANNELS			8

#define QPNP_ADC_WAKEUP_SRC_TIMEOUT_MS          2000

/* QPNP ADC TM HC end */

struct qpnp_adc_thr_info {
	u8		status_low;
	u8		status_high;
	u8		qpnp_adc_tm_meas_en;
	u8		adc_tm_low_enable;
	u8		adc_tm_high_enable;
	u8		adc_tm_low_thr_set;
	u8		adc_tm_high_thr_set;
	spinlock_t	adc_tm_low_lock;
	spinlock_t	adc_tm_high_lock;
};

struct qpnp_adc_thr_client_info {
	struct list_head		list;
	struct qpnp_adc_tm_btm_param	*btm_param;
	int32_t				low_thr_requested;
	int32_t				high_thr_requested;
	enum qpnp_state_request		state_requested;
	enum qpnp_state_request		state_req_copy;
	bool				low_thr_set;
	bool				high_thr_set;
	bool				notify_low_thr;
	bool				notify_high_thr;
};

struct qpnp_adc_tm_sensor {
	struct thermal_zone_device	*tz_dev;
	struct qpnp_adc_tm_chip		*chip;
	enum thermal_device_mode	mode;
	uint32_t			sensor_num;
	enum qpnp_adc_meas_timer_select	timer_select;
	uint32_t			meas_interval;
	uint32_t			low_thr;
	uint32_t			high_thr;
	uint32_t			btm_channel_num;
	uint32_t			vadc_channel_num;
	struct workqueue_struct		*req_wq;
	struct work_struct		work;
	bool				thermal_node;
	uint32_t			scale_type;
	struct list_head		thr_list;
	bool				high_thr_triggered;
	bool				low_thr_triggered;
};

struct qpnp_adc_tm_chip {
	struct device			*dev;
	struct qpnp_adc_drv		*adc;
	struct list_head		list;
	bool				adc_tm_initialized;
	bool				adc_tm_recalib_check;
	int				max_channels_available;
	struct qpnp_vadc_chip		*vadc_dev;
	struct workqueue_struct		*high_thr_wq;
	struct workqueue_struct		*low_thr_wq;
	struct workqueue_struct		*thr_wq;
	struct work_struct		trigger_high_thr_work;
	struct work_struct		trigger_low_thr_work;
	struct work_struct		trigger_thr_work;
	bool				adc_vote_enable;
	struct qpnp_adc_thr_info	th_info;
	bool				adc_tm_hc;
	struct qpnp_adc_tm_sensor	sensor[0];
};

LIST_HEAD(qpnp_adc_tm_device_list);

struct qpnp_adc_tm_trip_reg_type {
	enum qpnp_adc_tm_channel_select	btm_amux_chan;
	uint16_t			low_thr_lsb_addr;
	uint16_t			low_thr_msb_addr;
	uint16_t			high_thr_lsb_addr;
	uint16_t			high_thr_msb_addr;
	u8				multi_meas_en;
	u8				low_thr_int_chan_en;
	u8				high_thr_int_chan_en;
	u8				meas_interval_ctl;
};

static struct qpnp_adc_tm_trip_reg_type adc_tm_data[] = {
	[QPNP_ADC_TM_CHAN0] = {QPNP_ADC_TM_M0_ADC_CH_SEL_CTL,
		QPNP_M0_LOW_THR_LSB,
		QPNP_M0_LOW_THR_MSB, QPNP_M0_HIGH_THR_LSB,
		QPNP_M0_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M0,
		QPNP_ADC_TM_LOW_THR_INT_EN_M0, QPNP_ADC_TM_HIGH_THR_INT_EN_M0,
		QPNP_ADC_TM_M0_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN1] = {QPNP_ADC_TM_M1_ADC_CH_SEL_CTL,
		QPNP_M1_LOW_THR_LSB,
		QPNP_M1_LOW_THR_MSB, QPNP_M1_HIGH_THR_LSB,
		QPNP_M1_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M1,
		QPNP_ADC_TM_LOW_THR_INT_EN_M1, QPNP_ADC_TM_HIGH_THR_INT_EN_M1,
		QPNP_ADC_TM_M1_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN2] = {QPNP_ADC_TM_M2_ADC_CH_SEL_CTL,
		QPNP_M2_LOW_THR_LSB,
		QPNP_M2_LOW_THR_MSB, QPNP_M2_HIGH_THR_LSB,
		QPNP_M2_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M2,
		QPNP_ADC_TM_LOW_THR_INT_EN_M2, QPNP_ADC_TM_HIGH_THR_INT_EN_M2,
		QPNP_ADC_TM_M2_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN3] = {QPNP_ADC_TM_M3_ADC_CH_SEL_CTL,
		QPNP_M3_LOW_THR_LSB,
		QPNP_M3_LOW_THR_MSB, QPNP_M3_HIGH_THR_LSB,
		QPNP_M3_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M3,
		QPNP_ADC_TM_LOW_THR_INT_EN_M3, QPNP_ADC_TM_HIGH_THR_INT_EN_M3,
		QPNP_ADC_TM_M3_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN4] = {QPNP_ADC_TM_M4_ADC_CH_SEL_CTL,
		QPNP_M4_LOW_THR_LSB,
		QPNP_M4_LOW_THR_MSB, QPNP_M4_HIGH_THR_LSB,
		QPNP_M4_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M4,
		QPNP_ADC_TM_LOW_THR_INT_EN_M4, QPNP_ADC_TM_HIGH_THR_INT_EN_M4,
		QPNP_ADC_TM_M4_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN5] = {QPNP_ADC_TM_M5_ADC_CH_SEL_CTL,
		QPNP_M5_LOW_THR_LSB,
		QPNP_M5_LOW_THR_MSB, QPNP_M5_HIGH_THR_LSB,
		QPNP_M5_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M5,
		QPNP_ADC_TM_LOW_THR_INT_EN_M5, QPNP_ADC_TM_HIGH_THR_INT_EN_M5,
		QPNP_ADC_TM_M5_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN6] = {QPNP_ADC_TM_M6_ADC_CH_SEL_CTL,
		QPNP_M6_LOW_THR_LSB,
		QPNP_M6_LOW_THR_MSB, QPNP_M6_HIGH_THR_LSB,
		QPNP_M6_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M6,
		QPNP_ADC_TM_LOW_THR_INT_EN_M6, QPNP_ADC_TM_HIGH_THR_INT_EN_M6,
		QPNP_ADC_TM_M6_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_CHAN7] = {QPNP_ADC_TM_M7_ADC_CH_SEL_CTL,
		QPNP_M7_LOW_THR_LSB,
		QPNP_M7_LOW_THR_MSB, QPNP_M7_HIGH_THR_LSB,
		QPNP_M7_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M7,
		QPNP_ADC_TM_LOW_THR_INT_EN_M7, QPNP_ADC_TM_HIGH_THR_INT_EN_M7,
		QPNP_ADC_TM_M7_MEAS_INTERVAL_CTL},
};

static struct qpnp_adc_tm_reverse_scale_fn adc_tm_rscale_fn[] = {
	[SCALE_R_VBATT] = {qpnp_adc_vbatt_rscaler},
	[SCALE_RBATT_THERM] = {qpnp_adc_btm_scaler},
	[SCALE_R_USB_ID] = {qpnp_adc_usb_scaler},
	[SCALE_RPMIC_THERM] = {qpnp_adc_scale_millidegc_pmic_voltage_thr},
	[SCALE_R_SMB_BATT_THERM] = {qpnp_adc_smb_btm_rscaler},
	[SCALE_R_ABSOLUTE] = {qpnp_adc_absolute_rthr},
	[SCALE_QRD_SKUH_RBATT_THERM] = {qpnp_adc_qrd_skuh_btm_scaler},
	[SCALE_QRD_SKUT1_RBATT_THERM] = {qpnp_adc_qrd_skut1_btm_scaler},
	[SCALE_QRD_215_RBATT_THERM] = {qpnp_adc_qrd_215_btm_scaler},
};

static int32_t qpnp_adc_tm_read_reg(struct qpnp_adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->adc->regmap, (chip->adc->offset + reg),
								data, len);
	if (rc < 0)
		pr_err("adc-tm read reg %d failed with %d\n", reg, rc);

	return rc;
}

static int32_t qpnp_adc_tm_write_reg(struct qpnp_adc_tm_chip *chip,
					int16_t reg, u8 data, int len)
{
	int rc = 0;
	u8 *buf;

	buf = &data;

	rc = regmap_bulk_write(chip->adc->regmap, (chip->adc->offset + reg),
								buf, len);
	if (rc < 0)
		pr_err("adc-tm write reg %d failed with %d\n", reg, rc);

	return rc;
}

static int32_t qpnp_adc_tm_fast_avg_en(struct qpnp_adc_tm_chip *chip,
				uint32_t *fast_avg_sample)
{
	int rc = 0, version = 0;
	u8 fast_avg_en = 0;

	version = qpnp_adc_get_revid_version(chip->dev);
	if (!((version == QPNP_REV_ID_8916_1_0) ||
		(version == QPNP_REV_ID_8916_1_1) ||
		(version == QPNP_REV_ID_8916_2_0))) {
		pr_debug("fast-avg-en not required for this version\n");
		return rc;
	}

	fast_avg_en = QPNP_FAST_AVG_ENABLED;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_FAST_AVG_EN, fast_avg_en, 1);
	if (rc < 0) {
		pr_err("adc-tm fast-avg enable err\n");
		return rc;
	}

	if (*fast_avg_sample >= 3)
		*fast_avg_sample = 2;

	return rc;
}

static int qpnp_adc_tm_check_vreg_vote(struct qpnp_adc_tm_chip *chip)
{
	int rc = 0;

	if (!chip->adc_vote_enable) {
		if (chip->adc->hkadc_ldo && chip->adc->hkadc_ldo_ok) {
			rc = qpnp_adc_enable_voltage(chip->adc);
			if (rc) {
				pr_err("failed enabling VADC LDO\n");
				return rc;
			}
			chip->adc_vote_enable = true;
		}
	}

	return rc;
}

static int32_t qpnp_adc_tm_enable(struct qpnp_adc_tm_chip *chip)
{
	int rc = 0;
	u8 data = 0;

	rc = qpnp_adc_tm_check_vreg_vote(chip);
	if (rc) {
		pr_err("ADC TM VREG enable failed:%d\n", rc);
		return rc;
	}

	data = QPNP_ADC_TM_EN;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_EN_CTL1, data, 1);
	if (rc < 0) {
		pr_err("adc-tm enable failed\n");
		return rc;
	}

	if (chip->adc_tm_hc) {
		data = QPNP_ADC_CONV_REQ_EN;
		rc = qpnp_adc_tm_write_reg(chip, QPNP_BTM_CONV_REQ, data, 1);
		if (rc < 0) {
			pr_err("adc-tm enable failed\n");
			return rc;
		}
	}
	return rc;
}

static int32_t qpnp_adc_tm_disable(struct qpnp_adc_tm_chip *chip)
{
	u8 data = 0;
	int rc = 0;

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_write_reg(chip, QPNP_EN_CTL1, data, 1);
		if (rc < 0) {
			pr_err("adc-tm disable failed\n");
			return rc;
		}
	}

	return rc;
}

static int qpnp_adc_tm_is_valid(struct qpnp_adc_tm_chip *chip)
{
	struct qpnp_adc_tm_chip *adc_tm_chip = NULL;

	list_for_each_entry(adc_tm_chip, &qpnp_adc_tm_device_list, list)
		if (chip == adc_tm_chip)
			return 0;

	return -EINVAL;
}

static int32_t qpnp_adc_tm_rc_check_channel_en(struct qpnp_adc_tm_chip *chip)
{
	u8 adc_tm_ctl = 0, status_low = 0, status_high = 0;
	int rc = 0, i = 0;
	bool ldo_en = false;

	for (i = 0; i < chip->max_channels_available; i++) {
		rc = qpnp_adc_tm_read_reg(chip, QPNP_BTM_Mn_CTL(i),
							&adc_tm_ctl, 1);
		if (rc) {
			pr_err("adc-tm-tm read ctl failed with %d\n", rc);
			return rc;
		}

		adc_tm_ctl &= QPNP_BTM_Mn_MEAS_EN;
		status_low = adc_tm_ctl & QPNP_BTM_Mn_LOW_THR_INT_EN;
		status_high = adc_tm_ctl & QPNP_BTM_Mn_HIGH_THR_INT_EN;

		/* Enable only if there are pending measurement requests */
		if ((adc_tm_ctl && status_high) ||
					(adc_tm_ctl && status_low)) {
			qpnp_adc_tm_enable(chip);
			ldo_en = true;

			/* Request conversion */
			rc = qpnp_adc_tm_write_reg(chip, QPNP_CONV_REQ,
							QPNP_CONV_REQ_SET, 1);
			if (rc < 0) {
				pr_err("adc-tm request conversion failed\n");
				return rc;
			}
		}
		break;
	}

	if (!ldo_en) {
		/* disable the vote if applicable */
		if (chip->adc_vote_enable && chip->adc->hkadc_ldo &&
					chip->adc->hkadc_ldo_ok) {
			qpnp_adc_disable_voltage(chip->adc);
			chip->adc_vote_enable = false;
		}
	}

	return rc;
}

static int32_t qpnp_adc_tm_enable_if_channel_meas(
					struct qpnp_adc_tm_chip *chip)
{
	u8 adc_tm_meas_en = 0, status_low = 0, status_high = 0;
	int rc = 0;

	if (chip->adc_tm_hc) {
		rc = qpnp_adc_tm_rc_check_channel_en(chip);
		if (rc) {
			pr_err("adc_tm channel check failed\n");
			return rc;
		}
	} else {
		/* Check if a measurement request is still required */
		rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
				&adc_tm_meas_en, 1);
		if (rc) {
			pr_err("read status high failed with %d\n", rc);
			return rc;
		}
		rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_LOW_THR_INT_EN,
				&status_low, 1);
		if (rc) {
			pr_err("read status low failed with %d\n", rc);
			return rc;
		}

		rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_HIGH_THR_INT_EN,
							&status_high, 1);
		if (rc) {
			pr_err("read status high failed with %d\n", rc);
			return rc;
		}

		/* Enable only if there are pending measurement requests */
		if ((adc_tm_meas_en && status_high) ||
				(adc_tm_meas_en && status_low)) {
			qpnp_adc_tm_enable(chip);
			/* Request conversion */
			rc = qpnp_adc_tm_write_reg(chip, QPNP_CONV_REQ,
							QPNP_CONV_REQ_SET, 1);
			if (rc < 0) {
				pr_err("adc-tm request conversion failed\n");
				return rc;
			}
		} else {
			/* disable the vote if applicable */
			if (chip->adc_vote_enable && chip->adc->hkadc_ldo &&
					chip->adc->hkadc_ldo_ok) {
				qpnp_adc_disable_voltage(chip->adc);
				chip->adc_vote_enable = false;
			}
		}
	}
	return rc;
}

static int32_t qpnp_adc_tm_mode_select(struct qpnp_adc_tm_chip *chip,
								u8 mode_ctl)
{
	int rc;

	mode_ctl |= (QPNP_ADC_TRIM_EN | QPNP_AMUX_TRIM_EN);

	/* VADC_BTM current sets mode to recurring measurements */
	rc = qpnp_adc_tm_write_reg(chip, QPNP_MODE_CTL, mode_ctl, 1);
	if (rc < 0)
		pr_err("adc-tm write mode selection err\n");

	return rc;
}

static int32_t qpnp_adc_tm_req_sts_check(struct qpnp_adc_tm_chip *chip)
{
	u8 status1 = 0, mode_ctl = 0;
	int rc, count = 0;

	/* Re-enable the peripheral */
	rc = qpnp_adc_tm_enable(chip);
	if (rc) {
		pr_err("adc-tm re-enable peripheral failed\n");
		return rc;
	}

	/* The VADC_TM bank needs to be disabled for new conversion request */
	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS1, &status1, 1);
	if (rc) {
		pr_err("adc-tm read status1 failed\n");
		return rc;
	}

	/* Disable the bank if a conversion is occurring */
	while (status1 & QPNP_STATUS1_REQ_STS) {
		if (count > QPNP_RETRY) {
			pr_err("retry error=%d with 0x%x\n", count, status1);
			break;
		}
		/*
		 * Wait time is based on the optimum sampling rate
		 * and adding enough time buffer to account for ADC conversions
		 * occurring on different peripheral banks
		 */
		usleep_range(QPNP_MIN_TIME, QPNP_MAX_TIME);
		rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS1,
							&status1, 1);
		if (rc < 0) {
			pr_err("adc-tm disable failed\n");
			return rc;
		}
		count++;
	}

	if (!chip->adc_tm_hc) {
		/* Change the mode back to recurring measurement mode */
		mode_ctl = ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
		rc = qpnp_adc_tm_mode_select(chip, mode_ctl);
		if (rc < 0) {
			pr_err("adc-tm mode change to recurring failed\n");
			return rc;
		}
	}

	/* Disable the peripheral */
	rc = qpnp_adc_tm_disable(chip);
	if (rc < 0) {
		pr_err("adc-tm peripheral disable failed\n");
		return rc;
	}

	return rc;
}

static int32_t qpnp_adc_tm_get_btm_idx(struct qpnp_adc_tm_chip *chip,
				uint32_t btm_chan, uint32_t *btm_chan_idx)
{
	int rc = 0, i;
	bool chan_found = false;

	if (!chip->adc_tm_hc) {
		for (i = 0; i < QPNP_ADC_TM_CHAN_NONE; i++) {
			if (adc_tm_data[i].btm_amux_chan == btm_chan) {
				*btm_chan_idx = i;
				chan_found = true;
			}
		}
	} else {
		for (i = 0; i < chip->max_channels_available; i++) {
			if (chip->sensor[i].btm_channel_num == btm_chan) {
				*btm_chan_idx = i;
				chan_found = true;
				break;
			}
		}
	}

	if (!chan_found)
		return -EINVAL;
	return rc;
}

static int32_t qpnp_adc_tm_check_revision(struct qpnp_adc_tm_chip *chip,
							uint32_t btm_chan_num)
{
	u8 rev, perph_subtype;
	int rc = 0;

	rc = qpnp_adc_tm_read_reg(chip, QPNP_REVISION3, &rev, 1);
	if (rc) {
		pr_err("adc-tm revision read failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_PERPH_SUBTYPE, &perph_subtype, 1);
	if (rc) {
		pr_err("adc-tm perph_subtype read failed\n");
		return rc;
	}

	if (perph_subtype == QPNP_PERPH_TYPE2) {
		if ((rev < QPNP_REVISION_EIGHT_CHANNEL_SUPPORT) &&
			(btm_chan_num > QPNP_ADC_TM_M4_ADC_CH_SEL_CTL)) {
			pr_debug("Version does not support more than 5 channels\n");
			return -EINVAL;
		}
	}

	if (perph_subtype == QPNP_PERPH_SUBTYPE_TWO_CHANNEL_SUPPORT) {
		if (btm_chan_num > QPNP_ADC_TM_M1_ADC_CH_SEL_CTL) {
			pr_debug("Version does not support more than 2 channels\n");
			return -EINVAL;
		}
	}

	return rc;
}

static int32_t qpnp_adc_tm_timer_interval_select(
		struct qpnp_adc_tm_chip *chip, uint32_t btm_chan,
		struct qpnp_vadc_chan_properties *chan_prop)
{
	int rc, chan_idx = 0, i = 0;
	bool chan_found = false;
	u8 meas_interval_timer2 = 0, timer_interval_store = 0;
	uint32_t btm_chan_idx = 0;
	bool is_pmic_5 = chip->adc->adc_prop->is_pmic_5;

	while (i < chip->max_channels_available) {
		if (chip->sensor[i].btm_channel_num == btm_chan) {
			chan_idx = i;
			chan_found = true;
			i++;
		} else
			i++;
	}

	if (!chan_found) {
		pr_err("Channel not found\n");
		return -EINVAL;
	}

	switch (chip->sensor[chan_idx].timer_select) {
	case ADC_MEAS_TIMER_SELECT1:
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_write_reg(chip,
				QPNP_ADC_TM_MEAS_INTERVAL_CTL,
				chip->sensor[chan_idx].meas_interval, 1);
		else {
			if (!is_pmic_5)
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL,
					chip->sensor[chan_idx].meas_interval,
					1);
			else
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL_PM5,
					chip->sensor[chan_idx].meas_interval,
					1);
		}
		if (rc < 0) {
			pr_err("timer1 configure failed\n");
			return rc;
		}
		break;
	case ADC_MEAS_TIMER_SELECT2:
		/* Thermal channels uses timer2, default to 1 second */
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_read_reg(chip,
				QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				&meas_interval_timer2, 1);
		else {
			if (!is_pmic_5)
				rc = qpnp_adc_tm_read_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2,
					&meas_interval_timer2, 1);
			else
				rc = qpnp_adc_tm_read_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2_PM5,
					&meas_interval_timer2, 1);
		}
		if (rc < 0) {
			pr_err("timer2 configure read failed\n");
			return rc;
		}
		timer_interval_store = chip->sensor[chan_idx].meas_interval;
		timer_interval_store <<= QPNP_ADC_TM_MEAS_INTERVAL_CTL2_SHIFT;
		timer_interval_store &= QPNP_ADC_TM_MEAS_INTERVAL_CTL2_MASK;
		meas_interval_timer2 |= timer_interval_store;
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_write_reg(chip,
				QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				meas_interval_timer2, 1);
		else {
			if (!is_pmic_5)
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2,
					meas_interval_timer2, 1);
			else
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2_PM5,
					meas_interval_timer2, 1);
		}
		if (rc < 0) {
			pr_err("timer2 configure failed\n");
			return rc;
		}
	break;
	case ADC_MEAS_TIMER_SELECT3:
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_read_reg(chip,
				QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				&meas_interval_timer2, 1);
		else {
			if (!is_pmic_5)
				rc = qpnp_adc_tm_read_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2,
					&meas_interval_timer2, 1);
			else
				rc = qpnp_adc_tm_read_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2_PM5,
					&meas_interval_timer2, 1);
		}
		if (rc < 0) {
			pr_err("timer3 read failed\n");
			return rc;
		}
		timer_interval_store = chip->sensor[chan_idx].meas_interval;
		timer_interval_store &= QPNP_ADC_TM_MEAS_INTERVAL_CTL3_MASK;
		meas_interval_timer2 |= timer_interval_store;
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_write_reg(chip,
				QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				meas_interval_timer2, 1);
		else {
			if (!is_pmic_5)
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2,
					meas_interval_timer2, 1);
			else
				rc = qpnp_adc_tm_write_reg(chip,
					QPNP_BTM_MEAS_INTERVAL_CTL2_PM5,
					meas_interval_timer2, 1);
		}
		if (rc < 0) {
			pr_err("timer3 configure failed\n");
			return rc;
		}
	break;
	default:
		pr_err("Invalid timer selection\n");
		return -EINVAL;
	}

	/* Select the timer to use for the corresponding channel */
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}
	if (!chip->adc_tm_hc)
		rc = qpnp_adc_tm_write_reg(chip,
			adc_tm_data[btm_chan_idx].meas_interval_ctl,
				chip->sensor[chan_idx].timer_select, 1);
	else
		rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_MEAS_INTERVAL_CTL(btm_chan_idx),
			chip->sensor[chan_idx].timer_select, 1);
	if (rc < 0) {
		pr_err("TM channel timer configure failed\n");
		return rc;
	}

	pr_debug("timer select:%d, timer_value_within_select:%d, channel:%x\n",
			chip->sensor[chan_idx].timer_select,
			chip->sensor[chan_idx].meas_interval,
			btm_chan);

	return rc;
}

static int32_t qpnp_adc_tm_add_to_list(struct qpnp_adc_tm_chip *chip,
				uint32_t dt_index,
				struct qpnp_adc_tm_btm_param *param,
				struct qpnp_vadc_chan_properties *chan_prop)
{
	struct qpnp_adc_thr_client_info *client_info = NULL;
	bool client_info_exists = false;

	list_for_each_entry(client_info,
			&chip->sensor[dt_index].thr_list, list) {
		if (client_info->btm_param == param) {
			client_info->low_thr_requested = chan_prop->low_thr;
			client_info->high_thr_requested = chan_prop->high_thr;
			client_info->state_requested = param->state_request;
			client_info->state_req_copy = param->state_request;
			client_info->notify_low_thr = false;
			client_info->notify_high_thr = false;
			client_info_exists = true;
			pr_debug("client found\n");
		}
	}

	if (!client_info_exists) {
		client_info = devm_kzalloc(chip->dev,
			sizeof(struct qpnp_adc_thr_client_info), GFP_KERNEL);
		if (!client_info)
			return -ENOMEM;

		pr_debug("new client\n");
		client_info->btm_param = param;
		client_info->low_thr_requested = chan_prop->low_thr;
		client_info->high_thr_requested = chan_prop->high_thr;
		client_info->state_requested = param->state_request;
		client_info->state_req_copy = param->state_request;

		list_add_tail(&client_info->list,
					&chip->sensor[dt_index].thr_list);
	}

	return 0;
}

static int32_t qpnp_adc_tm_reg_update(struct qpnp_adc_tm_chip *chip,
		uint16_t addr, u8 mask, bool state)
{
	u8 reg_value = 0;
	int rc = 0;

	rc = qpnp_adc_tm_read_reg(chip, addr, &reg_value, 1);
	if (rc < 0) {
		pr_err("read failed for addr:0x%x\n", addr);
		return rc;
	}

	reg_value = reg_value & ~mask;
	if (state)
		reg_value |= mask;

	pr_debug("state:%d, reg:0x%x with bits:0x%x and mask:0x%x\n",
					state, addr, reg_value, ~mask);
	rc = qpnp_adc_tm_write_reg(chip, addr, reg_value, 1);
	if (rc < 0) {
		pr_err("write failed for addr:%x\n", addr);
		return rc;
	}

	return rc;
}

static int32_t qpnp_adc_tm_read_thr_value(struct qpnp_adc_tm_chip *chip,
			uint32_t btm_chan)
{
	int rc = 0;
	u8 data_lsb = 0, data_msb = 0;
	uint32_t btm_chan_idx = 0;
	int32_t low_thr = 0, high_thr = 0;

	if (!chip->adc_tm_hc) {
		pr_err("Not applicable for VADC HC peripheral\n");
		return -EINVAL;
	}

	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(chip,
			adc_tm_data[btm_chan_idx].low_thr_lsb_addr,
			&data_lsb, 1);
	if (rc < 0) {
		pr_err("low threshold lsb setting failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(chip,
		adc_tm_data[btm_chan_idx].low_thr_msb_addr,
		&data_msb, 1);
	if (rc < 0) {
		pr_err("low threshold msb setting failed\n");
		return rc;
	}

	low_thr = (data_msb << 8) | data_lsb;

	rc = qpnp_adc_tm_read_reg(chip,
		adc_tm_data[btm_chan_idx].high_thr_lsb_addr,
		&data_lsb, 1);
	if (rc < 0) {
		pr_err("high threshold lsb setting failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(chip,
		adc_tm_data[btm_chan_idx].high_thr_msb_addr,
		&data_msb, 1);
	if (rc < 0) {
		pr_err("high threshold msb setting failed\n");
		return rc;
	}

	high_thr = (data_msb << 8) | data_lsb;

	pr_debug("configured thresholds high:0x%x and low:0x%x\n",
		high_thr, low_thr);

	return rc;
}



static int32_t qpnp_adc_tm_thr_update(struct qpnp_adc_tm_chip *chip,
			uint32_t btm_chan, int32_t high_thr, int32_t low_thr)
{
	int rc = 0;
	uint32_t btm_chan_idx = 0;

	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_write_reg(chip,
			adc_tm_data[btm_chan_idx].low_thr_lsb_addr,
			QPNP_ADC_TM_THR_LSB_MASK(low_thr), 1);
		if (rc < 0) {
			pr_err("low threshold lsb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			adc_tm_data[btm_chan_idx].low_thr_msb_addr,
			QPNP_ADC_TM_THR_MSB_MASK(low_thr), 1);
		if (rc < 0) {
			pr_err("low threshold msb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			adc_tm_data[btm_chan_idx].high_thr_lsb_addr,
			QPNP_ADC_TM_THR_LSB_MASK(high_thr), 1);
		if (rc < 0) {
			pr_err("high threshold lsb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			adc_tm_data[btm_chan_idx].high_thr_msb_addr,
			QPNP_ADC_TM_THR_MSB_MASK(high_thr), 1);
		if (rc < 0)
			pr_err("high threshold msb setting failed\n");
	} else {
		rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_LOW_THR0(btm_chan_idx),
			QPNP_ADC_TM_THR_LSB_MASK(low_thr), 1);
		if (rc < 0) {
			pr_err("low threshold lsb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_LOW_THR1(btm_chan_idx),
			QPNP_ADC_TM_THR_MSB_MASK(low_thr), 1);
		if (rc < 0) {
			pr_err("low threshold msb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_HIGH_THR0(btm_chan_idx),
			QPNP_ADC_TM_THR_LSB_MASK(high_thr), 1);
		if (rc < 0) {
			pr_err("high threshold lsb setting failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_HIGH_THR1(btm_chan_idx),
			QPNP_ADC_TM_THR_MSB_MASK(high_thr), 1);
		if (rc < 0)
			pr_err("high threshold msb setting failed\n");

	}

	pr_debug("client requested high:%d and low:%d\n",
			high_thr, low_thr);

	return rc;
}

static int32_t qpnp_adc_tm_manage_thresholds(struct qpnp_adc_tm_chip *chip,
		uint32_t dt_index, uint32_t btm_chan)
{
	struct qpnp_adc_thr_client_info *client_info = NULL;
	struct list_head *thr_list;
	int high_thr = 0, low_thr = 0, rc = 0;


	/*
	 * high_thr/low_thr starting point and reset the high_thr_set and
	 * low_thr_set back to reset since the thresholds will be
	 * recomputed.
	 */
	list_for_each(thr_list,
			&chip->sensor[dt_index].thr_list) {
		client_info = list_entry(thr_list,
					struct qpnp_adc_thr_client_info, list);
		high_thr = client_info->high_thr_requested;
		low_thr = client_info->low_thr_requested;
		client_info->high_thr_set = false;
		client_info->low_thr_set = false;
	}

	pr_debug("init threshold is high:%d and low:%d\n", high_thr, low_thr);

	/* Find the min of high_thr and max of low_thr */
	list_for_each(thr_list,
			&chip->sensor[dt_index].thr_list) {
		client_info = list_entry(thr_list,
					struct qpnp_adc_thr_client_info, list);
		if ((client_info->state_req_copy == ADC_TM_HIGH_THR_ENABLE) ||
			(client_info->state_req_copy ==
						ADC_TM_HIGH_LOW_THR_ENABLE))
			if (client_info->high_thr_requested < high_thr)
				high_thr = client_info->high_thr_requested;

		if ((client_info->state_req_copy == ADC_TM_LOW_THR_ENABLE) ||
			(client_info->state_req_copy ==
						ADC_TM_HIGH_LOW_THR_ENABLE))
			if (client_info->low_thr_requested > low_thr)
				low_thr = client_info->low_thr_requested;

		pr_debug("threshold compared is high:%d and low:%d\n",
				client_info->high_thr_requested,
				client_info->low_thr_requested);
		pr_debug("current threshold is high:%d and low:%d\n",
							high_thr, low_thr);
	}

	/* Check which of the high_thr and low_thr got set */
	list_for_each(thr_list,
			&chip->sensor[dt_index].thr_list) {
		client_info = list_entry(thr_list,
					struct qpnp_adc_thr_client_info, list);
		if ((client_info->state_req_copy == ADC_TM_HIGH_THR_ENABLE) ||
			(client_info->state_req_copy ==
						ADC_TM_HIGH_LOW_THR_ENABLE))
			if (high_thr == client_info->high_thr_requested)
				client_info->high_thr_set = true;

		if ((client_info->state_req_copy == ADC_TM_LOW_THR_ENABLE) ||
			(client_info->state_req_copy ==
						ADC_TM_HIGH_LOW_THR_ENABLE))
			if (low_thr == client_info->low_thr_requested)
				client_info->low_thr_set = true;
	}

	rc = qpnp_adc_tm_thr_update(chip, btm_chan, high_thr, low_thr);
	if (rc < 0)
		pr_err("setting chan:%d threshold failed\n", btm_chan);

	pr_debug("threshold written is high:%d and low:%d\n",
							high_thr, low_thr);

	return 0;
}

static int32_t qpnp_adc_tm_channel_configure(struct qpnp_adc_tm_chip *chip,
			uint32_t btm_chan,
			struct qpnp_vadc_chan_properties *chan_prop,
			uint32_t amux_channel)
{
	int rc = 0, i = 0, chan_idx = 0;
	bool chan_found = false, high_thr_set = false, low_thr_set = false;
	u8 sensor_mask = 0;
	struct qpnp_adc_thr_client_info *client_info = NULL;
	uint32_t btm_chan_idx = 0;

	while (i < chip->max_channels_available) {
		if (chip->sensor[i].btm_channel_num == btm_chan) {
			chan_idx = i;
			chan_found = true;
			i++;
		} else
			i++;
	}

	if (!chan_found) {
		pr_err("Channel not found\n");
		return -EINVAL;
	}

	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	sensor_mask = 1 << chan_idx;
	if (!chip->sensor[chan_idx].thermal_node) {
		/* Update low and high notification thresholds */
		rc = qpnp_adc_tm_manage_thresholds(chip, chan_idx,
				btm_chan);
		if (rc < 0) {
			pr_err("setting chan:%d threshold failed\n", btm_chan);
			return rc;
		}

		list_for_each_entry(client_info,
				&chip->sensor[chan_idx].thr_list, list) {
			if (client_info->high_thr_set == true)
				high_thr_set = true;
			if (client_info->low_thr_set == true)
				low_thr_set = true;
		}

		if (low_thr_set) {
			pr_debug("low sensor mask:%x with state:%d\n",
					sensor_mask, chan_prop->state_request);
			/* Enable low threshold's interrupt */
			if (!chip->adc_tm_hc)
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_ADC_TM_LOW_THR_INT_EN,
					sensor_mask, true);
			else
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_BTM_Mn_EN(btm_chan_idx),
					QPNP_BTM_Mn_LOW_THR_INT_EN, true);
			if (rc < 0) {
				pr_err("low thr enable err:%d\n", btm_chan);
				return rc;
			}
		}

		if (high_thr_set) {
			/* Enable high threshold's interrupt */
			pr_debug("high sensor mask:%x\n", sensor_mask);
			if (!chip->adc_tm_hc)
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_ADC_TM_HIGH_THR_INT_EN,
					sensor_mask, true);
			else
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_BTM_Mn_EN(btm_chan_idx),
					QPNP_BTM_Mn_HIGH_THR_INT_EN, true);
			if (rc < 0) {
				pr_err("high thr enable err:%d\n", btm_chan);
				return rc;
			}
		}
	}

	/* Enable corresponding BTM channel measurement */
	if (!chip->adc_tm_hc)
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_ADC_TM_MULTI_MEAS_EN, sensor_mask, true);
	else
		rc = qpnp_adc_tm_reg_update(chip, QPNP_BTM_Mn_EN(btm_chan_idx),
			QPNP_BTM_Mn_MEAS_EN, true);
	if (rc < 0) {
		pr_err("multi measurement en failed\n");
		return rc;
	}
	return rc;
}

static int32_t qpnp_adc_tm_hc_configure(struct qpnp_adc_tm_chip *chip,
			struct qpnp_adc_amux_properties *chan_prop)
{
	u8 decimation = 0, fast_avg_ctl = 0;
	u8 buf[8];
	int rc = 0;
	uint32_t btm_chan = 0, cal_type = 0, btm_chan_idx = 0;

	/* Disable bank */
	rc = qpnp_adc_tm_disable(chip);
	if (rc)
		return rc;

	/* Decimation setup */
	decimation = chan_prop->decimation;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_BTM_HC_ADC_DIG_PARAM,
						decimation, 1);
	if (rc < 0) {
		pr_err("adc-tm digital parameter setup err\n");
		return rc;
	}

	/* Fast averaging setup/enable */
	rc = qpnp_adc_tm_read_reg(chip, QPNP_BTM_HC_FAST_AVG_CTL,
						&fast_avg_ctl, 1);
	if (rc < 0) {
		pr_err("adc-tm fast-avg enable read err\n");
		return rc;
	}
	fast_avg_ctl |= chan_prop->fast_avg_setup;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_BTM_HC_FAST_AVG_CTL,
						fast_avg_ctl, 1);
	if (rc < 0) {
		pr_err("adc-tm fast-avg enable write err\n");
		return rc;
	}

	/* Read block registers for respective BTM channel */
	btm_chan = chan_prop->chan_prop->tm_channel_select;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(chip,
			QPNP_BTM_Mn_ADC_CH_SEL_CTL(btm_chan_idx), buf, 8);
	if (rc < 0) {
		pr_err("qpnp adc configure block read failed\n");
		return rc;
	}

	/* Update ADC channel sel */
	rc = qpnp_adc_tm_write_reg(chip,
			QPNP_BTM_Mn_ADC_CH_SEL_CTL(btm_chan_idx),
				chan_prop->amux_channel, 1);
	if (rc < 0) {
		pr_err("adc-tm channel amux select failed\n");
		return rc;
	}

	/* Manage thresholds */
	rc = qpnp_adc_tm_channel_configure(chip, btm_chan,
			chan_prop->chan_prop, chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("adc-tm channel threshold configure failed\n");
		return rc;
	}

	/* Measurement interval setup */
	rc = qpnp_adc_tm_timer_interval_select(chip, btm_chan,
						chan_prop->chan_prop);
	if (rc < 0) {
		pr_err("adc-tm timer select failed\n");
		return rc;
	}

	/* Set calibration select, hw_settle delay */
	cal_type |= (chan_prop->calib_type << QPNP_BTM_CTL_CAL_SEL_MASK_SHIFT);
	buf[6] &= ~QPNP_BTM_CTL_HW_SETTLE_DELAY_MASK;
	buf[6] |= chan_prop->hw_settle_time;
	buf[6] &= ~QPNP_BTM_CTL_CAL_SEL;
	buf[6] |= cal_type;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_BTM_Mn_CTL(btm_chan_idx),
								buf[6], 1);
	if (rc < 0) {
		pr_err("adc-tm hw-settle, calib sel failed\n");
		return rc;
	}

	/* Enable bank */
	rc = qpnp_adc_tm_enable(chip);
	if (rc)
		return rc;

	/* Request conversion */
	rc = qpnp_adc_tm_write_reg(chip, QPNP_CONV_REQ, QPNP_CONV_REQ_SET, 1);
	if (rc < 0) {
		pr_err("adc-tm request conversion failed\n");
		return rc;
	}

	return 0;
}

static int32_t qpnp_adc_tm_configure(struct qpnp_adc_tm_chip *chip,
			struct qpnp_adc_amux_properties *chan_prop)
{
	u8 decimation = 0, op_cntrl = 0, mode_ctl = 0;
	int rc = 0;
	uint32_t btm_chan = 0;

	/* Set measurement in single measurement mode */
	mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
	rc = qpnp_adc_tm_mode_select(chip, mode_ctl);
	if (rc < 0) {
		pr_err("adc-tm single mode select failed\n");
		return rc;
	}

	/* Disable bank */
	rc = qpnp_adc_tm_disable(chip);
	if (rc)
		return rc;

	/* Check if a conversion is in progress */
	rc = qpnp_adc_tm_req_sts_check(chip);
	if (rc < 0) {
		pr_err("adc-tm req_sts check failed\n");
		return rc;
	}

	/* Configure AMUX channel select for the corresponding BTM channel*/
	btm_chan = chan_prop->chan_prop->tm_channel_select;
	rc = qpnp_adc_tm_write_reg(chip, btm_chan, chan_prop->amux_channel, 1);
	if (rc < 0) {
		pr_err("adc-tm channel selection err\n");
		return rc;
	}

	/* Digital parameter setup */
	decimation |= chan_prop->decimation <<
				QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT;
	rc = qpnp_adc_tm_write_reg(chip, QPNP_ADC_DIG_PARAM, decimation, 1);
	if (rc < 0) {
		pr_err("adc-tm digital parameter setup err\n");
		return rc;
	}

	/* Hardware setting time */
	rc = qpnp_adc_tm_write_reg(chip, QPNP_HW_SETTLE_DELAY,
					chan_prop->hw_settle_time, 1);
	if (rc < 0) {
		pr_err("adc-tm hw settling time setup err\n");
		return rc;
	}

	/* Fast averaging setup/enable */
	rc = qpnp_adc_tm_fast_avg_en(chip, &chan_prop->fast_avg_setup);
	if (rc < 0) {
		pr_err("adc-tm fast-avg enable err\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(chip, QPNP_FAST_AVG_CTL,
				chan_prop->fast_avg_setup, 1);
	if (rc < 0) {
		pr_err("adc-tm fast-avg setup err\n");
		return rc;
	}

	/* Measurement interval setup */
	rc = qpnp_adc_tm_timer_interval_select(chip, btm_chan,
						chan_prop->chan_prop);
	if (rc < 0) {
		pr_err("adc-tm timer select failed\n");
		return rc;
	}

	/* Channel configuration setup */
	rc = qpnp_adc_tm_channel_configure(chip, btm_chan,
			chan_prop->chan_prop, chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("adc-tm channel configure failed\n");
		return rc;
	}

	/* Recurring interval measurement enable */
	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_MEAS_INTERVAL_OP_CTL,
							&op_cntrl, 1);
	op_cntrl |= QPNP_ADC_MEAS_INTERVAL_OP;
	rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_MEAS_INTERVAL_OP_CTL,
			op_cntrl, true);
	if (rc < 0) {
		pr_err("adc-tm meas interval op configure failed\n");
		return rc;
	}

	/* Enable bank */
	rc = qpnp_adc_tm_enable(chip);
	if (rc)
		return rc;

	/* Request conversion */
	rc = qpnp_adc_tm_write_reg(chip, QPNP_CONV_REQ, QPNP_CONV_REQ_SET, 1);
	if (rc < 0) {
		pr_err("adc-tm request conversion failed\n");
		return rc;
	}

	return 0;
}

static int qpnp_adc_tm_set_mode(struct qpnp_adc_tm_sensor *adc_tm,
			      enum thermal_device_mode mode)
{
	struct qpnp_adc_tm_chip *chip = adc_tm->chip;
	int rc = 0, channel;
	u8 sensor_mask = 0, mode_ctl = 0;
	uint32_t btm_chan_idx = 0, btm_chan = 0;

	if (qpnp_adc_tm_is_valid(chip)) {
		pr_err("invalid device\n");
		return -ENODEV;
	}

	if (qpnp_adc_tm_check_revision(chip, adc_tm->btm_channel_num))
		return -EINVAL;

	mutex_lock(&chip->adc->adc_lock);

	btm_chan = adc_tm->btm_channel_num;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		goto fail;
	}

	if (mode == THERMAL_DEVICE_ENABLED) {
		chip->adc->amux_prop->amux_channel =
					adc_tm->vadc_channel_num;
		channel = adc_tm->sensor_num;
		chip->adc->amux_prop->decimation =
			chip->adc->adc_channels[channel].adc_decimation;
		chip->adc->amux_prop->hw_settle_time =
			chip->adc->adc_channels[channel].hw_settle_time;
		chip->adc->amux_prop->fast_avg_setup =
			chip->adc->adc_channels[channel].fast_avg_setup;
		chip->adc->amux_prop->mode_sel =
			ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
		chip->adc->amux_prop->chan_prop->low_thr = adc_tm->low_thr;
		chip->adc->amux_prop->chan_prop->high_thr = adc_tm->high_thr;
		chip->adc->amux_prop->chan_prop->tm_channel_select =
			adc_tm->btm_channel_num;
		chip->adc->amux_prop->calib_type =
			chip->adc->adc_channels[channel].calib_type;

		if (!chip->adc_tm_hc) {
			rc = qpnp_adc_tm_configure(chip, chip->adc->amux_prop);
			if (rc) {
				pr_err("adc-tm configure failed with %d\n", rc);
				goto fail;
			}
		} else {
			rc = qpnp_adc_tm_hc_configure(chip,
					chip->adc->amux_prop);
			if (rc) {
				pr_err("hc configure failed with %d\n", rc);
				goto fail;
			}
		}
	} else if (mode == THERMAL_DEVICE_DISABLED) {
		sensor_mask = 1 << adc_tm->sensor_num;
		if (!chip->adc_tm_hc) {
			mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
			rc = qpnp_adc_tm_mode_select(chip, mode_ctl);
			if (rc < 0) {
				pr_err("adc-tm single mode select failed\n");
				goto fail;
			}
		}

		/* Disable bank */
		rc = qpnp_adc_tm_disable(chip);
		if (rc < 0) {
			pr_err("adc-tm disable failed\n");
			goto fail;
		}

		if (!chip->adc_tm_hc) {
			/* Check if a conversion is in progress */
			rc = qpnp_adc_tm_req_sts_check(chip);
			if (rc < 0) {
				pr_err("adc-tm req_sts check failed\n");
				goto fail;
			}
			rc = qpnp_adc_tm_reg_update(chip,
					QPNP_ADC_TM_MULTI_MEAS_EN,
					sensor_mask, false);
			if (rc < 0) {
				pr_err("multi measurement update failed\n");
				goto fail;
			}
		} else {
			rc = qpnp_adc_tm_reg_update(chip,
					QPNP_BTM_Mn_EN(btm_chan_idx),
					QPNP_BTM_Mn_MEAS_EN, false);
			if (rc < 0) {
				pr_err("multi measurement disable failed\n");
				goto fail;
			}
		}

		rc = qpnp_adc_tm_enable_if_channel_meas(chip);
		if (rc < 0) {
			pr_err("re-enabling measurement failed\n");
			goto fail;
		}
	}

	adc_tm->mode = mode;

fail:
	mutex_unlock(&chip->adc->adc_lock);

	return 0;
}

static int qpnp_adc_tm_activate_trip_type(struct qpnp_adc_tm_sensor *adc_tm,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct qpnp_adc_tm_chip *chip = adc_tm->chip;
	int rc = 0, sensor_mask = 0;
	u8 thr_int_en = 0;
	bool state = false;
	uint32_t btm_chan_idx = 0, btm_chan = 0;

	if (qpnp_adc_tm_is_valid(chip))
		return -ENODEV;

	if (qpnp_adc_tm_check_revision(chip, adc_tm->btm_channel_num))
		return -EINVAL;

	if (mode == THERMAL_TRIP_ACTIVATION_ENABLED)
		state = true;

	sensor_mask = 1 << adc_tm->sensor_num;

	pr_debug("Sensor number:%x with state:%d\n",
					adc_tm->sensor_num, state);

	btm_chan = adc_tm->btm_channel_num;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		/* low_thr (lower voltage) for higher temp */
		thr_int_en = adc_tm_data[btm_chan_idx].low_thr_int_chan_en;
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_reg_update(chip,
					QPNP_ADC_TM_LOW_THR_INT_EN,
					sensor_mask, state);
		else
			rc = qpnp_adc_tm_reg_update(chip,
				QPNP_BTM_Mn_EN(btm_chan_idx),
				QPNP_BTM_Mn_LOW_THR_INT_EN, state);
		if (rc)
			pr_err("channel:%x failed\n", btm_chan);
	break;
	case ADC_TM_TRIP_LOW_COOL:
		/* high_thr (higher voltage) for cooler temp */
		thr_int_en = adc_tm_data[btm_chan_idx].high_thr_int_chan_en;
		if (!chip->adc_tm_hc)
			rc = qpnp_adc_tm_reg_update(chip,
				QPNP_ADC_TM_HIGH_THR_INT_EN,
				sensor_mask, state);
		else
			rc = qpnp_adc_tm_reg_update(chip,
				QPNP_BTM_Mn_EN(btm_chan_idx),
				QPNP_BTM_Mn_HIGH_THR_INT_EN, state);
		if (rc)
			pr_err("channel:%x failed\n", btm_chan);
	break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int qpnp_adc_tm_set_trip_temp(void *data, int low_temp, int high_temp)
{
	struct qpnp_adc_tm_sensor *adc_tm = data;
	struct qpnp_adc_tm_chip *chip = adc_tm->chip;
	struct qpnp_adc_tm_config tm_config;
	u8 trip_cool_thr0, trip_cool_thr1, trip_warm_thr0, trip_warm_thr1;
	uint16_t reg_low_thr_lsb, reg_low_thr_msb;
	uint16_t reg_high_thr_lsb, reg_high_thr_msb;
	int rc = 0;
	uint32_t btm_chan = 0, btm_chan_idx = 0;

	if (qpnp_adc_tm_is_valid(chip))
		return -ENODEV;

	if (qpnp_adc_tm_check_revision(chip, adc_tm->btm_channel_num))
		return -EINVAL;

	tm_config.channel = adc_tm->vadc_channel_num;
	tm_config.high_thr_temp = tm_config.low_thr_temp = 0;
	if (high_temp != INT_MAX)
		tm_config.high_thr_temp = high_temp;
	if (low_temp != INT_MIN)
		tm_config.low_thr_temp = low_temp;

	if ((high_temp == INT_MAX) && (low_temp == INT_MIN)) {
		pr_err("No trips to set\n");
		return -EINVAL;
	}

	pr_debug("requested a high - %d and low - %d\n",
			tm_config.high_thr_temp, tm_config.low_thr_temp);
	rc = qpnp_adc_tm_scale_therm_voltage_pu2(chip->vadc_dev,
				chip->adc->adc_prop, &tm_config);
	if (rc < 0) {
		pr_err("Failed to lookup the adc-tm thresholds\n");
		return rc;
	}

	trip_warm_thr0 = ((tm_config.low_thr_voltage << 24) >> 24);
	trip_warm_thr1 = ((tm_config.low_thr_voltage << 16) >> 24);
	trip_cool_thr0 = ((tm_config.high_thr_voltage << 24) >> 24);
	trip_cool_thr1 = ((tm_config.high_thr_voltage << 16) >> 24);

	pr_debug("low_thr:0x%llx, high_thr:0x%llx\n", tm_config.low_thr_voltage,
				tm_config.high_thr_voltage);

	btm_chan = adc_tm->btm_channel_num;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	if (!chip->adc_tm_hc) {
		reg_low_thr_lsb = adc_tm_data[btm_chan_idx].low_thr_lsb_addr;
		reg_low_thr_msb = adc_tm_data[btm_chan_idx].low_thr_msb_addr;
		reg_high_thr_lsb = adc_tm_data[btm_chan_idx].high_thr_lsb_addr;
		reg_high_thr_msb = adc_tm_data[btm_chan_idx].high_thr_msb_addr;
	} else {
		reg_low_thr_lsb = QPNP_BTM_Mn_LOW_THR0(btm_chan_idx);
		reg_low_thr_msb = QPNP_BTM_Mn_LOW_THR1(btm_chan_idx);
		reg_high_thr_lsb = QPNP_BTM_Mn_HIGH_THR0(btm_chan_idx);
		reg_high_thr_msb = QPNP_BTM_Mn_HIGH_THR1(btm_chan_idx);
	}

	if (high_temp != INT_MAX) {
		rc = qpnp_adc_tm_write_reg(chip, reg_low_thr_lsb,
						trip_cool_thr0, 1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip, reg_low_thr_msb,
						trip_cool_thr1, 1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}
		adc_tm->low_thr = tm_config.high_thr_voltage;

		rc = qpnp_adc_tm_activate_trip_type(adc_tm,
				ADC_TM_TRIP_HIGH_WARM,
				THERMAL_TRIP_ACTIVATION_ENABLED);
		if (rc) {
			pr_err("adc-tm warm activation failed\n");
			return rc;
		}
	} else {
		rc = qpnp_adc_tm_activate_trip_type(adc_tm,
				ADC_TM_TRIP_HIGH_WARM,
				THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc) {
			pr_err("adc-tm warm deactivation failed\n");
			return rc;
		}
	}

	if (low_temp != INT_MIN) {
		rc = qpnp_adc_tm_write_reg(chip, reg_high_thr_lsb,
						trip_warm_thr0, 1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(chip, reg_high_thr_msb,
						trip_warm_thr1, 1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}
		adc_tm->high_thr = tm_config.low_thr_voltage;

		rc = qpnp_adc_tm_activate_trip_type(adc_tm,
				ADC_TM_TRIP_LOW_COOL,
				THERMAL_TRIP_ACTIVATION_ENABLED);
		if (rc) {
			pr_err("adc-tm cool activation failed\n");
			return rc;
		}
	} else {
		rc = qpnp_adc_tm_activate_trip_type(adc_tm,
				ADC_TM_TRIP_LOW_COOL,
				THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc) {
			pr_err("adc-tm cool deactivation failed\n");
			return rc;
		}
	}

	if ((high_temp != INT_MAX) || (low_temp != INT_MIN)) {
		rc = qpnp_adc_tm_set_mode(adc_tm, THERMAL_DEVICE_ENABLED);
		if (rc) {
			pr_err("sensor enabled failed\n");
			return rc;
		}
	} else {
		rc = qpnp_adc_tm_set_mode(adc_tm, THERMAL_DEVICE_DISABLED);
		if (rc) {
			pr_err("sensor disable failed\n");
			return rc;
		}
	}

	return 0;
}

static void notify_battery_therm(struct qpnp_adc_tm_sensor *adc_tm)
{
	struct qpnp_adc_thr_client_info *client_info = NULL;

	list_for_each_entry(client_info,
			&adc_tm->thr_list, list) {
		/* Batt therm's warm temperature translates to low voltage */
		if (client_info->notify_low_thr) {
			/* HIGH_STATE = WARM_TEMP for battery client */
			client_info->btm_param->threshold_notification(
			ADC_TM_WARM_STATE, client_info->btm_param->btm_ctx);
			client_info->notify_low_thr = false;
		}

		/* Batt therm's cool temperature translates to high voltage */
		if (client_info->notify_high_thr) {
			/* LOW_STATE = COOL_TEMP for battery client */
			client_info->btm_param->threshold_notification(
			ADC_TM_COOL_STATE, client_info->btm_param->btm_ctx);
			client_info->notify_high_thr = false;
		}
	}
}

static void notify_clients(struct qpnp_adc_tm_sensor *adc_tm)
{
	struct qpnp_adc_thr_client_info *client_info = NULL;

	list_for_each_entry(client_info,
			&adc_tm->thr_list, list) {
		/* For non batt therm clients */
		if (client_info->notify_low_thr) {
			if (client_info->btm_param->threshold_notification
								!= NULL) {
				pr_debug("notify kernel with low state\n");
				client_info->btm_param->threshold_notification(
					ADC_TM_LOW_STATE,
					client_info->btm_param->btm_ctx);
				client_info->notify_low_thr = false;
			}
		}

		if (client_info->notify_high_thr) {
			if (client_info->btm_param->threshold_notification
								!= NULL) {
				pr_debug("notify kernel with high state\n");
				client_info->btm_param->threshold_notification(
					ADC_TM_HIGH_STATE,
					client_info->btm_param->btm_ctx);
				client_info->notify_high_thr = false;
			}
		}
	}
}

static void notify_adc_tm_fn(struct work_struct *work)
{
	struct qpnp_adc_tm_sensor *adc_tm = container_of(work,
		struct qpnp_adc_tm_sensor, work);

	if (adc_tm->thermal_node) {
		pr_debug("notifying uspace client\n");
		of_thermal_handle_trip(adc_tm->tz_dev);
	} else {
		if (adc_tm->scale_type == SCALE_RBATT_THERM)
			notify_battery_therm(adc_tm);
		else
			notify_clients(adc_tm);
	}
}

static int qpnp_adc_tm_recalib_request_check(struct qpnp_adc_tm_chip *chip,
			int sensor_num, u8 status_high, u8 *notify_check)
{
	int rc = 0;
	u8 sensor_mask = 0, mode_ctl = 0;
	int32_t old_thr = 0, new_thr = 0;
	uint32_t channel, btm_chan_num, scale_type;
	struct qpnp_vadc_result result;
	struct qpnp_adc_thr_client_info *client_info = NULL;
	struct list_head *thr_list;
	bool status = false;

	if (!chip->adc_tm_recalib_check) {
		*notify_check = 1;
		return rc;
	}

	list_for_each(thr_list, &chip->sensor[sensor_num].thr_list) {
		client_info = list_entry(thr_list,
				struct qpnp_adc_thr_client_info, list);
		channel = client_info->btm_param->channel;
		btm_chan_num = chip->sensor[sensor_num].btm_channel_num;
		sensor_mask = 1 << sensor_num;

		rc = qpnp_vadc_read(chip->vadc_dev, channel, &result);
		if (rc < 0) {
			pr_err("failure to read vadc channel=%d\n",
					client_info->btm_param->channel);
			goto fail;
		}
		new_thr = result.physical;

		if (status_high)
			old_thr = client_info->btm_param->high_thr;
		else
			old_thr = client_info->btm_param->low_thr;

		if (new_thr > old_thr)
			status = (status_high) ? true : false;
		else
			status = (status_high) ? false : true;

		pr_debug(
			"recalib:sen=%d, new_thr=%d, new_thr_adc_code=0x%x, old_thr=%d status=%d valid_status=%d\n",
			sensor_num, new_thr, result.adc_code,
			old_thr, status_high, status);

		rc = qpnp_adc_tm_read_thr_value(chip, btm_chan_num);
		if (rc < 0) {
			pr_err("adc-tm thresholds read failed\n");
			goto fail;
		}

		if (status) {
			*notify_check = 1;
			pr_debug("Client can be notify\n");
			return rc;
		}

		pr_debug("Client can not be notify, restart measurement\n");
		/* Set measurement in single measurement mode */
		mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
		rc = qpnp_adc_tm_mode_select(chip, mode_ctl);
		if (rc < 0) {
			pr_err("adc-tm single mode select failed\n");
			goto fail;
		}

		/* Disable bank */
		rc = qpnp_adc_tm_disable(chip);
		if (rc < 0) {
			pr_err("adc-tm disable failed\n");
			goto fail;
		}

		/* Check if a conversion is in progress */
		rc = qpnp_adc_tm_req_sts_check(chip);
		if (rc < 0) {
			pr_err("adc-tm req_sts check failed\n");
			goto fail;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_TM_LOW_THR_INT_EN,
							sensor_mask, false);
		if (rc < 0) {
			pr_err("low threshold int write failed\n");
			goto fail;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_TM_HIGH_THR_INT_EN,
							sensor_mask, false);
		if (rc < 0) {
			pr_err("high threshold int enable failed\n");
			goto fail;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
							sensor_mask, false);
		if (rc < 0) {
			pr_err("multi measurement en failed\n");
			goto fail;
		}

		/* restart measurement */
		scale_type = chip->sensor[sensor_num].scale_type;
		chip->adc->amux_prop->amux_channel = channel;
		chip->adc->amux_prop->decimation =
			chip->adc->adc_channels[sensor_num].adc_decimation;
		chip->adc->amux_prop->hw_settle_time =
			chip->adc->adc_channels[sensor_num].hw_settle_time;
		chip->adc->amux_prop->fast_avg_setup =
			chip->adc->adc_channels[sensor_num].fast_avg_setup;
		chip->adc->amux_prop->mode_sel =
			ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
		adc_tm_rscale_fn[scale_type].chan(chip->vadc_dev,
				client_info->btm_param,
				&chip->adc->amux_prop->chan_prop->low_thr,
				&chip->adc->amux_prop->chan_prop->high_thr);
		qpnp_adc_tm_add_to_list(chip, sensor_num,
				client_info->btm_param,
				chip->adc->amux_prop->chan_prop);
		chip->adc->amux_prop->chan_prop->tm_channel_select =
				chip->sensor[sensor_num].btm_channel_num;
		chip->adc->amux_prop->chan_prop->state_request =
				client_info->btm_param->state_request;

		rc = qpnp_adc_tm_configure(chip, chip->adc->amux_prop);
		if (rc) {
			pr_err("adc-tm configure failed with %d\n", rc);
			goto fail;
		}
		*notify_check = 0;
		pr_debug("BTM channel reconfigured for measuremnt\n");
	}
fail:
	return rc;
}

static int qpnp_adc_tm_disable_rearm_high_thresholds(
			struct qpnp_adc_tm_chip *chip, int sensor_num)
{

	struct qpnp_adc_thr_client_info *client_info = NULL;
	struct list_head *thr_list;
	uint32_t btm_chan_num = 0, btm_chan_idx = 0;
	u8 sensor_mask = 0, notify_check = 0;
	int rc = 0;

	btm_chan_num = chip->sensor[sensor_num].btm_channel_num;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan_num, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	pr_debug("high:sen:%d, hs:0x%x, ls:0x%x, meas_en:0x%x\n",
		sensor_num, chip->th_info.adc_tm_high_enable,
		chip->th_info.adc_tm_low_enable,
		chip->th_info.qpnp_adc_tm_meas_en);
	if (!chip->sensor[sensor_num].thermal_node) {
		/*
		 * For non thermal registered clients such as usb_id,
		 * vbatt, pmic_therm
		 */
		sensor_mask = 1 << sensor_num;
		pr_debug("non thermal node - mask:%x\n", sensor_mask);
		if (!chip->adc_tm_hc) {
			rc = qpnp_adc_tm_recalib_request_check(chip,
					sensor_num, true, &notify_check);
			if (rc < 0 || !notify_check) {
				pr_debug("Calib recheck re-armed rc=%d\n", rc);
				chip->th_info.adc_tm_high_enable = 0;
				return rc;
			}
		} else {
			rc = qpnp_adc_tm_reg_update(chip,
				QPNP_BTM_Mn_EN(btm_chan_idx),
				QPNP_BTM_Mn_HIGH_THR_INT_EN, false);
			if (rc < 0) {
				pr_err("high threshold int update failed\n");
				return rc;
			}
		}
		} else {
		/*
		 * Uses the thermal sysfs registered device to disable
		 * the corresponding high voltage threshold which
		 * is triggered by low temp
		 */
		sensor_mask = 1 << sensor_num;
		pr_debug("thermal node with mask:%x\n", sensor_mask);
		rc = qpnp_adc_tm_activate_trip_type(
			&chip->sensor[sensor_num],
			ADC_TM_TRIP_LOW_COOL,
			THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc < 0) {
			pr_err("notify error:%d\n", sensor_num);
			return rc;
		}
	}
	list_for_each(thr_list, &chip->sensor[sensor_num].thr_list) {
		client_info = list_entry(thr_list,
				struct qpnp_adc_thr_client_info, list);
		if (client_info->high_thr_set) {
			client_info->high_thr_set = false;
			client_info->notify_high_thr = true;
			if (client_info->state_req_copy ==
					ADC_TM_HIGH_LOW_THR_ENABLE)
				client_info->state_req_copy =
						ADC_TM_LOW_THR_ENABLE;
			else
				client_info->state_req_copy =
						ADC_TM_HIGH_THR_DISABLE;
		}
	}
	qpnp_adc_tm_manage_thresholds(chip, sensor_num, btm_chan_num);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_ADC_TM_MULTI_MEAS_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("multi meas disable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_BTM_Mn_EN(sensor_num),
			QPNP_BTM_Mn_MEAS_EN, false);
		if (rc < 0) {
			pr_err("multi meas disable failed\n");
			return rc;
		}
	}

	rc = qpnp_adc_tm_enable_if_channel_meas(chip);
	if (rc < 0) {
		pr_err("re-enabling measurement failed\n");
		return rc;
	}

	queue_work(chip->sensor[sensor_num].req_wq,
		&chip->sensor[sensor_num].work);

	return rc;
}

static int qpnp_adc_tm_disable_rearm_low_thresholds(
			struct qpnp_adc_tm_chip *chip, int sensor_num)
{
	struct qpnp_adc_thr_client_info *client_info = NULL;
	struct list_head *thr_list;
	uint32_t btm_chan_num = 0, btm_chan_idx = 0;
	u8 sensor_mask = 0, notify_check = 0;
	int rc = 0;

	btm_chan_num = chip->sensor[sensor_num].btm_channel_num;
	rc = qpnp_adc_tm_get_btm_idx(chip, btm_chan_num, &btm_chan_idx);
	if (rc < 0) {
		pr_err("Invalid btm channel idx\n");
		return rc;
	}

	pr_debug("low:sen:%d, hs:0x%x, ls:0x%x, meas_en:0x%x\n",
		sensor_num, chip->th_info.adc_tm_high_enable,
		chip->th_info.adc_tm_low_enable,
		chip->th_info.qpnp_adc_tm_meas_en);
	if (!chip->sensor[sensor_num].thermal_node) {
		/*
		 * For non thermal registered clients such as usb_id,
		 * vbatt, pmic_therm
		 */
		sensor_mask = 1 << sensor_num;
		pr_debug("non thermal node - mask:%x\n", sensor_mask);
		if (!chip->adc_tm_hc) {
			rc = qpnp_adc_tm_recalib_request_check(chip,
					sensor_num, false, &notify_check);
			if (rc < 0 || !notify_check) {
				pr_debug("Calib recheck re-armed rc=%d\n", rc);
				chip->th_info.adc_tm_low_enable = 0;
				return rc;
			}
		} else {
			rc = qpnp_adc_tm_reg_update(chip,
				QPNP_BTM_Mn_EN(btm_chan_idx),
				QPNP_BTM_Mn_LOW_THR_INT_EN, false);
			if (rc < 0) {
				pr_err("low threshold int update failed\n");
				return rc;
			}
		}
	} else {
		/*
		 * Uses the thermal sysfs registered device to disable
		 * the corresponding high voltage threshold which
		 * is triggered by low temp
		 */
		sensor_mask = 1 << sensor_num;
		pr_debug("thermal node with mask:%x\n", sensor_mask);
		rc = qpnp_adc_tm_activate_trip_type(
			&chip->sensor[sensor_num],
			ADC_TM_TRIP_HIGH_WARM,
			THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc < 0) {
			pr_err("notify error:%d\n", sensor_num);
			return rc;
		}
	}
	list_for_each(thr_list, &chip->sensor[sensor_num].thr_list) {
		client_info = list_entry(thr_list,
				struct qpnp_adc_thr_client_info, list);
		if (client_info->low_thr_set) {
			client_info->low_thr_set = false;
			client_info->notify_low_thr = true;
			if (client_info->state_req_copy ==
					ADC_TM_HIGH_LOW_THR_ENABLE)
				client_info->state_req_copy =
						ADC_TM_HIGH_THR_ENABLE;
			else
				client_info->state_req_copy =
						ADC_TM_LOW_THR_DISABLE;
		}
	}
	qpnp_adc_tm_manage_thresholds(chip, sensor_num, btm_chan_num);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_ADC_TM_MULTI_MEAS_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("multi meas disable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_BTM_Mn_EN(sensor_num),
			QPNP_BTM_Mn_MEAS_EN, false);
		if (rc < 0) {
			pr_err("multi meas disable failed\n");
			return rc;
		}
	}

	rc = qpnp_adc_tm_enable_if_channel_meas(chip);
	if (rc < 0) {
		pr_err("re-enabling measurement failed\n");
		return rc;
	}

	queue_work(chip->sensor[sensor_num].req_wq,
				&chip->sensor[sensor_num].work);

	return rc;
}

static int qpnp_adc_tm_read_status(struct qpnp_adc_tm_chip *chip)
{
	int rc = 0, sensor_notify_num = 0, i = 0, sensor_num = 0;
	unsigned long flags;

	if (qpnp_adc_tm_is_valid(chip))
		return -ENODEV;

	mutex_lock(&chip->adc->adc_lock);

	rc = qpnp_adc_tm_req_sts_check(chip);
	if (rc) {
		pr_err("adc-tm-tm req sts check failed with %d\n", rc);
		goto fail;
	}

	if (chip->th_info.adc_tm_high_enable) {
		spin_lock_irqsave(&chip->th_info.adc_tm_high_lock, flags);
		sensor_notify_num = chip->th_info.adc_tm_high_enable;
		chip->th_info.adc_tm_high_enable = 0;
		spin_unlock_irqrestore(&chip->th_info.adc_tm_high_lock, flags);
		while (i < chip->max_channels_available) {
			if ((sensor_notify_num & 0x1) == 1) {
				sensor_num = i;
				rc = qpnp_adc_tm_disable_rearm_high_thresholds(
						chip, sensor_num);
				if (rc < 0) {
					pr_err("rearm threshold failed\n");
					goto fail;
				}
			}
			sensor_notify_num >>= 1;
			i++;
		}
	}

	if (chip->th_info.adc_tm_low_enable) {
		spin_lock_irqsave(&chip->th_info.adc_tm_low_lock, flags);
		sensor_notify_num = chip->th_info.adc_tm_low_enable;
		chip->th_info.adc_tm_low_enable = 0;
		spin_unlock_irqrestore(&chip->th_info.adc_tm_low_lock, flags);
		i = 0;
		while (i < chip->max_channels_available) {
			if ((sensor_notify_num & 0x1) == 1) {
				sensor_num = i;
				rc = qpnp_adc_tm_disable_rearm_low_thresholds(
						chip, sensor_num);
				if (rc < 0) {
					pr_err("rearm threshold failed\n");
					goto fail;
				}
			}
			sensor_notify_num >>= 1;
			i++;
		}
	}

fail:
	mutex_unlock(&chip->adc->adc_lock);

	return rc;
}

static int qpnp_adc_tm_hc_read_status(struct qpnp_adc_tm_chip *chip)
{
	int rc = 0, sensor_num = 0;

	if (qpnp_adc_tm_is_valid(chip))
		return -ENODEV;

	pr_debug("%s\n", __func__);

	mutex_lock(&chip->adc->adc_lock);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_req_sts_check(chip);
		if (rc) {
			pr_err("adc-tm-tm req sts check failed with %d\n", rc);
			goto fail;
		}
	}
	while (sensor_num < chip->max_channels_available) {
		if (chip->sensor[sensor_num].high_thr_triggered) {
			rc = qpnp_adc_tm_disable_rearm_high_thresholds(
					chip, sensor_num);
			if (rc) {
				pr_err("rearm threshold failed\n");
				goto fail;
			}
			chip->sensor[sensor_num].high_thr_triggered = false;
		}
		sensor_num++;
	}

	sensor_num = 0;
	while (sensor_num < chip->max_channels_available) {
		if (chip->sensor[sensor_num].low_thr_triggered) {
			rc = qpnp_adc_tm_disable_rearm_low_thresholds(
					chip, sensor_num);
			if (rc) {
				pr_err("rearm threshold failed\n");
				goto fail;
			}
			chip->sensor[sensor_num].low_thr_triggered = false;
		}
		sensor_num++;
	}

fail:
	mutex_unlock(&chip->adc->adc_lock);

	return rc;
}

static void qpnp_adc_tm_high_thr_work(struct work_struct *work)
{
	struct qpnp_adc_tm_chip *chip = container_of(work,
			struct qpnp_adc_tm_chip, trigger_high_thr_work);
	int rc;

	/* disable the vote if applicable */
	if (chip->adc_vote_enable && chip->adc->hkadc_ldo &&
					chip->adc->hkadc_ldo_ok) {
		qpnp_adc_disable_voltage(chip->adc);
		chip->adc_vote_enable = false;
	}

	pr_debug("thr:0x%x\n", chip->th_info.adc_tm_high_enable);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_read_status(chip);
		if (rc < 0)
			pr_err("adc-tm high thr work failed\n");
	} else {
		rc = qpnp_adc_tm_hc_read_status(chip);
		if (rc < 0)
			pr_err("adc-tm-hc high thr work failed\n");
	}
}

static irqreturn_t qpnp_adc_tm_high_thr_isr(int irq, void *data)
{
	struct qpnp_adc_tm_chip *chip = data;
	u8 mode_ctl = 0, status1 = 0, sensor_mask = 0;
	int rc = 0, sensor_notify_num = 0, i = 0, sensor_num = 0;

	mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
	/* Set measurement in single measurement mode */
	qpnp_adc_tm_mode_select(chip, mode_ctl);

	qpnp_adc_tm_disable(chip);

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS1, &status1, 1);
	if (rc) {
		pr_err("adc-tm read status1 failed\n");
		return IRQ_HANDLED;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS_HIGH,
					&chip->th_info.status_high, 1);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_HIGH_THR_INT_EN,
				&chip->th_info.adc_tm_high_thr_set, 1);
	if (rc) {
		pr_err("adc-tm-tm read high thr failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	/* Check which interrupt threshold is lower and measure against the
	 * enabled channel
	 */
	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
				&chip->th_info.qpnp_adc_tm_meas_en, 1);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	chip->th_info.adc_tm_high_enable = chip->th_info.qpnp_adc_tm_meas_en &
						chip->th_info.status_high;
	chip->th_info.adc_tm_high_enable &= chip->th_info.adc_tm_high_thr_set;

	sensor_notify_num = chip->th_info.adc_tm_high_enable;
	while (i < chip->max_channels_available) {
		if ((sensor_notify_num & 0x1) == 1)
			sensor_num = i;
		sensor_notify_num >>= 1;
		i++;
	}

	if (!chip->sensor[sensor_num].thermal_node) {
		sensor_mask = 1 << sensor_num;
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_ADC_TM_HIGH_THR_INT_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("high threshold int read failed\n");
			return IRQ_HANDLED;
		}
	} else {
		/*
		 * Uses the thermal sysfs registered device to disable
		 * the corresponding high voltage threshold which
		 * is triggered by low temp
		 */
		pr_debug("thermal node with mask:%x\n", sensor_mask);
		rc = qpnp_adc_tm_activate_trip_type(
			&chip->sensor[sensor_num],
			ADC_TM_TRIP_LOW_COOL,
			THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc < 0) {
			pr_err("notify error:%d\n", sensor_num);
			return IRQ_HANDLED;
		}
	}

	queue_work(chip->high_thr_wq, &chip->trigger_high_thr_work);

	return IRQ_HANDLED;
}

static void qpnp_adc_tm_low_thr_work(struct work_struct *work)
{
	struct qpnp_adc_tm_chip *chip = container_of(work,
			struct qpnp_adc_tm_chip, trigger_low_thr_work);
	int rc;

	/* disable the vote if applicable */
	if (chip->adc_vote_enable && chip->adc->hkadc_ldo &&
					chip->adc->hkadc_ldo_ok) {
		qpnp_adc_disable_voltage(chip->adc);
		chip->adc_vote_enable = false;
	}

	pr_debug("thr:0x%x\n", chip->th_info.adc_tm_low_enable);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_read_status(chip);
		if (rc < 0)
			pr_err("adc-tm low thr work failed\n");
	} else {
		rc = qpnp_adc_tm_hc_read_status(chip);
		if (rc < 0)
			pr_err("adc-tm-hc low thr work failed\n");
	}
}

static irqreturn_t qpnp_adc_tm_low_thr_isr(int irq, void *data)
{
	struct qpnp_adc_tm_chip *chip = data;
	u8 mode_ctl = 0, status1 = 0, sensor_mask = 0;
	int rc = 0, sensor_notify_num = 0, i = 0, sensor_num = 0;

	mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
	/* Set measurement in single measurement mode */
	qpnp_adc_tm_mode_select(chip, mode_ctl);

	qpnp_adc_tm_disable(chip);

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS1, &status1, 1);
	if (rc) {
		pr_err("adc-tm read status1 failed\n");
		return IRQ_HANDLED;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS_LOW,
					&chip->th_info.status_low, 1);
	if (rc) {
		pr_err("adc-tm-tm read status low failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_LOW_THR_INT_EN,
				&chip->th_info.adc_tm_low_thr_set, 1);
	if (rc) {
		pr_err("adc-tm-tm read low thr failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
				&chip->th_info.qpnp_adc_tm_meas_en, 1);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	chip->th_info.adc_tm_low_enable = chip->th_info.qpnp_adc_tm_meas_en &
					chip->th_info.status_low;
	chip->th_info.adc_tm_low_enable &= chip->th_info.adc_tm_low_thr_set;

	sensor_notify_num = chip->th_info.adc_tm_low_enable;
	while (i < chip->max_channels_available) {
		if ((sensor_notify_num & 0x1) == 1)
			sensor_num = i;
		sensor_notify_num >>= 1;
		i++;
	}

	if (!chip->sensor[sensor_num].thermal_node) {
		sensor_mask = 1 << sensor_num;
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_ADC_TM_LOW_THR_INT_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("low threshold int read failed\n");
			return IRQ_HANDLED;
		}
	} else {
		/*
		 * Uses the thermal sysfs registered device to disable
		 * the corresponding low voltage threshold which
		 * is triggered by high temp
		 */
		pr_debug("thermal node with mask:%x\n", sensor_mask);
		rc = qpnp_adc_tm_activate_trip_type(
			&chip->sensor[sensor_num],
			ADC_TM_TRIP_HIGH_WARM,
			THERMAL_TRIP_ACTIVATION_DISABLED);
		if (rc < 0) {
			pr_err("notify error:%d\n", sensor_num);
			return IRQ_HANDLED;
		}
	}

	queue_work(chip->low_thr_wq, &chip->trigger_low_thr_work);

	return IRQ_HANDLED;
}

static int qpnp_adc_tm_rc_check_sensor_trip(struct qpnp_adc_tm_chip *chip,
			u8 status_low, u8 status_high, int i,
			int *sensor_low_notify_num, int *sensor_high_notify_num)
{
	int rc = 0;
	u8 ctl = 0, sensor_mask = 0;

	if (((status_low & 0x1) == 1) || ((status_high & 0x1) == 1)) {
		rc = qpnp_adc_tm_read_reg(chip,
					QPNP_BTM_Mn_EN(i), &ctl, 1);
		if (rc) {
			pr_err("ctl read failed with %d\n", rc);
			return IRQ_HANDLED;
		}

		if ((status_low & 0x1) && (ctl & QPNP_BTM_Mn_MEAS_EN)
			&& (ctl & QPNP_BTM_Mn_LOW_THR_INT_EN)) {
			/* Mask the corresponding low threshold interrupt en */
			if (!chip->sensor[i].thermal_node) {
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_BTM_Mn_EN(i),
					QPNP_BTM_Mn_LOW_THR_INT_EN, false);
				if (rc < 0) {
					pr_err("low thr_int en failed\n");
					return IRQ_HANDLED;
				}
			} else {
			/*
			 * Uses the thermal sysfs registered device to disable
			 * the corresponding low voltage threshold which
			 * is triggered by high temp
			 */
			pr_debug("thermal node with mask:%x\n", sensor_mask);
				rc = qpnp_adc_tm_activate_trip_type(
					&chip->sensor[i],
					ADC_TM_TRIP_HIGH_WARM,
					THERMAL_TRIP_ACTIVATION_DISABLED);
				if (rc < 0) {
					pr_err("notify error:%d\n", i);
					return IRQ_HANDLED;
				}
			}
			*sensor_low_notify_num |= (status_low & 0x1);
			chip->sensor[i].low_thr_triggered = true;
		}

		if ((status_high & 0x1) && (ctl & QPNP_BTM_Mn_MEAS_EN) &&
					(ctl & QPNP_BTM_Mn_HIGH_THR_INT_EN)) {
			/* Mask the corresponding high threshold interrupt en */
			if (!chip->sensor[i].thermal_node) {
				rc = qpnp_adc_tm_reg_update(chip,
					QPNP_BTM_Mn_EN(i),
					QPNP_BTM_Mn_HIGH_THR_INT_EN, false);
				if (rc < 0) {
					pr_err("high thr_int en failed\n");
					return IRQ_HANDLED;
				}
			} else {
			/*
			 * Uses the thermal sysfs registered device to disable
			 * the corresponding high voltage threshold which
			 * is triggered by low temp
			 */
				pr_debug("thermal node with mask:%x\n", i);
				rc = qpnp_adc_tm_activate_trip_type(
					&chip->sensor[i],
					ADC_TM_TRIP_LOW_COOL,
					THERMAL_TRIP_ACTIVATION_DISABLED);
				if (rc < 0) {
					pr_err("notify error:%d\n", i);
					return IRQ_HANDLED;
				}
			}
			*sensor_high_notify_num |= (status_high & 0x1);
			chip->sensor[i].high_thr_triggered = true;
		}
	}

	return rc;
}

static irqreturn_t qpnp_adc_tm_rc_thr_isr(int irq, void *data)
{
	struct qpnp_adc_tm_chip *chip = data;
	u8 status_low = 0, status_high = 0;
	int rc = 0, sensor_low_notify_num = 0, i = 0;
	int sensor_high_notify_num = 0;

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS_LOW,
						&status_low, 1);
	if (rc) {
		pr_err("adc-tm-tm read status low failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	if (status_low)
		chip->th_info.adc_tm_low_enable = status_low;

	rc = qpnp_adc_tm_read_reg(chip, QPNP_ADC_TM_STATUS_HIGH,
							&status_high, 1);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		return IRQ_HANDLED;
	}

	if (status_high)
		chip->th_info.adc_tm_high_enable = status_high;

	while (i < chip->max_channels_available) {
		rc = qpnp_adc_tm_rc_check_sensor_trip(chip,
				status_low, status_high, i,
				&sensor_low_notify_num,
				&sensor_high_notify_num);
		if (rc) {
			pr_err("Sensor trip read failed\n");
			return IRQ_HANDLED;
		}
		status_low >>= 1;
		status_high >>= 1;
		i++;
	}

	if (sensor_low_notify_num) {
		pm_wakeup_event(chip->dev,
				QPNP_ADC_WAKEUP_SRC_TIMEOUT_MS);
		queue_work(chip->low_thr_wq, &chip->trigger_low_thr_work);
	}

	if (sensor_high_notify_num) {
		pm_wakeup_event(chip->dev,
				QPNP_ADC_WAKEUP_SRC_TIMEOUT_MS);
		queue_work(chip->high_thr_wq,
				&chip->trigger_high_thr_work);
	}

	return IRQ_HANDLED;
}

static int qpnp_adc_read_temp(void *data, int *temp)
{
	struct qpnp_adc_tm_sensor *adc_tm_sensor = data;
	struct qpnp_adc_tm_chip *chip = adc_tm_sensor->chip;
	struct qpnp_vadc_result result;
	int rc = 0;

	rc = qpnp_vadc_read(chip->vadc_dev,
				adc_tm_sensor->vadc_channel_num, &result);
	if (rc)
		return rc;

	*temp = result.physical;

	return rc;
}

static struct thermal_zone_of_device_ops qpnp_adc_tm_thermal_ops = {
	.get_temp = qpnp_adc_read_temp,
	.set_trips = qpnp_adc_tm_set_trip_temp,
};

int32_t qpnp_adc_tm_channel_measure(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param)
{
	uint32_t channel, amux_prescaling, dt_index = 0, scale_type = 0;
	int rc = 0, i = 0, version = 0;
	bool chan_found = false;

	if (qpnp_adc_tm_is_valid(chip)) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	if (param->threshold_notification == NULL) {
		pr_debug("No notification for high/low temp??\n");
		return -EINVAL;
	}

	mutex_lock(&chip->adc->adc_lock);

	channel = param->channel;

	if (channel == VSYS) {
		version = qpnp_adc_get_revid_version(chip->dev);
		if (version == QPNP_REV_ID_PM8950_1_0) {
			pr_debug("Channel not supported\n");
			rc = -EINVAL;
			goto fail_unlock;
		}
	}

	while (i < chip->max_channels_available) {
		if (chip->adc->adc_channels[i].channel_num ==
							channel) {
			dt_index = i;
			chan_found = true;
			i++;
		} else
			i++;
	}

	if (!chan_found)  {
		pr_err("not a valid ADC_TM channel\n");
		rc = -EINVAL;
		goto fail_unlock;
	}

	rc = qpnp_adc_tm_check_revision(chip,
			chip->sensor[dt_index].btm_channel_num);
	if (rc < 0)
		goto fail_unlock;

	scale_type = chip->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_RSCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}


	amux_prescaling =
		chip->adc->adc_channels[dt_index].chan_path_prescaling;

	if (amux_prescaling >= PATH_SCALING_NONE) {
		rc = -EINVAL;
		goto fail_unlock;
	}

	pr_debug("channel:%d, scale_type:%d, dt_idx:%d",
					channel, scale_type, dt_index);
	param->gain_num = qpnp_vadc_amux_scaling_ratio[amux_prescaling].num;
	param->gain_den = qpnp_vadc_amux_scaling_ratio[amux_prescaling].den;
	param->adc_tm_hc = chip->adc_tm_hc;
	param->full_scale_code = chip->adc->adc_prop->full_scale_code;
	chip->adc->amux_prop->amux_channel = channel;
	chip->adc->amux_prop->decimation =
			chip->adc->adc_channels[dt_index].adc_decimation;
	chip->adc->amux_prop->hw_settle_time =
			chip->adc->adc_channels[dt_index].hw_settle_time;
	chip->adc->amux_prop->fast_avg_setup =
			chip->adc->adc_channels[dt_index].fast_avg_setup;
	chip->adc->amux_prop->mode_sel =
		ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
	adc_tm_rscale_fn[scale_type].chan(chip->vadc_dev, param,
			&chip->adc->amux_prop->chan_prop->low_thr,
			&chip->adc->amux_prop->chan_prop->high_thr);
	qpnp_adc_tm_add_to_list(chip, dt_index, param,
				chip->adc->amux_prop->chan_prop);
	chip->adc->amux_prop->chan_prop->tm_channel_select =
				chip->sensor[dt_index].btm_channel_num;
	chip->adc->amux_prop->chan_prop->state_request =
					param->state_request;
	chip->adc->amux_prop->calib_type =
			chip->adc->adc_channels[dt_index].calib_type;
	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_configure(chip, chip->adc->amux_prop);
		if (rc) {
			pr_err("adc-tm configure failed with %d\n", rc);
			goto fail_unlock;
		}
	} else {
		rc = qpnp_adc_tm_hc_configure(chip, chip->adc->amux_prop);
		if (rc) {
			pr_err("adc-tm hc configure failed with %d\n", rc);
			goto fail_unlock;
		}
	}

	chip->sensor[dt_index].scale_type = scale_type;

fail_unlock:
	mutex_unlock(&chip->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_adc_tm_channel_measure);

int32_t qpnp_adc_tm_disable_chan_meas(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param)
{
	uint32_t channel, dt_index = 0, btm_chan_num;
	u8 sensor_mask = 0, mode_ctl = 0;
	int rc = 0;

	if (qpnp_adc_tm_is_valid(chip))
		return -ENODEV;

	mutex_lock(&chip->adc->adc_lock);

	if (!chip->adc_tm_hc) {
		/* Set measurement in single measurement mode */
		mode_ctl = ADC_OP_NORMAL_MODE << QPNP_OP_MODE_SHIFT;
		rc = qpnp_adc_tm_mode_select(chip, mode_ctl);
		if (rc < 0) {
			pr_err("adc-tm single mode select failed\n");
			goto fail;
		}
	}

	/* Disable bank */
	rc = qpnp_adc_tm_disable(chip);
	if (rc < 0) {
		pr_err("adc-tm disable failed\n");
		goto fail;
	}

	if (!chip->adc_tm_hc) {
		/* Check if a conversion is in progress */
		rc = qpnp_adc_tm_req_sts_check(chip);
		if (rc < 0) {
			pr_err("adc-tm req_sts check failed\n");
			goto fail;
		}
	}

	channel = param->channel;
	while ((chip->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < chip->max_channels_available))
		dt_index++;

	if (dt_index >= chip->max_channels_available) {
		pr_err("not a valid ADC_TMN channel\n");
		rc = -EINVAL;
		goto fail;
	}

	btm_chan_num = chip->sensor[dt_index].btm_channel_num;

	if (!chip->adc_tm_hc) {
		sensor_mask = 1 << chip->sensor[dt_index].sensor_num;

		rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_TM_LOW_THR_INT_EN,
				sensor_mask, false);
		if (rc < 0) {
			pr_err("high threshold int enable failed\n");
			goto fail;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
				sensor_mask, false);
		if (rc < 0) {
			pr_err("multi measurement en failed\n");
			goto fail;
		}
	} else {
		rc = qpnp_adc_tm_reg_update(chip, QPNP_BTM_Mn_EN(btm_chan_num),
					QPNP_BTM_Mn_HIGH_THR_INT_EN, false);
		if (rc < 0) {
			pr_err("high thr disable err:%d\n", btm_chan_num);
			return rc;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_BTM_Mn_EN(btm_chan_num),
				QPNP_BTM_Mn_LOW_THR_INT_EN, false);
		if (rc < 0) {
			pr_err("low thr disable err:%d\n", btm_chan_num);
			return rc;
		}

		rc = qpnp_adc_tm_reg_update(chip, QPNP_BTM_Mn_EN(btm_chan_num),
				QPNP_BTM_Mn_MEAS_EN, false);
		if (rc < 0) {
			pr_err("multi measurement disable failed\n");
			return rc;
		}
	}

	rc = qpnp_adc_tm_enable_if_channel_meas(chip);
	if (rc < 0)
		pr_err("re-enabling measurement failed\n");

fail:
	mutex_unlock(&chip->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_adc_tm_disable_chan_meas);

struct qpnp_adc_tm_chip *qpnp_get_adc_tm(struct device *dev, const char *name)
{
	struct qpnp_adc_tm_chip *chip;
	struct device_node *node = NULL;
	char prop_name[QPNP_MAX_PROP_NAME_LEN];

	snprintf(prop_name, QPNP_MAX_PROP_NAME_LEN, "qcom,%s-adc_tm", name);

	node = of_parse_phandle(dev->of_node, prop_name, 0);
	if (node == NULL)
		return ERR_PTR(-ENODEV);

	list_for_each_entry(chip, &qpnp_adc_tm_device_list, list)
		if (chip->adc->pdev->dev.of_node == node)
			return chip;

	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(qpnp_get_adc_tm);

static int qpnp_adc_tm_initial_setup(struct qpnp_adc_tm_chip *chip)
{
	u8 thr_init = 0;
	int rc = 0;

	rc = qpnp_adc_tm_write_reg(chip, QPNP_ADC_TM_HIGH_THR_INT_EN,
							thr_init, 1);
	if (rc < 0) {
		pr_err("high thr init failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(chip, QPNP_ADC_TM_LOW_THR_INT_EN,
			thr_init, 1);
	if (rc < 0) {
		pr_err("low thr init failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(chip, QPNP_ADC_TM_MULTI_MEAS_EN,
							thr_init, 1);
	if (rc < 0) {
		pr_err("multi meas en failed\n");
		return rc;
	}

	return rc;
}

static const struct of_device_id qpnp_adc_tm_match_table[] = {
	{	.compatible = "qcom,qpnp-adc-tm" },
	{	.compatible = "qcom,qpnp-adc-tm-hc" },
	{	.compatible = "qcom,qpnp-adc-tm-hc-pm5" },
	{}
};

static int qpnp_adc_tm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node, *child;
	struct qpnp_adc_tm_chip *chip;
	struct qpnp_adc_drv *adc_qpnp;
	int32_t count_adc_channel_list = 0, rc, sen_idx = 0, i = 0;
	bool thermal_node = false;
	const struct of_device_id *id;

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	id = of_match_node(qpnp_adc_tm_match_table, node);
	if (id == NULL) {
		pr_err("qpnp_adc_tm_match of_node prop not present\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(struct qpnp_adc_tm_chip) +
			(count_adc_channel_list *
			sizeof(struct qpnp_adc_tm_sensor)),
				GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	list_add(&chip->list, &qpnp_adc_tm_device_list);
	chip->max_channels_available = count_adc_channel_list;

	adc_qpnp = devm_kzalloc(&pdev->dev, sizeof(struct qpnp_adc_drv),
			GFP_KERNEL);
	if (!adc_qpnp) {
		rc = -ENOMEM;
		goto fail;
	}

	chip->dev = &(pdev->dev);
	chip->adc = adc_qpnp;
	chip->adc->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->adc->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		rc = -EINVAL;
		goto fail;
	}

	if (of_device_is_compatible(node, "qcom,qpnp-adc-tm-hc")) {
		chip->adc_tm_hc = true;
		chip->adc->adc_hc = true;
	}

	rc = qpnp_adc_get_devicetree_data(pdev, chip->adc);
	if (rc) {
		dev_err(&pdev->dev, "failed to read device tree\n");
		goto fail;
	}
	mutex_init(&chip->adc->adc_lock);

	/* Register the ADC peripheral interrupt */
	if (!chip->adc_tm_hc) {
		chip->adc->adc_high_thr_irq = platform_get_irq_byname(pdev,
				"high-thr-en-set");
		if (chip->adc->adc_high_thr_irq < 0) {
			pr_err("Invalid irq\n");
			rc = -ENXIO;
			goto fail;
		}

		chip->adc->adc_low_thr_irq = platform_get_irq_byname(pdev,
				"low-thr-en-set");
		if (chip->adc->adc_low_thr_irq < 0) {
			pr_err("Invalid irq\n");
			rc = -ENXIO;
			goto fail;
		}
	}
	chip->vadc_dev = qpnp_get_vadc(&pdev->dev, "adc_tm");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("vadc property missing, rc=%d\n", rc);
		goto fail;
	}

	chip->adc_tm_recalib_check = of_property_read_bool(node,
				"qcom,adc-tm-recalib-check");

	for_each_child_of_node(node, child) {
		char name[25];
		int btm_channel_num, timer_select = 0;

		rc = of_property_read_u32(child,
				"qcom,btm-channel-number", &btm_channel_num);
		if (rc) {
			pr_err("Invalid btm channel number\n");
			goto fail;
		}
		rc = of_property_read_u32(child,
				"qcom,meas-interval-timer-idx", &timer_select);
		if (rc) {
			pr_debug("Default to timer2 with interval of 1 sec\n");
			chip->sensor[sen_idx].timer_select =
							ADC_MEAS_TIMER_SELECT2;
			chip->sensor[sen_idx].meas_interval =
							ADC_MEAS2_INTERVAL_1S;
		} else {
			if (timer_select >= ADC_MEAS_TIMER_NUM) {
				pr_err("Invalid timer selection number\n");
				goto fail;
			}
			chip->sensor[sen_idx].timer_select = timer_select;
			if (timer_select == ADC_MEAS_TIMER_SELECT1)
				chip->sensor[sen_idx].meas_interval =
						ADC_MEAS1_INTERVAL_3P9MS;
			else if (timer_select == ADC_MEAS_TIMER_SELECT3)
				chip->sensor[sen_idx].meas_interval =
						ADC_MEAS3_INTERVAL_4S;
			else if (timer_select == ADC_MEAS_TIMER_SELECT2)
				chip->sensor[sen_idx].meas_interval =
						ADC_MEAS2_INTERVAL_1S;
		}

		chip->sensor[sen_idx].btm_channel_num = btm_channel_num;
		chip->sensor[sen_idx].vadc_channel_num =
				chip->adc->adc_channels[sen_idx].channel_num;
		chip->sensor[sen_idx].sensor_num = sen_idx;
		chip->sensor[sen_idx].chip = chip;
		pr_debug("btm_chan:%x, vadc_chan:%x\n", btm_channel_num,
			chip->adc->adc_channels[sen_idx].channel_num);
		thermal_node = of_property_read_bool(child,
					"qcom,thermal-node");
		if (thermal_node) {
			/* Register with the thermal zone */
			pr_debug("thermal node%x\n", btm_channel_num);
			chip->sensor[sen_idx].mode = THERMAL_DEVICE_DISABLED;
			chip->sensor[sen_idx].thermal_node = true;
			snprintf(name, sizeof(name), "%s",
				chip->adc->adc_channels[sen_idx].name);
			chip->sensor[sen_idx].low_thr =
						QPNP_ADC_TM_M0_LOW_THR;
			chip->sensor[sen_idx].high_thr =
						QPNP_ADC_TM_M0_HIGH_THR;
			chip->sensor[sen_idx].tz_dev =
				devm_thermal_zone_of_sensor_register(
				chip->dev,
				chip->sensor[sen_idx].vadc_channel_num,
				&chip->sensor[sen_idx],
				&qpnp_adc_tm_thermal_ops);
			if (IS_ERR(chip->sensor[sen_idx].tz_dev))
				pr_err("thermal device register failed.\n");
		}
		chip->sensor[sen_idx].req_wq = alloc_workqueue(
				"qpnp_adc_notify_wq", WQ_HIGHPRI, 0);
		if (!chip->sensor[sen_idx].req_wq) {
			pr_err("Requesting priority wq failed\n");
			goto fail;
		}
		INIT_WORK(&chip->sensor[sen_idx].work, notify_adc_tm_fn);
		INIT_LIST_HEAD(&chip->sensor[sen_idx].thr_list);
		sen_idx++;
	}

	chip->high_thr_wq = alloc_workqueue("qpnp_adc_tm_high_thr_wq",
							WQ_HIGHPRI, 0);
	if (!chip->high_thr_wq) {
		pr_err("Requesting high thr priority wq failed\n");
		goto fail;
	}

	chip->low_thr_wq = alloc_workqueue("qpnp_adc_tm_low_thr_wq",
							WQ_HIGHPRI, 0);
	if (!chip->low_thr_wq) {
		pr_err("Requesting low thr priority wq failed\n");
		goto fail;
	}

	chip->thr_wq = alloc_workqueue("qpnp_adc_tm_thr_wq",
						WQ_HIGHPRI, 0);
	if (!chip->thr_wq) {
		pr_err("Requesting thr priority wq failed\n");
		goto fail;
	}

	INIT_WORK(&chip->trigger_high_thr_work, qpnp_adc_tm_high_thr_work);
	INIT_WORK(&chip->trigger_low_thr_work, qpnp_adc_tm_low_thr_work);

	if (!chip->adc_tm_hc) {
		rc = qpnp_adc_tm_initial_setup(chip);
		if (rc)
			goto fail;
		rc = devm_request_irq(&pdev->dev, chip->adc->adc_high_thr_irq,
				qpnp_adc_tm_high_thr_isr,
		IRQF_TRIGGER_RISING, "qpnp_adc_tm_high_interrupt", chip);
		if (rc) {
			dev_err(&pdev->dev, "failed to request adc irq\n");
			goto fail;
		} else {
			enable_irq_wake(chip->adc->adc_high_thr_irq);
		}

		rc = devm_request_irq(&pdev->dev, chip->adc->adc_low_thr_irq,
				qpnp_adc_tm_low_thr_isr,
				IRQF_TRIGGER_RISING,
				"qpnp_adc_tm_low_interrupt", chip);
		if (rc) {
			dev_err(&pdev->dev, "failed to request adc irq\n");
			goto fail;
		} else {
			enable_irq_wake(chip->adc->adc_low_thr_irq);
		}
	} else {
		rc = devm_request_irq(&pdev->dev, chip->adc->adc_irq_eoc,
				qpnp_adc_tm_rc_thr_isr,
			IRQF_TRIGGER_RISING, "qpnp_adc_tm_interrupt", chip);
		if (rc)
			dev_err(&pdev->dev, "failed to request adc irq\n");
		else
			enable_irq_wake(chip->adc->adc_irq_eoc);
	}

	chip->adc_vote_enable = false;
	dev_set_drvdata(&pdev->dev, chip);
	spin_lock_init(&chip->th_info.adc_tm_low_lock);
	spin_lock_init(&chip->th_info.adc_tm_high_lock);

	pr_debug("OK\n");
	return 0;
fail:
	for_each_child_of_node(node, child) {
		thermal_node = of_property_read_bool(child,
					"qcom,thermal-node");
		if (thermal_node) {
			thermal_zone_device_unregister(chip->sensor[i].tz_dev);
			if (chip->sensor[i].req_wq)
				destroy_workqueue(chip->sensor[sen_idx].req_wq);
		}
	}
	if (chip->high_thr_wq)
		destroy_workqueue(chip->high_thr_wq);
	if (chip->low_thr_wq)
		destroy_workqueue(chip->low_thr_wq);
	list_del(&chip->list);
	dev_set_drvdata(&pdev->dev, NULL);
	return rc;
}

static int qpnp_adc_tm_remove(struct platform_device *pdev)
{
	struct qpnp_adc_tm_chip *chip = dev_get_drvdata(&pdev->dev);
	struct device_node *node = pdev->dev.of_node, *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		if (chip->sensor[i].req_wq)
			destroy_workqueue(chip->sensor[i].req_wq);
		i++;
	}

	if (chip->high_thr_wq)
		destroy_workqueue(chip->high_thr_wq);
	if (chip->low_thr_wq)
		destroy_workqueue(chip->low_thr_wq);
	if (chip->adc->hkadc_ldo && chip->adc->hkadc_ldo_ok)
		qpnp_adc_free_voltage_resource(chip->adc);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static void qpnp_adc_tm_shutdown(struct platform_device *pdev)
{
	struct qpnp_adc_tm_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc = 0, i = 0;

	/* Disable bank */
	rc = qpnp_adc_tm_disable(chip);
	if (rc < 0)
		pr_err("adc-tm disable failed\n");

	for (i = 0; i < QPNP_BTM_CHANNELS; i++) {
		rc = qpnp_adc_tm_reg_update(chip,
			QPNP_BTM_Mn_EN(i),
			QPNP_BTM_Mn_MEAS_EN, false);
		if (rc < 0)
			pr_err("multi measurement disable failed\n");
	}
}

static int qpnp_adc_tm_suspend_noirq(struct device *dev)
{
	struct qpnp_adc_tm_chip *chip = dev_get_drvdata(dev);
	struct device_node *node = dev->of_node, *child;
	int i = 0;

	flush_workqueue(chip->high_thr_wq);
	flush_workqueue(chip->low_thr_wq);

	for_each_child_of_node(node, child) {
		if (chip->sensor[i].req_wq) {
			pr_debug("flushing queue for sensor %d\n", i);
			flush_workqueue(chip->sensor[i].req_wq);
		}
		i++;
	}
	return 0;
}

static const struct dev_pm_ops qpnp_adc_tm_pm_ops = {
	.suspend_noirq	= qpnp_adc_tm_suspend_noirq,
};

static struct platform_driver qpnp_adc_tm_driver = {
	.driver		= {
		.name		= "qcom,qpnp-adc-tm",
		.of_match_table	= qpnp_adc_tm_match_table,
		.pm		= &qpnp_adc_tm_pm_ops,
	},
	.probe		= qpnp_adc_tm_probe,
	.remove		= qpnp_adc_tm_remove,
	.shutdown	= qpnp_adc_tm_shutdown,
};

static int __init qpnp_adc_tm_init(void)
{
	return platform_driver_register(&qpnp_adc_tm_driver);
}
module_init(qpnp_adc_tm_init);

static void __exit qpnp_adc_tm_exit(void)
{
	platform_driver_unregister(&qpnp_adc_tm_driver);
}
module_exit(qpnp_adc_tm_exit);

MODULE_DESCRIPTION("QPNP PMIC ADC Threshold Monitoring driver");
MODULE_LICENSE("GPL v2");
