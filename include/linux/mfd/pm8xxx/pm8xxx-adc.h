/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
/*
 * Qualcomm PMIC 8921/8018 ADC driver header file
 *
 */

#ifndef __PM8XXX_ADC_H
#define __PM8XXX_ADC_H

#include <linux/kernel.h>
#include <linux/list.h>

/**
 * enum pm8xxx_adc_channels - PM8XXX AMUX arbiter channels
 * %CHANNEL_VCOIN: Backup voltage for certain register set
 * %CHANNEL_VBAT: Battery voltage
 * %CHANNEL_DCIN: Charger input voltage without internal OVP
 * %CHANNEL_ICHG: Charge-current monitor
 * %CHANNEL_VPH_PWR: Main system power
 * %CHANNEL_IBAT: Battery charge current
 * %CHANNEL_MPP_1: 16:1 pre-mux unity scale MPP input
 * %CHANNEL_MPP_2: 16:1 pre-mux 1/3 scale MPP input
 * %CHANNEL_BATT_THERM: Battery temperature
 * %CHANNEL_BATT_ID: Battery detection
 * %CHANNEL_USBIN: Charger input voltage with internal OVP
 * %CHANNEL_DIE_TEMP: Pmic_die temperature
 * %CHANNEL_625MV: 625mv reference channel
 * %CHANNEL_125V: 1.25v reference channel
 * %CHANNEL_CHG_TEMP: Charger temperature
 * %CHANNEL_MUXOFF: Channel to reduce input load on the mux
 * %CHANNEL_NONE: Do not use this channel
 */
enum pm8xxx_adc_channels {
	CHANNEL_VCOIN = 0,
	CHANNEL_VBAT,
	CHANNEL_DCIN,
	CHANNEL_ICHG,
	CHANNEL_VPH_PWR,
	CHANNEL_IBAT,
	CHANNEL_MPP_1,
	CHANNEL_MPP_2,
	CHANNEL_BATT_THERM,
	/* PM8018 ADC Arbiter uses a single channel on AMUX8
	 * to read either Batt_id or Batt_therm.
	 */
	CHANNEL_BATT_ID_THERM = CHANNEL_BATT_THERM,
	CHANNEL_BATT_ID,
	CHANNEL_USBIN,
	CHANNEL_DIE_TEMP,
	CHANNEL_625MV,
	CHANNEL_125V,
	CHANNEL_CHG_TEMP,
	CHANNEL_MUXOFF,
	CHANNEL_NONE,
	ADC_MPP_1_ATEST_8 = 20,
	ADC_MPP_1_USB_SNS_DIV20,
	ADC_MPP_1_DCIN_SNS_DIV20,
	ADC_MPP_1_AMUX3,
	ADC_MPP_1_AMUX4,
	ADC_MPP_1_AMUX5,
	ADC_MPP_1_AMUX6,
	ADC_MPP_1_AMUX7,
	ADC_MPP_1_AMUX8,
	ADC_MPP_1_ATEST_1,
	ADC_MPP_1_ATEST_2,
	ADC_MPP_1_ATEST_3,
	ADC_MPP_1_ATEST_4,
	ADC_MPP_1_ATEST_5,
	ADC_MPP_1_ATEST_6,
	ADC_MPP_1_ATEST_7,
	ADC_MPP_2_ATEST_8 = 40,
	ADC_MPP_2_USB_SNS_DIV20,
	ADC_MPP_2_DCIN_SNS_DIV20,
	ADC_MPP_2_AMUX3,
	ADC_MPP_2_AMUX4,
	ADC_MPP_2_AMUX5,
	ADC_MPP_2_AMUX6,
	ADC_MPP_2_AMUX7,
	ADC_MPP_2_AMUX8,
	ADC_MPP_2_ATEST_1,
	ADC_MPP_2_ATEST_2,
	ADC_MPP_2_ATEST_3,
	ADC_MPP_2_ATEST_4,
	ADC_MPP_2_ATEST_5,
	ADC_MPP_2_ATEST_6,
	ADC_MPP_2_ATEST_7,
	ADC_CHANNEL_MAX_NUM,
};

#define PM8XXX_ADC_PMIC_0	0x0

#define PM8XXX_CHANNEL_ADC_625_UV	625000
#define PM8XXX_CHANNEL_MPP_SCALE1_IDX	20
#define PM8XXX_CHANNEL_MPP_SCALE3_IDX	40

