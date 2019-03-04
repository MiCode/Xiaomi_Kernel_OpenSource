/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __QCOM_ADC_TM_H__
#define __QCOM_ADC_TM_H__

#include <linux/kernel.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/adc-tm-clients.h>

#define ADC_TM_DECIMATION_DEFAULT	840
#define ADC_TM_DECIMATION_SAMPLES_MAX	3
#define ADC_TM_DEF_AVG_SAMPLES		0 /* 1 sample */
#define ADC_TM_DEF_HW_SETTLE_TIME	0 /* 15 us */
#define ADC_TM_HW_SETTLE_SAMPLES_MAX	16
#define ADC_TM_AVG_SAMPLES_MAX		16
#define ADC_TM_TIMER1			3 /* 3.9ms */
#define ADC_TM_TIMER2			10 /* 1 second */
#define ADC_TM_TIMER3			4 /* 4 second */
#define ADC_HC_VDD_REF			1875000
#define MAX_PROP_NAME_LEN					32

enum adc_cal_method {
	ADC_NO_CAL = 0,
	ADC_RATIO_CAL = 1,
	ADC_ABS_CAL = 2,
	ADC_CAL_SEL_NONE,
};

enum adc_cal_val {
	ADC_TIMER_CAL = 0,
	ADC_NEW_CAL,
	ADC_CAL_VAL_NONE,
};

enum adc_timer_select {
	ADC_TIMER_SEL_1 = 0,
	ADC_TIMER_SEL_2,
	ADC_TIMER_SEL_3,
	ADC_TIMER_SEL_NONE,
};

/**
 * enum adc_tm_rscale_fn_type - Scaling function used to convert the
 *	channels input voltage/temperature to corresponding ADC code that is
 *	applied for thresholds. Check the corresponding channels scaling to
 *	determine the appropriate temperature/voltage units that are passed
 *	to the scaling function. Example battery follows the power supply
 *	framework that needs its units to be in decidegreesC so it passes
 *	deci-degreesC. PA_THERM clients pass the temperature in degrees.
 *	The order below should match the one in the driver for
 *	adc_tm_rscale_fn[].
 */
enum adc_tm_rscale_fn_type {
	SCALE_R_ABSOLUTE = 0,
	SCALE_RSCALE_NONE,
};

struct adc_tm_sensor {
	struct adc_tm_chip		*chip;
	struct thermal_zone_device	*tzd;
	enum adc_cal_val		cal_val;
	enum adc_cal_method		cal_sel;
	unsigned int			hw_settle_time;
	unsigned int			adc_ch;
	unsigned int			btm_ch;
	unsigned int			prescaling;
	unsigned int			timer_select;
	enum adc_tm_rscale_fn_type	adc_rscale_fn;
	struct iio_channel		*adc;
	struct list_head		thr_list;
	bool					non_thermal;
	bool				high_thr_triggered;
	bool				low_thr_triggered;
	struct workqueue_struct		*req_wq;
	struct work_struct		work;
};

struct adc_tm_client_info {
	struct list_head			list;
	struct adc_tm_param			*param;
	int32_t						low_thr_requested;
	int32_t						high_thr_requested;
	bool						notify_low_thr;
	bool						notify_high_thr;
	bool						high_thr_set;
	bool						low_thr_set;
	enum adc_tm_state_request	state_request;
};

struct adc_tm_cmn_prop {
	unsigned int			decimation;
	unsigned int			fast_avg_samples;
	unsigned int			timer1;
	unsigned int			timer2;
	unsigned int			timer3;
};

struct adc_tm_ops {
	int (*get_temp)(struct adc_tm_sensor *, int *);
	int (*init)(struct adc_tm_chip *, uint32_t);
	int (*set_trips)(struct adc_tm_sensor *, int, int);
	int (*interrupts_reg)(struct adc_tm_chip *);
	int (*shutdown)(struct adc_tm_chip *);
};

struct adc_tm_chip {
	struct device			*dev;
	struct list_head		list;
	struct regmap			*regmap;
	u16				base;
	struct adc_tm_cmn_prop		prop;
	spinlock_t			adc_tm_lock;
	struct mutex		adc_mutex_lock;
	const struct adc_tm_ops		*ops;
	const struct adc_tm_data	*data;
	unsigned int			dt_channels;
	struct pmic_revid_data		*pmic_rev_id;
	struct adc_tm_sensor		sensor[0];
};

struct adc_tm_data {
	const struct adc_tm_ops *ops;
	const u32	full_scale_code_volt;
	unsigned int	*decimation;
	unsigned int	*hw_settle;
};

extern const struct adc_tm_data data_adc_tm5;
/**
 * Channel index for the corresponding index to adc_tm_channel_select
 */
