/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * Qualcomm PMIC QPNP ADC driver header file
 *
 */

#ifndef __QPNP_ADC_H
#define __QPNP_ADC_H

#include <linux/kernel.h>
#include <linux/list.h>
/**
 * enum qpnp_vadc_channels - QPNP AMUX arbiter channels
 */
enum qpnp_vadc_channels {
	USBIN = 0,
	DCIN,
	VCHG_SNS,
	SPARE1_03,
	SPARE2_03,
	VCOIN,
	VBAT_SNS,
	VSYS,
	DIE_TEMP,
	REF_625MV,
	REF_125V,
	CHG_TEMP,
	SPARE1,
	SPARE2,
	GND_REF,
	VDD_VADC,
	P_MUX1_1_1,
	P_MUX2_1_1,
	P_MUX3_1_1,
	P_MUX4_1_1,
	P_MUX5_1_1,
	P_MUX6_1_1,
	P_MUX7_1_1,
	P_MUX8_1_1,
	P_MUX9_1_1,
	P_MUX10_1_1,
	P_MUX11_1_1,
	P_MUX12_1_1,
	P_MUX13_1_1,
	P_MUX14_1_1,
	P_MUX15_1_1,
	P_MUX16_1_1,
	P_MUX1_1_3,
	P_MUX2_1_3,
	P_MUX3_1_3,
	P_MUX4_1_3,
	P_MUX5_1_3,
	P_MUX6_1_3,
	P_MUX7_1_3,
	P_MUX8_1_3,
	P_MUX9_1_3,
	P_MUX10_1_3,
	P_MUX11_1_3,
	P_MUX12_1_3,
	P_MUX13_1_3,
	P_MUX14_1_3,
	P_MUX15_1_3,
	P_MUX16_1_3,
	LR_MUX1_BATT_THERM,
	LR_MUX2_BAT_ID,
	LR_MUX3_XO_THERM,
	LR_MUX4_AMUX_THM1,
	LR_MUX5_AMUX_THM2,
	LR_MUX6_AMUX_THM3,
	LR_MUX7_HW_ID,
	LR_MUX8_AMUX_THM4,
	LR_MUX9_AMUX_THM5,
	LR_MUX10_USB_ID,
	AMUX_PU1,
	AMUX_PU2,
	LR_MUX3_BUF_XO_THERM_BUF,
	LR_MUX1_PU1_BAT_THERM,
	LR_MUX2_PU1_BAT_ID,
	LR_MUX3_PU1_XO_THERM,
	LR_MUX4_PU1_AMUX_THM1,
	LR_MUX5_PU1_AMUX_THM2,
	LR_MUX6_PU1_AMUX_THM3,
	LR_MUX7_PU1_AMUX_HW_ID,
	LR_MUX8_PU1_AMUX_THM4,
	LR_MUX9_PU1_AMUX_THM5,
	LR_MUX10_PU1_AMUX_USB_ID,
	LR_MUX3_BUF_PU1_XO_THERM_BUF,
	LR_MUX1_PU2_BAT_THERM,
	LR_MUX2_PU2_BAT_ID,
	LR_MUX3_PU2_XO_THERM,
	LR_MUX4_PU2_AMUX_THM1,
	LR_MUX5_PU2_AMUX_THM2,
	LR_MUX6_PU2_AMUX_THM3,
	LR_MUX7_PU2_AMUX_HW_ID,
	LR_MUX8_PU2_AMUX_THM4,
	LR_MUX9_PU2_AMUX_THM5,
	LR_MUX10_PU2_AMUX_USB_ID,
	LR_MUX3_BUF_PU2_XO_THERM_BUF,
	LR_MUX1_PU1_PU2_BAT_THERM,
	LR_MUX2_PU1_PU2_BAT_ID,
	LR_MUX3_PU1_PU2_XO_THERM,
	LR_MUX4_PU1_PU2_AMUX_THM1,
	LR_MUX5_PU1_PU2_AMUX_THM2,
	LR_MUX6_PU1_PU2_AMUX_THM3,
	LR_MUX7_PU1_PU2_AMUX_HW_ID,
	LR_MUX8_PU1_PU2_AMUX_THM4,
	LR_MUX9_PU1_PU2_AMUX_THM5,
	LR_MUX10_PU1_PU2_AMUX_USB_ID,
	LR_MUX3_BUF_PU1_PU2_XO_THERM_BUF,
	ALL_OFF,
	ADC_MAX_NUM,
};