#define PM8XXX_AMUX_MPP_3	0x3
#define PM8XXX_AMUX_MPP_4	0x4
#define PM8XXX_AMUX_MPP_5	0x5
#define PM8XXX_AMUX_MPP_6	0x6
#define PM8XXX_AMUX_MPP_7	0x7
#define PM8XXX_AMUX_MPP_8	0x8

#define PM8XXX_ADC_DEV_NAME	"pm8xxx-adc"

/**
 * enum pm8xxx_adc_decimation_type - Sampling rate supported
 * %ADC_DECIMATION_TYPE1: 512
 * %ADC_DECIMATION_TYPE2: 1K
 * %ADC_DECIMATION_TYPE3: 2K
 * %ADC_DECIMATION_TYPE4: 4k
 * %ADC_DECIMATION_NONE: Do not use this Sampling type
 *
 * The Sampling rate is specific to each channel of the PM8XXX ADC arbiter.
 */
enum pm8xxx_adc_decimation_type {
	ADC_DECIMATION_TYPE1 = 0,
	ADC_DECIMATION_TYPE2,
	ADC_DECIMATION_TYPE3,
	ADC_DECIMATION_TYPE4,
	ADC_DECIMATION_NONE,
};

/**
 * enum pm8xxx_adc_calib_type - PM8XXX ADC Calibration type
 * %ADC_CALIB_ABSOLUTE: Use 625mV and 1.25V reference channels
 * %ADC_CALIB_RATIOMETRIC: Use reference Voltage/GND
 * %ADC_CALIB_CONFIG_NONE: Do not use this calibration type
 *
 * Use the input reference voltage depending on the calibration type
 * to calcluate the offset and gain parameters. The calibration is
 * specific to each channel of the PM8XXX ADC.
 */
enum pm8xxx_adc_calib_type {
	ADC_CALIB_ABSOLUTE = 0,
	ADC_CALIB_RATIOMETRIC,
	ADC_CALIB_NONE,
};

/**
 * enum pm8xxx_adc_channel_scaling_param - pre-scaling AMUX ratio
 * %CHAN_PATH_SCALING1: ratio of {1, 1}
 * %CHAN_PATH_SCALING2: ratio of {1, 3}
 * %CHAN_PATH_SCALING3: ratio of {1, 4}
 * %CHAN_PATH_SCALING4: ratio of {1, 6}
 * %CHAN_PATH_NONE: Do not use this pre-scaling ratio type
 *
 * The pre-scaling is applied for signals to be within the voltage range
 * of the ADC.
 */
enum pm8xxx_adc_channel_scaling_param {
	CHAN_PATH_SCALING1 = 0,
	CHAN_PATH_SCALING2,
	CHAN_PATH_SCALING3,
	CHAN_PATH_SCALING4,
	CHAN_PATH_SCALING_NONE,
};

/**
 * enum pm8xxx_adc_amux_input_rsv - HK/XOADC reference voltage
 * %AMUX_RSV0: XO_IN/XOADC_GND
 * %AMUX_RSV1: PMIC_IN/XOADC_GND
 * %AMUX_RSV2: PMIC_IN/BMS_CSP
 * %AMUX_RSV3: not used
 * %AMUX_RSV4: XOADC_GND/XOADC_GND
 * %AMUX_RSV5: XOADC_VREF/XOADC_GND
 * %AMUX_NONE: Do not use this input reference voltage selection
 */
enum pm8xxx_adc_amux_input_rsv {
	AMUX_RSV0 = 0,
	AMUX_RSV1,
	AMUX_RSV2,
	AMUX_RSV3,
	AMUX_RSV4,
	AMUX_RSV5,
	AMUX_NONE,
};

/**
 * enum pm8xxx_adc_premux_mpp_scale_type - 16:1 pre-mux scale ratio
 * %PREMUX_MPP_SCALE_0: No scaling to the input signal
 * %PREMUX_MPP_SCALE_1: Unity scaling selected by the user for MPP input
 * %PREMUX_MPP_SCALE_1_DIV3: 1/3 pre-scale to the input MPP signal
 * %PREMUX_MPP_NONE: Do not use this pre-scale mpp type
 */
enum pm8xxx_adc_premux_mpp_scale_type {
	PREMUX_MPP_SCALE_0 = 0,
	PREMUX_MPP_SCALE_1,
	PREMUX_MPP_SCALE_1_DIV3,
	PREMUX_MPP_NONE,
};