enum adc_tm_channel_num {
	ADC_TM_CHAN0 = 0,
	ADC_TM_CHAN1,
	ADC_TM_CHAN2,
	ADC_TM_CHAN3,
	ADC_TM_CHAN4,
	ADC_TM_CHAN5,
	ADC_TM_CHAN6,
	ADC_TM_CHAN7,
	ADC_TM_CHAN_NONE
};

/**
 * Channel selection registers for each of the configurable measurements
 * Channels allotment is set at device config for a channel.
 */
enum adc_tm_channel_sel	{
	ADC_TM_M0_ADC_CH_SEL_CTL = 0x60,
	ADC_TM_M1_ADC_CH_SEL_CTL = 0x68,
	ADC_TM_M2_ADC_CH_SEL_CTL = 0x70,
	ADC_TM_M3_ADC_CH_SEL_CTL = 0x78,
	ADC_TM_M4_ADC_CH_SEL_CTL = 0x80,
	ADC_TM_M5_ADC_CH_SEL_CTL = 0x88,
	ADC_TM_M6_ADC_CH_SEL_CTL = 0x90,
	ADC_TM_M7_ADC_CH_SEL_CTL = 0x98,
	ADC_TM_CH_SELECT_NONE
};

/**
 * enum adc_tm_fast_avg_ctl - Provides ability to obtain single result
 *		from the ADC that is an average of multiple measurement
 *		samples. Select number of samples for use in fast
 *		average mode (i.e. 2 ^ value).
 * %ADC_FAST_AVG_SAMPLE_1:   0x0 = 1
 * %ADC_FAST_AVG_SAMPLE_2:   0x1 = 2
 * %ADC_FAST_AVG_SAMPLE_4:   0x2 = 4
 * %ADC_FAST_AVG_SAMPLE_8:   0x3 = 8
 * %ADC_FAST_AVG_SAMPLE_16:  0x4 = 16
 */
enum qpnp_adc_fast_avg_ctl {
	ADC_FAST_AVG_SAMPLE_1 = 0,
	ADC_FAST_AVG_SAMPLE_2,
	ADC_FAST_AVG_SAMPLE_4,
	ADC_FAST_AVG_SAMPLE_8,
	ADC_FAST_AVG_SAMPLE_16,
	ADC_FAST_AVG_SAMPLE_NONE,
};

struct adc_tm_trip_reg_type {
	enum adc_tm_channel_sel		btm_amux_ch;
	uint16_t			low_thr_lsb_addr;
	uint16_t			low_thr_msb_addr;
	uint16_t			high_thr_lsb_addr;
	uint16_t			high_thr_msb_addr;
	u8				multi_meas_en;
	u8				low_thr_int_chan_en;
	u8				high_thr_int_chan_en;
	u8				meas_interval_ctl;
};

/**
 * struct adc_tm_config - Represent ADC Thermal Monitor configuration.
 * @channel: ADC channel for which thermal monitoring is requested.
 * @adc_code: The pre-calibrated digital output of a given ADC releative to the
 *		ADC reference.
 * @high_thr_temp: Temperature at which high threshold notification is required.
 * @low_thr_temp: Temperature at which low threshold notification is required.
 * @low_thr_voltage : Low threshold voltage ADC code used for reverse
 *			calibration.
 * @high_thr_voltage: High threshold voltage ADC code used for reverse
 *			calibration.
 */
struct adc_tm_config {
	int	channel;
	int	adc_code;
	int	prescal;
	int	high_thr_temp;
	int	low_thr_temp;
	int64_t	high_thr_voltage;
	int64_t	low_thr_voltage;
};

/**
 * struct adc_tm_reverse_scale_fn - Reverse scaling prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties and returns the physical result
 */
struct adc_tm_reverse_scale_fn {
	int32_t (*chan)(const struct adc_tm_data *,
		struct adc_tm_config *);
};

/**
 * struct adc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code.
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance.
 */
struct adc_tm_map_pt {
	int32_t x;
	int32_t y;
};

/**
 * struct adc_linear_graph - Represent ADC characteristics.
 * @dy: numerator slope to calculate the gain.
 * @dx: denominator slope to calculate the gain.
 * @gnd: A/D word of the ground reference used for the channel.
 *
 * Each ADC device has different offset and gain parameters which are
 * computed to calibrate the device.
 */
struct adc_tm_linear_graph {
	s32 dy;
	s32 dx;
	s32 gnd;
};

void adc_tm_scale_therm_voltage_100k(struct adc_tm_config *param,
				const struct adc_tm_data *data);

int32_t adc_tm_absolute_rthr(const struct adc_tm_data *data,
			struct adc_tm_config *tm_config);

void notify_adc_tm_fn(struct work_struct *work);

int adc_tm_is_valid(struct adc_tm_chip *chip);

#endif /* __QCOM_ADC_TM_H__ */