/**
 * enum qpnp_iadc_channels - QPNP IADC channel list
 */
enum qpnp_iadc_channels {
	INTERNAL_RSENSE = 0,
	EXTERNAL_RSENSE,
	ALT_LEAD_PAIR,
	GAIN_CALIBRATION_25MV,
	OFFSET_CALIBRATION_SHORT_CADC_LEADS,
	OFFSET_CALIBRATION_CSP_CSN,
	OFFSET_CALIBRATION_CSP2_CSN2,
	IADC_MUX_NUM,
};

#define QPNP_ADC_625_UV	625000
#define QPNP_ADC_HWMON_NAME_LENGTH				16

/**
 * enum qpnp_adc_decimation_type - Sampling rate supported.
 * %DECIMATION_TYPE1: 512
 * %DECIMATION_TYPE2: 1K
 * %DECIMATION_TYPE3: 2K
 * %DECIMATION_TYPE4: 4k
 * %DECIMATION_NONE: Do not use this Sampling type.
 *
 * The Sampling rate is specific to each channel of the QPNP ADC arbiter.
 */
enum qpnp_adc_decimation_type {
	DECIMATION_TYPE1 = 0,
	DECIMATION_TYPE2,
	DECIMATION_TYPE3,
	DECIMATION_TYPE4,
	DECIMATION_NONE,
};

/**
 * enum qpnp_adc_calib_type - QPNP ADC Calibration type.
 * %ADC_CALIB_ABSOLUTE: Use 625mV and 1.25V reference channels.
 * %ADC_CALIB_RATIOMETRIC: Use reference Voltage/GND.
 * %ADC_CALIB_CONFIG_NONE: Do not use this calibration type.
 *
 * Use the input reference voltage depending on the calibration type
 * to calcluate the offset and gain parameters. The calibration is
 * specific to each channel of the QPNP ADC.
 */
enum qpnp_adc_calib_type {
	CALIB_ABSOLUTE = 0,
	CALIB_RATIOMETRIC,
	CALIB_NONE,
};

/**
 * enum qpnp_adc_channel_scaling_param - pre-scaling AMUX ratio.
 * %CHAN_PATH_SCALING0: ratio of {1, 1}
 * %CHAN_PATH_SCALING1: ratio of {1, 3}
 * %CHAN_PATH_SCALING2: ratio of {1, 4}
 * %CHAN_PATH_SCALING3: ratio of {1, 6}
 * %CHAN_PATH_SCALING4: ratio of {1, 20}
 * %CHAN_PATH_NONE: Do not use this pre-scaling ratio type.
 *
 * The pre-scaling is applied for signals to be within the voltage range
 * of the ADC.
 */
enum qpnp_adc_channel_scaling_param {
	PATH_SCALING0 = 0,
	PATH_SCALING1,
	PATH_SCALING2,
	PATH_SCALING3,
	PATH_SCALING4,
	PATH_SCALING_NONE,
};

/**
 * enum qpnp_adc_scale_fn_type - Scaling function for pm8921 pre calibrated
 *				   digital data relative to ADC reference.
 * %ADC_SCALE_DEFAULT: Default scaling to convert raw adc code to voltage.
 * %ADC_SCALE_BATT_THERM: Conversion to temperature based on btm parameters.
 * %ADC_SCALE_PA_THERM: Returns temperature in degC.
 * %ADC_SCALE_PMIC_THERM: Returns result in milli degree's Centigrade.
 * %ADC_SCALE_XOTHERM: Returns XO thermistor voltage in degree's Centigrade.
 * %ADC_SCALE_NONE: Do not use this scaling type.
 */