/**
 * enum pm8xxx_adc_scale_fn_type - Scaling function for pm8921 pre calibrated
 *				   digital data relative to ADC reference
 * %ADC_SCALE_DEFAULT: Default scaling to convert raw adc code to voltage
 * %ADC_SCALE_BATT_THERM: Conversion to temperature based on btm parameters
 * %ADC_SCALE_PMIC_THERM: Returns result in milli degree's Centigrade
 * %ADC_SCALE_XTERN_CHGR_CUR: Returns current across 0.1 ohm resistor
 * %ADC_SCALE_XOTHERM: Returns XO thermistor voltage in degree's Centigrade
 * %ADC_SCALE_NONE: Do not use this scaling type
 */
enum pm8xxx_adc_scale_fn_type {
	ADC_SCALE_DEFAULT = 0,
	ADC_SCALE_BATT_THERM,
	ADC_SCALE_PA_THERM,
	ADC_SCALE_PMIC_THERM,
	ADC_SCALE_XOTHERM,
	ADC_SCALE_NONE,
};

/**
 * struct pm8xxx_adc_linear_graph - Represent ADC characteristics
 * @dy: Numerator slope to calculate the gain
 * @dx: Denominator slope to calculate the gain
 * @adc_vref: A/D word of the voltage reference used for the channel
 * @adc_gnd: A/D word of the ground reference used for the channel
 *
 * Each ADC device has different offset and gain parameters which are computed
 * to calibrate the device.
 */
struct pm8xxx_adc_linear_graph {
	int64_t dy;
	int64_t dx;
	int64_t adc_vref;
	int64_t adc_gnd;
};

/**
 * struct pm8xxx_adc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance
 */
struct pm8xxx_adc_map_pt {
	int32_t x;
	int32_t y;
};

/**
 * struct pm8xxx_adc_scaling_ratio - Represent scaling ratio for adc input
 * @num: Numerator scaling parameter
 * @den: Denominator scaling parameter
 */
struct pm8xxx_adc_scaling_ratio {
	int32_t num;
	int32_t den;
};

/**
 * struct pm8xxx_adc_properties - Represent the ADC properties
 * @adc_reference: Reference voltage for PM8XXX ADC
 * @bitresolution: ADC bit resolution for PM8XXX ADC
 * @biploar: Polarity for PM8XXX ADC
 */
struct pm8xxx_adc_properties {
	uint32_t	adc_vdd_reference;
	uint32_t	bitresolution;
	bool		bipolar;
};

/**
 * struct pm8xxx_adc_chan_properties - Represent channel properties of the ADC
 * @offset_gain_numerator: The inverse numerator of the gain applied to the
 *			   input channel
 * @offset_gain_denominator: The inverse denominator of the gain applied to the
 *			     input channel
 * @adc_graph: ADC graph for the channel of struct type pm8xxx_adc_linear_graph
 */
struct pm8xxx_adc_chan_properties {
	uint32_t			offset_gain_numerator;
	uint32_t			offset_gain_denominator;
	struct pm8xxx_adc_linear_graph	adc_graph[2];
};

/**
 * struct pm8xxx_adc_chan_result - Represent the result of the PM8XXX ADC
 * @chan: The channel number of the requested conversion
 * @adc_code: The pre-calibrated digital output of a given ADC relative to the
 *	      the ADC reference
 * @measurement: In units specific for a given ADC; most ADC uses reference
 *		 voltage but some ADC uses reference current. This measurement
 *		 here is a number relative to a reference of a given ADC
 * @physical: The data meaningful for each individual channel whether it is
 *	      voltage, current, temperature, etc.
 *	      All voltage units are represented in micro - volts.
 *	      -Battery temperature units are represented as 0.1 DegC
 *	      -PA Therm temperature units are represented as DegC
 *	      -PMIC Die temperature units are represented as 0.001 DegC
 */
struct pm8xxx_adc_chan_result {
	uint32_t	chan;
	int32_t		adc_code;
	int64_t		measurement;
	int64_t		physical;
};