enum qpnp_adc_scale_fn_type {
	SCALE_DEFAULT = 0,
	SCALE_BATT_THERM,
	SCALE_PA_THERM,
	SCALE_PMIC_THERM,
	SCALE_XOTHERM,
	SCALE_NONE,
};

/**
 * enum qpnp_adc_fast_avg_ctl - Provides ability to obtain single result
 *		from the ADC that is an average of multiple measurement
 *		samples. Select number of samples for use in fast
 *		average mode (i.e. 2 ^ value).
 * %ADC_FAST_AVG_SAMPLE_1:   0x0 = 1
 * %ADC_FAST_AVG_SAMPLE_2:   0x1 = 2
 * %ADC_FAST_AVG_SAMPLE_4:   0x2 = 4
 * %ADC_FAST_AVG_SAMPLE_8:   0x3 = 8
 * %ADC_FAST_AVG_SAMPLE_16:  0x4 = 16
 * %ADC_FAST_AVG_SAMPLE_32:  0x5 = 32
 * %ADC_FAST_AVG_SAMPLE_64:  0x6 = 64
 * %ADC_FAST_AVG_SAMPLE_128: 0x7 = 128
 * %ADC_FAST_AVG_SAMPLE_256: 0x8 = 256
 * %ADC_FAST_AVG_SAMPLE_512: 0x9 = 512
 */
enum qpnp_adc_fast_avg_ctl {
	ADC_FAST_AVG_SAMPLE_1 = 0,
	ADC_FAST_AVG_SAMPLE_2,
	ADC_FAST_AVG_SAMPLE_4,
	ADC_FAST_AVG_SAMPLE_8,
	ADC_FAST_AVG_SAMPLE_16,
	ADC_FAST_AVG_SAMPLE_32,
	ADC_FAST_AVG_SAMPLE_64,
	ADC_FAST_AVG_SAMPLE_128,
	ADC_FAST_AVG_SAMPLE_256,
	ADC_FAST_AVG_SAMPLE_512,
	ADC_FAST_AVG_SAMPLE_NONE,
};

/**
 * enum qpnp_adc_hw_settle_time - Time between AMUX getting configured and
 *		the ADC starting conversion. Delay = 100us * value for
 *		value < 11 and 2ms * (value - 10) otherwise.
 * %ADC_CHANNEL_HW_SETTLE_DELAY_0US:   0us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_100US: 100us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_200US: 200us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_300US: 300us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_400US: 400us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_500US: 500us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_600US: 600us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_700US: 700us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_800US: 800us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_900US: 900us
 * %ADC_CHANNEL_HW_SETTLE_DELAY_1MS:   1ms
 * %ADC_CHANNEL_HW_SETTLE_DELAY_2MS:   2ms
 * %ADC_CHANNEL_HW_SETTLE_DELAY_4MS:   4ms
 * %ADC_CHANNEL_HW_SETTLE_DELAY_6MS:   6ms
 * %ADC_CHANNEL_HW_SETTLE_DELAY_8MS:   8ms
 * %ADC_CHANNEL_HW_SETTLE_DELAY_10MS:  10ms
 * %ADC_CHANNEL_HW_SETTLE_NONE
 */
enum qpnp_adc_hw_settle_time {
	ADC_CHANNEL_HW_SETTLE_DELAY_0US = 0,
	ADC_CHANNEL_HW_SETTLE_DELAY_100US,
	ADC_CHANNEL_HW_SETTLE_DELAY_2000US,
	ADC_CHANNEL_HW_SETTLE_DELAY_300US,
	ADC_CHANNEL_HW_SETTLE_DELAY_400US,
	ADC_CHANNEL_HW_SETTLE_DELAY_500US,
	ADC_CHANNEL_HW_SETTLE_DELAY_600US,
	ADC_CHANNEL_HW_SETTLE_DELAY_700US,
	ADC_CHANNEL_HW_SETTLE_DELAY_800US,
	ADC_CHANNEL_HW_SETTLE_DELAY_900US,
	ADC_CHANNEL_HW_SETTLE_DELAY_1MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_2MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_4MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_6MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_8MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_10MS,
	ADC_CHANNEL_HW_SETTLE_NONE,
};

/**
 * enum qpnp_vadc_mode_sel - Selects the basic mode of operation.
 *		- The normal mode is used for single measurement.
 *		- The Conversion sequencer is used to trigger an
 *		  ADC read when a HW trigger is selected.
 *		- The measurement interval performs a single or
 *		  continous measurement at a specified interval/delay.
 * %ADC_OP_NORMAL_MODE : Normal mode used for single measurement.
 * %ADC_OP_CONVERSION_SEQUENCER : Conversion sequencer used to trigger
 *		  an ADC read on a HW supported trigger.
 *		  Refer to enum qpnp_vadc_trigger for
 *		  supported HW triggers.
 * %ADC_OP_MEASUREMENT_INTERVAL : The measurement interval performs a
 *		  single or continous measurement after a specified delay.
 *		  For delay look at qpnp_adc_meas_timer.
 */
enum qpnp_vadc_mode_sel {
	ADC_OP_NORMAL_MODE = 0,
	ADC_OP_CONVERSION_SEQUENCER,
	ADC_OP_MEASUREMENT_INTERVAL,
	ADC_OP_MODE_NONE,
};

/**
 * enum qpnp_vadc_trigger - Select the HW trigger to be used while
 *		measuring the ADC reading.
 * %ADC_GSM_PA_ON : GSM power amplifier on.
 * %ADC_TX_GTR_THRES : Transmit power greater than threshold.
 * %ADC_CAMERA_FLASH_RAMP : Flash ramp up done.
 * %ADC_DTEST : DTEST.
 */
enum qpnp_vadc_trigger {
	ADC_GSM_PA_ON = 0,
	ADC_TX_GTR_THRES,
	ADC_CAMERA_FLASH_RAMP,
	ADC_DTEST,
	ADC_SEQ_NONE,
};

/**
 * enum qpnp_vadc_conv_seq_timeout - Select delay (0 to 15ms) from
 *		conversion request to triggering conversion sequencer
 *		hold off time.
 */
enum qpnp_vadc_conv_seq_timeout {
	ADC_CONV_SEQ_TIMEOUT_0MS = 0,
	ADC_CONV_SEQ_TIMEOUT_1MS,
	ADC_CONV_SEQ_TIMEOUT_2MS,
	ADC_CONV_SEQ_TIMEOUT_3MS,
	ADC_CONV_SEQ_TIMEOUT_4MS,
	ADC_CONV_SEQ_TIMEOUT_5MS,
	ADC_CONV_SEQ_TIMEOUT_6MS,
	ADC_CONV_SEQ_TIMEOUT_7MS,
	ADC_CONV_SEQ_TIMEOUT_8MS,
	ADC_CONV_SEQ_TIMEOUT_9MS,
	ADC_CONV_SEQ_TIMEOUT_10MS,
	ADC_CONV_SEQ_TIMEOUT_11MS,
	ADC_CONV_SEQ_TIMEOUT_12MS,
	ADC_CONV_SEQ_TIMEOUT_13MS,
	ADC_CONV_SEQ_TIMEOUT_14MS,
	ADC_CONV_SEQ_TIMEOUT_15MS,
	ADC_CONV_SEQ_TIMEOUT_NONE,
};

/**
 * enum qpnp_adc_conv_seq_holdoff - Select delay from conversion
 *		trigger signal (i.e. adc_conv_seq_trig) transition
 *		to ADC enable. Delay = 25us * (value + 1).
 */