static inline int32_t pm8xxx_adc_scale_default(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8xxx_adc_tdkntcg_therm(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8xxx_adc_scale_batt_therm(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8xxx_adc_scale_pa_therm(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8xxx_adc_scale_pmic_therm(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8xxx_adc_scale_batt_id(int32_t adc_code,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop,
			struct pm8xxx_adc_chan_result *chan_rslt)
{ return -ENXIO; }

/**
 * struct pm8xxx_adc_scale_fn - Scaling function prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties, channel properties,
 *	and returns the physical result
 */
struct pm8xxx_adc_scale_fn {
	int32_t (*chan) (int32_t,
		const struct pm8xxx_adc_properties *,
		const struct pm8xxx_adc_chan_properties *,
		struct pm8xxx_adc_chan_result *);
};

/**
 * struct pm8xxx_adc_amux - AMUX properties for individual channel
 * @name: Channel name
 * @channel_name: Channel in integer used from pm8xxx_adc_channels
 * @chan_path_prescaling: Channel scaling performed on the input signal
 * @adc_rsv: Input reference Voltage/GND selection to the ADC
 * @adc_decimation: Sampling rate desired for the channel
 * adc_scale_fn: Scaling function to convert to the data meaningful for
 *		 each individual channel whether it is voltage, current,
 *		 temperature, etc and compensates the channel properties
 */
struct pm8xxx_adc_amux {
	char					*name;
	enum pm8xxx_adc_channels		channel_name;
	enum pm8xxx_adc_channel_scaling_param	chan_path_prescaling;
	enum pm8xxx_adc_amux_input_rsv		adc_rsv;
	enum pm8xxx_adc_decimation_type		adc_decimation;
	enum pm8xxx_adc_scale_fn_type		adc_scale_fn;
};

/**
 * struct pm8xxx_adc_arb_btm_param - PM8XXX ADC BTM parameters to set threshold
 *				     temperature for client notification
 * @low_thr_temp: low temperature threshold request for notification
 * @high_thr_temp: high temperature threshold request for notification
 * @low_thr_voltage: low temperature converted to voltage by arbiter driver
 * @high_thr_voltage: high temperature converted to voltage by arbiter driver
 * @interval: Interval period to check for temperature notification
 * @btm_warm_fn: Remote function call for warm threshold.
 * @btm_cool_fn: Remote function call for cold threshold.
 *
 * BTM client passes the parameters to be set for the
 * temperature threshold notifications. The client is
 * responsible for setting the new threshold
 * levels once the thresholds are reached
 */
struct pm8xxx_adc_arb_btm_param {
	int32_t		low_thr_temp;
	int32_t		high_thr_temp;
	uint64_t	low_thr_voltage;
	uint64_t	high_thr_voltage;
	int32_t		interval;
	void		(*btm_warm_fn) (bool);
	void		(*btm_cool_fn) (bool);
};

int32_t pm8xxx_adc_batt_scaler(struct pm8xxx_adc_arb_btm_param *,
			const struct pm8xxx_adc_properties *adc_prop,
			const struct pm8xxx_adc_chan_properties *chan_prop);
/**
 * struct pm8xxx_adc_platform_data - PM8XXX ADC platform data
 * @adc_prop: ADC specific parameters, voltage and channel setup
 * @adc_channel: Channel properties of the ADC arbiter
 * @adc_num_board_channel: Number of channels added in the board file
 * @adc_mpp_base: PM8XXX MPP0 base passed from board file. This is used
 *		  to offset the PM8XXX MPP passed to configure the
 *		  the MPP to AMUX mapping.
 */
struct pm8xxx_adc_platform_data {
	struct pm8xxx_adc_properties	*adc_prop;
	struct pm8xxx_adc_amux		*adc_channel;
	uint32_t			adc_num_board_channel;
	uint32_t			adc_mpp_base;
};

/* Public API */
static inline uint32_t pm8xxx_adc_read(uint32_t channel,
				struct pm8xxx_adc_chan_result *result)
{ return -ENXIO; }
static inline uint32_t pm8xxx_adc_mpp_config_read(uint32_t mpp_num,
				enum pm8xxx_adc_channels channel,
				struct pm8xxx_adc_chan_result *result)
{ return -ENXIO; }
static inline uint32_t pm8xxx_adc_btm_start(void)
{ return -ENXIO; }
static inline uint32_t pm8xxx_adc_btm_end(void)
{ return -ENXIO; }
static inline uint32_t pm8xxx_adc_btm_configure(
		struct pm8xxx_adc_arb_btm_param *param)
{ return -ENXIO; }

#endif /* PM8XXX_ADC_H */