enum qpnp_adc_conv_seq_holdoff {
	ADC_SEQ_HOLD_25US = 0,
	ADC_SEQ_HOLD_50US,
	ADC_SEQ_HOLD_75US,
	ADC_SEQ_HOLD_100US,
	ADC_SEQ_HOLD_125US,
	ADC_SEQ_HOLD_150US,
	ADC_SEQ_HOLD_175US,
	ADC_SEQ_HOLD_200US,
	ADC_SEQ_HOLD_225US,
	ADC_SEQ_HOLD_250US,
	ADC_SEQ_HOLD_275US,
	ADC_SEQ_HOLD_300US,
	ADC_SEQ_HOLD_325US,
	ADC_SEQ_HOLD_350US,
	ADC_SEQ_HOLD_375US,
	ADC_SEQ_HOLD_400US,
	ADC_SEQ_HOLD_NONE,
};

/**
 * enum qpnp_adc_conv_seq_state - Conversion sequencer operating state
 * %ADC_CONV_SEQ_IDLE : Sequencer is in idle.
 * %ADC_CONV_TRIG_RISE : Waiting for rising edge trigger.
 * %ADC_CONV_TRIG_HOLDOFF : Waiting for rising trigger hold off time.
 * %ADC_CONV_MEAS_RISE : Measuring selected ADC signal.
 * %ADC_CONV_TRIG_FALL : Waiting for falling trigger edge.
 * %ADC_CONV_FALL_HOLDOFF : Waiting for falling trigger hold off time.
 * %ADC_CONV_MEAS_FALL : Measuring selected ADC signal.
 * %ADC_CONV_ERROR : Aberrant Hardware problem.
 */
enum qpnp_adc_conv_seq_state {
	ADC_CONV_SEQ_IDLE = 0,
	ADC_CONV_TRIG_RISE,
	ADC_CONV_TRIG_HOLDOFF,
	ADC_CONV_MEAS_RISE,
	ADC_CONV_TRIG_FALL,
	ADC_CONV_FALL_HOLDOFF,
	ADC_CONV_MEAS_FALL,
	ADC_CONV_ERROR,
	ADC_CONV_NONE,
};

/**
 * enum qpnp_adc_meas_timer - Selects the measurement interval time.
 *		If value = 0, use 0ms else use 2^(value + 4)/ 32768).
 * %ADC_MEAS_INTERVAL_0MS : 0ms
 * %ADC_MEAS_INTERVAL_1P0MS : 1ms
 * %ADC_MEAS_INTERVAL_2P0MS : 2ms
 * %ADC_MEAS_INTERVAL_3P9MS : 3.9ms
 * %ADC_MEAS_INTERVAL_7P8MS : 7.8ms
 * %ADC_MEAS_INTERVAL_15P6MS : 15.6ms
 * %ADC_MEAS_INTERVAL_31P3MS : 31.3ms
 * %ADC_MEAS_INTERVAL_62P5MS : 62.5ms
 * %ADC_MEAS_INTERVAL_125MS : 125ms
 * %ADC_MEAS_INTERVAL_250MS : 250ms
 * %ADC_MEAS_INTERVAL_500MS : 500ms
 * %ADC_MEAS_INTERVAL_1S : 1seconds
 * %ADC_MEAS_INTERVAL_2S : 2seconds
 * %ADC_MEAS_INTERVAL_4S : 4seconds
 * %ADC_MEAS_INTERVAL_8S : 8seconds
 * %ADC_MEAS_INTERVAL_16S: 16seconds
 */
enum qpnp_adc_meas_timer {
	ADC_MEAS_INTERVAL_0MS = 0,
	ADC_MEAS_INTERVAL_1P0MS,
	ADC_MEAS_INTERVAL_2P0MS,
	ADC_MEAS_INTERVAL_3P9MS,
	ADC_MEAS_INTERVAL_7P8MS,
	ADC_MEAS_INTERVAL_15P6MS,
	ADC_MEAS_INTERVAL_31P3MS,
	ADC_MEAS_INTERVAL_62P5MS,
	ADC_MEAS_INTERVAL_125MS,
	ADC_MEAS_INTERVAL_250MS,
	ADC_MEAS_INTERVAL_500MS,
	ADC_MEAS_INTERVAL_1S,
	ADC_MEAS_INTERVAL_2S,
	ADC_MEAS_INTERVAL_4S,
	ADC_MEAS_INTERVAL_8S,
	ADC_MEAS_INTERVAL_16S,
	ADC_MEAS_INTERVAL_NONE,
};

/**
 * enum qpnp_adc_meas_interval_op_ctl - Select operating mode.
 * %ADC_MEAS_INTERVAL_OP_SINGLE : Conduct single measurement at specified time
 *			delay.
 * %ADC_MEAS_INTERVAL_OP_CONTINUOUS : Make measurements at measurement interval
 *			times.
 */
enum qpnp_adc_meas_interval_op_ctl {
	ADC_MEAS_INTERVAL_OP_SINGLE = 0,
	ADC_MEAS_INTERVAL_OP_CONTINUOUS,
	ADC_MEAS_INTERVAL_OP_NONE,
};

/**
 * struct qpnp_vadc_linear_graph - Represent ADC characteristics.
 * @dy: Numerator slope to calculate the gain.
 * @dx: Denominator slope to calculate the gain.
 * @adc_vref: A/D word of the voltage reference used for the channel.
 * @adc_gnd: A/D word of the ground reference used for the channel.
 *
 * Each ADC device has different offset and gain parameters which are computed
 * to calibrate the device.
 */
struct qpnp_vadc_linear_graph {
	int64_t dy;
	int64_t dx;
	int64_t adc_vref;
	int64_t adc_gnd;
};

/**
 * struct qpnp_vadc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code.
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance.
 */
struct qpnp_vadc_map_pt {
	int32_t x;
	int32_t y;
};

/**
 * struct qpnp_vadc_scaling_ratio - Represent scaling ratio for adc input.
 * @num: Numerator scaling parameter.
 * @den: Denominator scaling parameter.
 */
struct qpnp_vadc_scaling_ratio {
	int32_t num;
	int32_t den;
};

/**
 * struct qpnp_adc_properties - Represent the ADC properties.
 * @adc_reference: Reference voltage for QPNP ADC.
 * @bitresolution: ADC bit resolution for QPNP ADC.
 * @biploar: Polarity for QPNP ADC.
 */
struct qpnp_adc_properties {
	uint32_t	adc_vdd_reference;
	uint32_t	bitresolution;
	bool		bipolar;
};

/**
 * struct qpnp_vadc_chan_properties - Represent channel properties of the ADC.
 * @offset_gain_numerator: The inverse numerator of the gain applied to the
 *			   input channel.
 * @offset_gain_denominator: The inverse denominator of the gain applied to the
 *			     input channel.
 * @adc_graph: ADC graph for the channel of struct type qpnp_adc_linear_graph.
 */
struct qpnp_vadc_chan_properties {
	uint32_t			offset_gain_numerator;
	uint32_t			offset_gain_denominator;
	struct qpnp_vadc_linear_graph	adc_graph[2];
};

/**
 * struct qpnp_adc_result - Represent the result of the QPNP ADC.
 * @chan: The channel number of the requested conversion.
 * @adc_code: The pre-calibrated digital output of a given ADC relative to the
 *	      the ADC reference.
 * @measurement: In units specific for a given ADC; most ADC uses reference
 *		 voltage but some ADC uses reference current. This measurement
 *		 here is a number relative to a reference of a given ADC.
 * @physical: The data meaningful for each individual channel whether it is
 *	      voltage, current, temperature, etc.
 *	      All voltage units are represented in micro - volts.
 *	      -Battery temperature units are represented as 0.1 DegC.
 *	      -PA Therm temperature units are represented as DegC.
 *	      -PMIC Die temperature units are represented as 0.001 DegC.
 */
struct qpnp_vadc_result {
	uint32_t	chan;
	int32_t		adc_code;
	int64_t		measurement;
	int64_t		physical;
};

/**
 * struct qpnp_adc_amux - AMUX properties for individual channel
 * @name: Channel string name.
 * @channel_num: Channel in integer used from qpnp_adc_channels.
 * @chan_path_prescaling: Channel scaling performed on the input signal.
 * @adc_decimation: Sampling rate desired for the channel.
 * adc_scale_fn: Scaling function to convert to the data meaningful for
 *		 each individual channel whether it is voltage, current,
 *		 temperature, etc and compensates the channel properties.
 */
struct qpnp_vadc_amux {
	char					*name;
	enum qpnp_vadc_channels			channel_num;
	enum qpnp_adc_channel_scaling_param	chan_path_prescaling;
	enum qpnp_adc_decimation_type		adc_decimation;
	enum qpnp_adc_scale_fn_type		adc_scale_fn;
	enum qpnp_adc_fast_avg_ctl		fast_avg_setup;
	enum qpnp_adc_hw_settle_time		hw_settle_time;
};

/**
 * struct qpnp_vadc_scaling_ratio
 *
 */
static const struct qpnp_vadc_scaling_ratio qpnp_vadc_amux_scaling_ratio[] = {
	{1, 1},
	{1, 3},
	{1, 4},
	{1, 6},
	{1, 20}
};

/**
 * struct qpnp_vadc_scale_fn - Scaling function prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties, channel properties,
 *	and returns the physical result
 */
struct qpnp_vadc_scale_fn {
	int32_t (*chan) (int32_t,
		const struct qpnp_adc_properties *,
		const struct qpnp_vadc_chan_properties *,
		struct qpnp_vadc_result *);
};

/**
 * struct qpnp_iadc_calib - IADC channel calibration structure.
 * @channel - Channel for which the historical offset and gain is
 *	      calculated. Available channels are internal rsense,
 *	      external rsense and alternate lead pairs.
 * @offset - Offset value for the channel.
 * @gain - Gain of the channel.
 */
struct qpnp_iadc_calib {
	enum qpnp_iadc_channels		channel;
	int32_t				offset;
	int32_t				gain;
};

/**
 * struct qpnp_adc_drv - QPNP ADC device structure.
 * @spmi - spmi device for ADC peripheral.
 * @offset - base offset for the ADC peripheral.
 * @adc_prop - ADC properties specific to the ADC peripheral.
 * @amux_prop - AMUX properties representing the ADC peripheral.
 * @adc_channels - ADC channel properties for the ADC peripheral.
 * @adc_irq - IRQ number that is mapped to the ADC peripheral.
 * @adc_lock - ADC lock for access to the peripheral.
 * @adc_rslt_completion - ADC result notification after interrupt
 *			  is received.
 * @calib - Internal rsens calibration values for gain and offset.
 */
struct qpnp_adc_drv {
	struct spmi_device		*spmi;
	uint8_t				slave;
	uint16_t			offset;
	struct qpnp_adc_properties	*adc_prop;
	struct qpnp_adc_amux_properties	*amux_prop;
	struct qpnp_vadc_amux		*adc_channels;
	int				adc_irq;
	struct mutex			adc_lock;
	struct completion		adc_rslt_completion;
	struct qpnp_iadc_calib		calib;
};

/**
 * struct qpnp_adc_amux_properties - QPNP VADC amux channel property.
 * @amux_channel - Refer to the qpnp_vadc_channel list.
 * @decimation - Sampling rate supported for the channel.
 * @mode_sel - The basic mode of operation.
 * @hw_settle_time - The time between AMUX being configured and the
 *			start of conversion.
 * @fast_avg_setup - Ability to provide single result from the ADC
 *			that is an average of multiple measurements.
 * @trigger_channel - HW trigger channel for conversion sequencer.
 * @chan_prop - Represent the channel properties of the ADC.
 */
struct qpnp_adc_amux_properties {
	uint32_t			amux_channel;
	uint32_t			decimation;
	uint32_t			mode_sel;
	uint32_t			hw_settle_time;
	uint32_t			fast_avg_setup;
	enum qpnp_vadc_trigger		trigger_channel;
	struct qpnp_vadc_chan_properties	chan_prop[0];
};

/* Public API */
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE_MODULE)
/**
 * qpnp_vadc_read() - Performs ADC read on the channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t qpnp_vadc_read(enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result);

/**
 * qpnp_vadc_conv_seq_request() - Performs ADC read on the conversion
 *				sequencer channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t qpnp_vadc_conv_seq_request(
			enum qpnp_vadc_trigger trigger_channel,
			enum qpnp_vadc_channels channel,
			struct qpnp_vadc_result *result);

/**
 * qpnp_vadc_check_result() - Performs check on the ADC raw code.
 * @data:	Data used for verifying the range of the ADC code.
 */
int32_t qpnp_vadc_check_result(int32_t *data);

/**
 * qpnp_adc_get_devicetree_data() - Abstracts the ADC devicetree data.
 * @spmi:	spmi ADC device.
 * @adc_qpnp:	spmi device tree node structure
 */
int32_t qpnp_adc_get_devicetree_data(struct spmi_device *spmi,
					struct qpnp_adc_drv *adc_qpnp);

/**
 * qpnp_vadc_configure() - Configure ADC device to start conversion.
 * @chan_prop:	Individual channel properties for the AMUX channel.
 */
int32_t qpnp_vadc_configure(
			struct qpnp_adc_amux_properties *chan_prop);

/**
 * qpnp_adc_scale_default() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the qpnp adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	Individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	Physical result to be stored.
 */
int32_t qpnp_adc_scale_default(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_pmic_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Performs the AMUX out as 2mV/K and returns
 *		the temperature in milli degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the qpnp adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	Individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	Physical result to be stored.
 */
int32_t qpnp_adc_scale_pmic_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_batt_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature in degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_batt_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_batt_id() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_batt_id(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_tdkntcg_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature of the xo therm in mili
		degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_tdkntcg_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_vadc_is_ready() - Clients can use this API to check if the
 *			  device is ready to use.
 * @result:	0 on success and -EPROBE_DEFER when probe for the device
 *		has not occured.
 */
int32_t qpnp_vadc_is_ready(void);
#else
static inline int32_t qpnp_vadc_read(uint32_t channel,
				struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_conv_seq_request(
			enum qpnp_vadc_trigger trigger_channel,
			enum qpnp_vadc_channels channel,
			struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_default(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_pmic_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_id(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tdkntcg_therm(int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_is_read(void)
{ return -ENXIO; }
#endif

/* Public API */
#if defined(CONFIG_SENSORS_QPNP_ADC_CURRENT)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_CURRENT_MODULE)
/**
 * qpnp_iadc_read() - Performs ADC read on the current channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Current across rsens in mV.
 */
int32_t qpnp_iadc_read(enum qpnp_iadc_channels channel,
							int32_t *result);
/**
 * qpnp_iadc_get_gain() - Performs gain calibration over 25mV reference
 *			  across CCADC.
 * @result:	Gain result across 25mV reference.
 */
int32_t qpnp_iadc_get_gain(int32_t *result);

/**
 * qpnp_iadc_get_offset() - Performs offset calibration over selected
 *			    channel. Channel can be internal rsense,
 *			    external rsense and alternate lead pair.
 * @result:	Gain result across 25mV reference.
 */
int32_t qpnp_iadc_get_offset(enum qpnp_iadc_channels channel,
						int32_t *result);
/**
 * qpnp_iadc_is_ready() - Clients can use this API to check if the
 *			  device is ready to use.
 * @result:	0 on success and -EPROBE_DEFER when probe for the device
 *		has not occured.
 */
int32_t qpnp_iadc_is_ready(void);
#else
static inline int32_t qpnp_iadc_read(enum qpnp_iadc_channels channel,
							int *result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_gain(int32_t *result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_offset(enum qpnp_iadc_channels channel,
						int32_t *result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_is_read(void)
{ return -ENXIO; }
#endif

#endif
