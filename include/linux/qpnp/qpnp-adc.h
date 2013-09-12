/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/qpnp-revid.h>
/**
 * enum qpnp_vadc_channels - QPNP AMUX arbiter channels
 */
enum qpnp_vadc_channels {
	USBIN = 0,
	DCIN,
	VCHG_SNS,
	SPARE1_03,
	USB_ID_MV,
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
	LR_MUX10_USB_ID_LV,
	AMUX_PU1,
	AMUX_PU2,
	LR_MUX3_BUF_XO_THERM_BUF,
	LR_MUX1_PU1_BAT_THERM = 112,
	LR_MUX2_PU1_BAT_ID = 113,
	LR_MUX3_PU1_XO_THERM = 114,
	LR_MUX4_PU1_AMUX_THM1 = 115,
	LR_MUX5_PU1_AMUX_THM2 = 116,
	LR_MUX6_PU1_AMUX_THM3 = 117,
	LR_MUX7_PU1_AMUX_HW_ID = 118,
	LR_MUX8_PU1_AMUX_THM4 = 119,
	LR_MUX9_PU1_AMUX_THM5 = 120,
	LR_MUX10_PU1_AMUX_USB_ID_LV = 121,
	LR_MUX3_BUF_PU1_XO_THERM_BUF = 124,
	LR_MUX1_PU2_BAT_THERM = 176,
	LR_MUX2_PU2_BAT_ID = 177,
	LR_MUX3_PU2_XO_THERM = 178,
	LR_MUX4_PU2_AMUX_THM1 = 179,
	LR_MUX5_PU2_AMUX_THM2 = 180,
	LR_MUX6_PU2_AMUX_THM3 = 181,
	LR_MUX7_PU2_AMUX_HW_ID = 182,
	LR_MUX8_PU2_AMUX_THM4 = 183,
	LR_MUX9_PU2_AMUX_THM5 = 184,
	LR_MUX10_PU2_AMUX_USB_ID_LV = 185,
	LR_MUX3_BUF_PU2_XO_THERM_BUF = 188,
	LR_MUX1_PU1_PU2_BAT_THERM = 240,
	LR_MUX2_PU1_PU2_BAT_ID = 241,
	LR_MUX3_PU1_PU2_XO_THERM = 242,
	LR_MUX4_PU1_PU2_AMUX_THM1 = 243,
	LR_MUX5_PU1_PU2_AMUX_THM2 = 244,
	LR_MUX6_PU1_PU2_AMUX_THM3 = 245,
	LR_MUX7_PU1_PU2_AMUX_HW_ID = 246,
	LR_MUX8_PU1_PU2_AMUX_THM4 = 247,
	LR_MUX9_PU1_PU2_AMUX_THM5 = 248,
	LR_MUX10_PU1_PU2_AMUX_USB_ID_LV = 249,
	LR_MUX3_BUF_PU1_PU2_XO_THERM_BUF = 252,
	ALL_OFF = 255,
	ADC_MAX_NUM,
};

/**
 * enum qpnp_iadc_channels - QPNP IADC channel list
 */
enum qpnp_iadc_channels {
	INTERNAL_RSENSE = 0,
	EXTERNAL_RSENSE,
	ALT_LEAD_PAIR,
	GAIN_CALIBRATION_17P857MV,
	OFFSET_CALIBRATION_SHORT_CADC_LEADS,
	OFFSET_CALIBRATION_CSP_CSN,
	OFFSET_CALIBRATION_CSP2_CSN2,
	IADC_MUX_NUM,
};

#define QPNP_ADC_625_UV	625000
#define QPNP_ADC_HWMON_NAME_LENGTH				64
#define QPNP_MAX_PROP_NAME_LEN					32

/* Structure device for qpnp vadc */
struct qpnp_vadc_chip;

/* Structure device for qpnp iadc */
struct qpnp_iadc_chip;

/* Structure device for qpnp adc tm */
struct qpnp_adc_tm_chip;

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
 * enum qpnp_adc_scale_fn_type - Scaling function for pm8941 pre calibrated
 *				   digital data relative to ADC reference.
 * %SCALE_DEFAULT: Default scaling to convert raw adc code to voltage (uV).
 * %SCALE_BATT_THERM: Conversion to temperature(decidegC) based on btm
 *			parameters.
 * %SCALE_THERM_100K_PULLUP: Returns temperature in degC.
 *				 Uses a mapping table with 100K pullup.
 * %SCALE_PMIC_THERM: Returns result in milli degree's Centigrade.
 * %SCALE_XOTHERM: Returns XO thermistor voltage in degree's Centigrade.
 * %SCALE_THERM_150K_PULLUP: Returns temperature in degC.
 *				 Uses a mapping table with 150K pullup.
 * %SCALE_QRD_BATT_THERM: Conversion to temperature(decidegC) based on
 *			btm parameters.
 * %SCALE_QRD_SKUAA_BATT_THERM: Conversion to temperature(decidegC) based on
 *          btm parametersi for SKUAA.
 * %SCALE_NONE: Do not use this scaling type.
 */
enum qpnp_adc_scale_fn_type {
	SCALE_DEFAULT = 0,
	SCALE_BATT_THERM,
	SCALE_THERM_100K_PULLUP,
	SCALE_PMIC_THERM,
	SCALE_XOTHERM,
	SCALE_THERM_150K_PULLUP,
	SCALE_QRD_BATT_THERM,
	SCALE_QRD_SKUAA_BATT_THERM,
	SCALE_NONE,
};


/**
 * enum qpnp_adc_tm_rscale_fn_type - Scaling function used to convert the
 *	channels input voltage/temperature to corresponding ADC code that is
 *	applied for thresholds. Check the corresponding channels scaling to
 *	determine the appropriate temperature/voltage units that are passed
 *	to the scaling function. Example battery follows the power supply
 *	framework that needs its units to be in decidegreesC so it passes
 *	deci-degreesC. PA_THERM clients pass the temperature in degrees.
 *	The order below should match the one in the driver for
 *	adc_tm_rscale_fn[].
 */
enum qpnp_adc_tm_rscale_fn_type {
	SCALE_R_VBATT = 0,
	SCALE_RBATT_THERM,
	SCALE_R_USB_ID,
	SCALE_RPMIC_THERM,
	SCALE_RSCALE_NONE,
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
 * enum qpnp_adc_meas_timer_1 - Selects the measurement interval time.
 *		If value = 0, use 0ms else use 2^(value + 4)/ 32768).
 * The timer period is used by the USB_ID. Do not set a polling rate
 * greater than 1 second on PMIC 2.0. The max polling rate on the PMIC 2.0
 * appears to be limited to 1 second.
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
enum qpnp_adc_meas_timer_1 {
	ADC_MEAS1_INTERVAL_0MS = 0,
	ADC_MEAS1_INTERVAL_1P0MS,
	ADC_MEAS1_INTERVAL_2P0MS,
	ADC_MEAS1_INTERVAL_3P9MS,
	ADC_MEAS1_INTERVAL_7P8MS,
	ADC_MEAS1_INTERVAL_15P6MS,
	ADC_MEAS1_INTERVAL_31P3MS,
	ADC_MEAS1_INTERVAL_62P5MS,
	ADC_MEAS1_INTERVAL_125MS,
	ADC_MEAS1_INTERVAL_250MS,
	ADC_MEAS1_INTERVAL_500MS,
	ADC_MEAS1_INTERVAL_1S,
	ADC_MEAS1_INTERVAL_2S,
	ADC_MEAS1_INTERVAL_4S,
	ADC_MEAS1_INTERVAL_8S,
	ADC_MEAS1_INTERVAL_16S,
	ADC_MEAS1_INTERVAL_NONE,
};

/**
 * enum qpnp_adc_meas_timer_2 - Selects the measurement interval time.
 *		If value = 0, use 0ms else use 2^(value + 4)/ 32768).
 * The timer period is used by the batt_therm. Do not set a polling rate
 * greater than 1 second on PMIC 2.0. The max polling rate on the PMIC 2.0
 * appears to be limited to 1 second.
 * %ADC_MEAS_INTERVAL_0MS : 0ms
 * %ADC_MEAS_INTERVAL_100MS : 100ms
 * %ADC_MEAS_INTERVAL_200MS : 200ms
 * %ADC_MEAS_INTERVAL_300MS : 300ms
 * %ADC_MEAS_INTERVAL_400MS : 400ms
 * %ADC_MEAS_INTERVAL_500MS : 500ms
 * %ADC_MEAS_INTERVAL_600MS : 600ms
 * %ADC_MEAS_INTERVAL_700MS : 700ms
 * %ADC_MEAS_INTERVAL_800MS : 800ms
 * %ADC_MEAS_INTERVAL_900MS : 900ms
 * %ADC_MEAS_INTERVAL_1S: 1seconds
 * %ADC_MEAS_INTERVAL_1P1S: 1.1seconds
 * %ADC_MEAS_INTERVAL_1P2S: 1.2seconds
 * %ADC_MEAS_INTERVAL_1P3S: 1.3seconds
 * %ADC_MEAS_INTERVAL_1P4S: 1.4seconds
 * %ADC_MEAS_INTERVAL_1P5S: 1.5seconds
 */
enum qpnp_adc_meas_timer_2 {
	ADC_MEAS2_INTERVAL_0MS = 0,
	ADC_MEAS2_INTERVAL_100MS,
	ADC_MEAS2_INTERVAL_200MS,
	ADC_MEAS2_INTERVAL_300MS,
	ADC_MEAS2_INTERVAL_400MS,
	ADC_MEAS2_INTERVAL_500MS,
	ADC_MEAS2_INTERVAL_600MS,
	ADC_MEAS2_INTERVAL_700MS,
	ADC_MEAS2_INTERVAL_800MS,
	ADC_MEAS2_INTERVAL_900MS,
	ADC_MEAS2_INTERVAL_1S,
	ADC_MEAS2_INTERVAL_1P1S,
	ADC_MEAS2_INTERVAL_1P2S,
	ADC_MEAS2_INTERVAL_1P3S,
	ADC_MEAS2_INTERVAL_1P4S,
	ADC_MEAS2_INTERVAL_1P5S,
	ADC_MEAS2_INTERVAL_NONE,
};

/**
 * enum qpnp_adc_meas_timer_3 - Selects the measurement interval time.
 *		If value = 0, use 0ms else use 2^(value + 4)/ 32768).
 * Do not set a polling rate greater than 1 second on PMIC 2.0.
 * The max polling rate on the PMIC 2.0 appears to be limited to 1 second.
 * %ADC_MEAS_INTERVAL_0MS : 0ms
 * %ADC_MEAS_INTERVAL_1S : 1seconds
 * %ADC_MEAS_INTERVAL_2S : 2seconds
 * %ADC_MEAS_INTERVAL_3S : 3seconds
 * %ADC_MEAS_INTERVAL_4S : 4seconds
 * %ADC_MEAS_INTERVAL_5S : 5seconds
 * %ADC_MEAS_INTERVAL_6S: 6seconds
 * %ADC_MEAS_INTERVAL_7S : 7seconds
 * %ADC_MEAS_INTERVAL_8S : 8seconds
 * %ADC_MEAS_INTERVAL_9S : 9seconds
 * %ADC_MEAS_INTERVAL_10S : 10seconds
 * %ADC_MEAS_INTERVAL_11S : 11seconds
 * %ADC_MEAS_INTERVAL_12S : 12seconds
 * %ADC_MEAS_INTERVAL_13S : 13seconds
 * %ADC_MEAS_INTERVAL_14S : 14seconds
 * %ADC_MEAS_INTERVAL_15S : 15seconds
 */
enum qpnp_adc_meas_timer_3 {
	ADC_MEAS3_INTERVAL_0S = 0,
	ADC_MEAS3_INTERVAL_1S,
	ADC_MEAS3_INTERVAL_2S,
	ADC_MEAS3_INTERVAL_3S,
	ADC_MEAS3_INTERVAL_4S,
	ADC_MEAS3_INTERVAL_5S,
	ADC_MEAS3_INTERVAL_6S,
	ADC_MEAS3_INTERVAL_7S,
	ADC_MEAS3_INTERVAL_8S,
	ADC_MEAS3_INTERVAL_9S,
	ADC_MEAS3_INTERVAL_10S,
	ADC_MEAS3_INTERVAL_11S,
	ADC_MEAS3_INTERVAL_12S,
	ADC_MEAS3_INTERVAL_13S,
	ADC_MEAS3_INTERVAL_14S,
	ADC_MEAS3_INTERVAL_15S,
	ADC_MEAS3_INTERVAL_NONE,
};

/**
 * enum qpnp_adc_meas_timer_select - Selects the timer for which
 *	the appropriate polling frequency is set.
 * %ADC_MEAS_TIMER_SELECT1 - Select this timer if the client is USB_ID.
 * %ADC_MEAS_TIMER_SELECT2 - Select this timer if the client is batt_therm.
 * %ADC_MEAS_TIMER_SELECT3 - The timer is added only for completion. It is
 *	not used by kernel space clients and user space clients cannot set
 *	the polling frequency. The driver will set a appropriate polling
 *	frequency to measure the user space clients from qpnp_adc_meas_timer_3.
 */
enum qpnp_adc_meas_timer_select {
	ADC_MEAS_TIMER_SELECT1 = 0,
	ADC_MEAS_TIMER_SELECT2,
	ADC_MEAS_TIMER_SELECT3,
	ADC_MEAS_TIMER_NUM,
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
 * Channel selection registers for each of the configurable measurements
 * Channels allotment is set at device config for a channel.
 * The USB_ID, BATT_THERM, PMIC_THERM and VBAT channels are used by the
 * kernel space USB, Battery and IADC drivers.
 * The other 3 channels are configurable for use by userspace clients.
 */
enum qpnp_adc_tm_channel_select	{
	QPNP_ADC_TM_M0_ADC_CH_SEL_CTL = 0x48,
	QPNP_ADC_TM_M1_ADC_CH_SEL_CTL = 0x68,
	QPNP_ADC_TM_M2_ADC_CH_SEL_CTL = 0x70,
	QPNP_ADC_TM_M3_ADC_CH_SEL_CTL = 0x78,
	QPNP_ADC_TM_M4_ADC_CH_SEL_CTL = 0x80,
	QPNP_ADC_TM_M5_ADC_CH_SEL_CTL = 0x88,
	QPNP_ADC_TM_M6_ADC_CH_SEL_CTL = 0x90,
	QPNP_ADC_TM_M7_ADC_CH_SEL_CTL = 0x98,
	QPNP_ADC_TM_CH_SELECT_NONE
};

/**
 * Channel index for the corresponding index to qpnp_adc_tm_channel_selec
 */
enum qpnp_adc_tm_channel_num {
	QPNP_ADC_TM_CHAN0 = 0,
	QPNP_ADC_TM_CHAN1,
	QPNP_ADC_TM_CHAN2,
	QPNP_ADC_TM_CHAN3,
	QPNP_ADC_TM_CHAN4,
	QPNP_ADC_TM_CHAN5,
	QPNP_ADC_TM_CHAN6,
	QPNP_ADC_TM_CHAN7,
	QPNP_ADC_TM_CHAN_NONE
};

enum qpnp_comp_scheme_type {
	COMP_ID_GF = 0,
	COMP_ID_SMIC,
	COMP_ID_TSMC,
	COMP_ID_NUM,
};

/**
 * struct qpnp_adc_tm_config - Represent ADC Thermal Monitor configuration.
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
struct qpnp_adc_tm_config {
	int	channel;
	int	adc_code;
	int	high_thr_temp;
	int	low_thr_temp;
	int64_t	high_thr_voltage;
	int64_t	low_thr_voltage;
};

/**
 * enum qpnp_adc_tm_trip_type - Type for setting high/low temperature/voltage.
 * %ADC_TM_TRIP_HIGH_WARM: Setting high temperature. Note that high temperature
 *			corresponds to low voltage. Driver handles this case
 *			appropriately to set high/low thresholds for voltage.
 *			threshold.
 * %ADC_TM_TRIP_LOW_COOL: Setting low temperature.
 */
enum qpnp_adc_tm_trip_type {
	ADC_TM_TRIP_HIGH_WARM = 0,
	ADC_TM_TRIP_LOW_COOL,
	ADC_TM_TRIP_NUM,
};

#define ADC_TM_WRITABLE_TRIPS_MASK ((1 << ADC_TM_TRIP_NUM) - 1)

/**
 * enum qpnp_tm_state - This lets the client know whether the threshold
 *		that was crossed was high/low.
 * %ADC_TM_HIGH_STATE: Client is notified of crossing the requested high
 *			voltage threshold.
 * %ADC_TM_COOL_STATE: Client is notified of crossing the requested cool
 *			temperature threshold.
 * %ADC_TM_LOW_STATE: Client is notified of crossing the requested low
 *			voltage threshold.
 * %ADC_TM_WARM_STATE: Client is notified of crossing the requested high
 *			temperature threshold.
 */
enum qpnp_tm_state {
	ADC_TM_HIGH_STATE = 0,
	ADC_TM_COOL_STATE = ADC_TM_HIGH_STATE,
	ADC_TM_LOW_STATE,
	ADC_TM_WARM_STATE = ADC_TM_LOW_STATE,
	ADC_TM_STATE_NUM,
};

/**
 * enum qpnp_state_request - Request to enable/disable the corresponding
 *			high/low voltage/temperature thresholds.
 * %ADC_TM_HIGH_THR_ENABLE: Enable high voltage threshold.
 * %ADC_TM_COOL_THR_ENABLE = Enables cool temperature threshold.
 * %ADC_TM_LOW_THR_ENABLE: Enable low voltage/temperature threshold.
 * %ADC_TM_WARM_THR_ENABLE = Enables warm temperature threshold.
 * %ADC_TM_HIGH_LOW_THR_ENABLE: Enable high and low voltage/temperature
 *				threshold.
 * %ADC_TM_HIGH_THR_DISABLE: Disable high voltage/temperature threshold.
 * %ADC_TM_COOL_THR_ENABLE = Disables cool temperature threshold.
 * %ADC_TM_LOW_THR_DISABLE: Disable low voltage/temperature threshold.
 * %ADC_TM_WARM_THR_ENABLE = Disables warm temperature threshold.
 * %ADC_TM_HIGH_THR_DISABLE: Disable high and low voltage/temperature
 *				threshold.
 */
enum qpnp_state_request {
	ADC_TM_HIGH_THR_ENABLE = 0,
	ADC_TM_COOL_THR_ENABLE = ADC_TM_HIGH_THR_ENABLE,
	ADC_TM_LOW_THR_ENABLE,
	ADC_TM_WARM_THR_ENABLE = ADC_TM_LOW_THR_ENABLE,
	ADC_TM_HIGH_LOW_THR_ENABLE,
	ADC_TM_HIGH_THR_DISABLE,
	ADC_TM_COOL_THR_DISABLE = ADC_TM_HIGH_THR_DISABLE,
	ADC_TM_LOW_THR_DISABLE,
	ADC_TM_WARM_THR_DISABLE = ADC_TM_LOW_THR_DISABLE,
	ADC_TM_HIGH_LOW_THR_DISABLE,
	ADC_TM_THR_NUM,
};

/**
 * struct qpnp_adc_tm_btm_param - Represent Battery temperature threshold
 *				monitoring configuration.
 * @high_temp: High temperature threshold for which notification is requested.
 * @low_temp: Low temperature threshold for which notification is requested.
 * @high_thr_voltage: High voltage for which notification is requested.
 * @low_thr_voltage: Low voltage for which notification is requested.
 * @state_request: Enable/disable the corresponding high and low temperature
 *		thresholds.
 * @timer_interval1: Select polling rate from qpnp_adc_meas_timer_1 type.
 * @timer_interval2: Select polling rate from qpnp_adc_meas_timer_2 type.
 * @timer_interval3: Select polling rate from qpnp_adc_meas_timer_3 type.
 * @btmid_ctx: A context of void type.
 * @threshold_notification: Notification callback once threshold are crossed.
 * units to be used for High/Low temperature and voltage notification -
 * This depends on the clients usage. Check the rscaling function
 * for the appropriate channel nodes.
 * @Batt therm clients temperature units is decidegreesCentigrate.
 * @USB_ID inputs the voltage units in milli-volts.
 * @PA_THERM inputs the units in degC.
 * @PMIC_THERM inputs the units in millidegC.
 */
struct qpnp_adc_tm_btm_param {
	int32_t					high_temp;
	int32_t					low_temp;
	int32_t					high_thr;
	int32_t					low_thr;
	enum qpnp_vadc_channels			channel;
	enum qpnp_state_request			state_request;
	enum qpnp_adc_meas_timer_1		timer_interval;
	enum qpnp_adc_meas_timer_2		timer_interval2;
	enum qpnp_adc_meas_timer_3		timer_interval3;
	void					*btm_ctx;
	void	(*threshold_notification) (enum qpnp_tm_state state,
						void *ctx);
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
 * @high_thr: High threshold voltage that is requested to be set.
 * @low_thr: Low threshold voltage that is requested to be set.
 * @timer_select: Choosen from one of the 3 timers to set the polling rate for
 *		  the VADC_BTM channel.
 * @meas_interval1: Polling rate to set for timer 1.
 * @meas_interval2: Polling rate to set for timer 2.
 * @tm_channel_select: BTM channel number for the 5 VADC_BTM channels.
 * @state_request: User can select either enable or disable high/low or both
 * activation levels based on the qpnp_state_request type.
 * @adc_graph: ADC graph for the channel of struct type qpnp_adc_linear_graph.
 */
struct qpnp_vadc_chan_properties {
	uint32_t			offset_gain_numerator;
	uint32_t			offset_gain_denominator;
	uint32_t				high_thr;
	uint32_t				low_thr;
	enum qpnp_adc_meas_timer_select		timer_select;
	enum qpnp_adc_meas_timer_1		meas_interval1;
	enum qpnp_adc_meas_timer_2		meas_interval2;
	enum qpnp_adc_tm_channel_select		tm_channel_select;
	enum qpnp_state_request			state_request;
	struct qpnp_vadc_linear_graph	adc_graph[2];
};

/**
 * struct qpnp_vadc_result - Represent the result of the QPNP ADC.
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
struct qpnp_adc_amux {
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
	int32_t (*chan) (struct qpnp_vadc_chip *, int32_t,
		const struct qpnp_adc_properties *,
		const struct qpnp_vadc_chan_properties *,
		struct qpnp_vadc_result *);
};

/**
 * struct qpnp_adc_tm_reverse_scale_fn - Reverse scaling prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties, channel properties,
 *	and returns the physical result
 */
struct qpnp_adc_tm_reverse_scale_fn {
	int32_t (*chan) (struct qpnp_vadc_chip *,
		struct qpnp_adc_tm_btm_param *,
		uint32_t *, uint32_t *);
};

/**
 * struct qpnp_iadc_calib - IADC channel calibration structure.
 * @channel - Channel for which the historical offset and gain is
 *	      calculated. Available channels are internal rsense,
 *	      external rsense and alternate lead pairs.
 * @offset_raw - raw Offset value for the channel.
 * @gain_raw - raw Gain of the channel.
 * @ideal_offset_uv - ideal offset value for the channel.
 * @ideal_gain_nv - ideal gain for the channel.
 * @offset_uv - converted value of offset in uV.
 * @gain_uv - converted value of gain in uV.
 */
struct qpnp_iadc_calib {
	enum qpnp_iadc_channels		channel;
	uint16_t			offset_raw;
	uint16_t			gain_raw;
	uint32_t			ideal_offset_uv;
	uint32_t			ideal_gain_nv;
	uint32_t			offset_uv;
	uint32_t			gain_uv;
};

/**
 * struct qpnp_iadc_result - IADC read result structure.
 * @oresult_uv - Result of ADC in uV.
 * @result_ua - Result of ADC in uA.
 */
struct qpnp_iadc_result {
	int32_t				result_uv;
	int32_t				result_ua;
};

/**
 * struct qpnp_adc_drv - QPNP ADC device structure.
 * @spmi - spmi device for ADC peripheral.
 * @offset - base offset for the ADC peripheral.
 * @adc_prop - ADC properties specific to the ADC peripheral.
 * @amux_prop - AMUX properties representing the ADC peripheral.
 * @adc_channels - ADC channel properties for the ADC peripheral.
 * @adc_irq_eoc - End of Conversion IRQ.
 * @adc_irq_fifo_not_empty - Conversion sequencer request written
 *			to FIFO when not empty.
 * @adc_irq_conv_seq_timeout - Conversion sequencer trigger timeout.
 * @adc_high_thr_irq - Output higher than high threshold set for measurement.
 * @adc_low_thr_irq - Output lower than low threshold set for measurement.
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
	struct qpnp_adc_amux		*adc_channels;
	int				adc_irq_eoc;
	int				adc_irq_fifo_not_empty;
	int				adc_irq_conv_seq_timeout;
	int				adc_high_thr_irq;
	int				adc_low_thr_irq;
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
	uint32_t				amux_channel;
	uint32_t				decimation;
	uint32_t				mode_sel;
	uint32_t				hw_settle_time;
	uint32_t				fast_avg_setup;
	enum qpnp_vadc_trigger			trigger_channel;
	struct qpnp_vadc_chan_properties	chan_prop[0];
};

/* SW index's for PMIC type and version used by QPNP VADC and IADC */
#define QPNP_REV_ID_8941_3_1	1
#define QPNP_REV_ID_8026_1_0	2
#define QPNP_REV_ID_8026_2_0	3
#define QPNP_REV_ID_8110_1_0	4
#define QPNP_REV_ID_8026_2_1	5

/* Public API */
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE_MODULE)
/**
 * qpnp_vadc_read() - Performs ADC read on the channel.
 * @dev:	Structure device for qpnp vadc
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t qpnp_vadc_read(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result);

/**
 * qpnp_vadc_conv_seq_request() - Performs ADC read on the conversion
 *				sequencer channel.
 * @dev:	Structure device for qpnp vadc
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t qpnp_vadc_conv_seq_request(struct qpnp_vadc_chip *dev,
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
 * qpnp_adc_scale_default() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the qpnp adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	Individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	Physical result to be stored.
 */
int32_t qpnp_adc_scale_default(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_pmic_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Performs the AMUX out as 2mV/K and returns
 *		the temperature in milli degC.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the qpnp adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	Individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	Physical result to be stored.
 */
int32_t qpnp_adc_scale_pmic_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_batt_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature in decidegC.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_qrd_batt_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature in decidegC.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_qrd_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_qrd_skuaa_batt_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature in decidegC.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_qrd_skuaa_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_batt_id() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_batt_id(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_tdkntcg_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature of the xo therm in mili
		degC.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_tdkntcg_therm(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_therm_pu1() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature of the therm in degC.
 *		It uses a mapping table computed for a 150K pull-up.
 *		Pull-up1 is an internal pull-up on the AMUX of 150K.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_therm_pu1(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_adc_scale_therm_pu2() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature of the therm in degC.
 *		It uses a mapping table computed for a 100K pull-up.
 *		Pull-up2 is an internal pull-up on the AMUX of 100K.
 * @dev:	Structure device for qpnp vadc
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8xxx adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t qpnp_adc_scale_therm_pu2(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
/**
 * qpnp_get_vadc() - Clients need to register with the vadc using the
 *		corresponding device instance it wants to read the channels
 *		from. Read the bindings document on how to pass the phandle
 *		for the corresponding vadc driver to register with.
 * @dev:	Clients device structure
 * @name:	Corresponding client's DT parser name. Read the DT bindings
 *		document on how to register with the vadc
 * @struct qpnp_vadc_chip * - On success returns the vadc device structure
 *		pointer that needs to be used during an ADC request.
 */
struct qpnp_vadc_chip *qpnp_get_vadc(struct device *dev, const char *name);
/**
 * qpnp_adc_tm_scaler() - Performs reverse calibration.
 * @config:	Thermal monitoring configuration.
 * @adc_prop:	adc properties of the qpnp adc such as bit resolution and
 *		reference voltage.
 * @chan_prop:	Individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 */
static inline int32_t qpnp_adc_tm_scaler(struct qpnp_adc_tm_config *tm_config,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop)
{ return -ENXIO; }
/**
 * qpnp_get_vadc_gain_and_offset() - Obtains the VADC gain and offset
 *		for absolute and ratiometric calibration.
 * @dev:	Structure device for qpnp vadc
 * @param:	The result in which the ADC offset and gain values are stored.
 * @type:	The calibration type whether client needs the absolute or
 *		ratiometric gain and offset values.
 */
int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_vadc_chip *dev,
			struct qpnp_vadc_linear_graph *param,
			enum qpnp_adc_calib_type calib_type);
/**
 * qpnp_adc_scale_millidegc_pmic_voltage_thr() - Performs reverse calibration
 *		on the low/high temperature threshold values passed by the
 *		client. The function coverts milldegC to voltage threshold
 *		and accounts for the corresponding channels scaling as (2mV/K).
 * @dev:	Structure device for qpnp vadc
 * @param:	The input parameters that contain the low/high temperature
 *		values.
 * @low_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 * @high_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 */
int32_t qpnp_adc_scale_millidegc_pmic_voltage_thr(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
/**
 * qpnp_adc_btm_scaler() - Performs reverse calibration on the low/high
 *		temperature threshold values passed by the client.
 *		The function maps the temperature to voltage and applies
 *		ratiometric calibration on the voltage values.
 * @dev:	Structure device for qpnp vadc
 * @param:	The input parameters that contain the low/high temperature
 *		values.
 * @low_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 * @high_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 */
int32_t qpnp_adc_btm_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
/**
 * qpnp_adc_tm_scale_therm_voltage_pu2() - Performs reverse calibration
 *		and convert given temperature to voltage on supported
 *		thermistor channels using 100k pull-up.
 * @dev:	Structure device for qpnp vadc
 * @param:	The input temperature values.
 */
int32_t qpnp_adc_tm_scale_therm_voltage_pu2(struct qpnp_vadc_chip *dev,
				struct qpnp_adc_tm_config *param);
/**
 * qpnp_adc_tm_scale_therm_voltage_pu2() - Performs reverse calibration
 *		and converts the given ADC code to temperature for
 *		thermistor channels using 100k pull-up.
 * @dev:	Structure device for qpnp vadc
 * @reg:	The input ADC code.
 * @result:	The physical measurement temperature on the thermistor.
 */
int32_t qpnp_adc_tm_scale_voltage_therm_pu2(struct qpnp_vadc_chip *dev,
				uint32_t reg, int64_t *result);
/**
 * qpnp_adc_usb_scaler() - Performs reverse calibration on the low/high
 *		voltage threshold values passed by the client.
 *		The function applies ratiometric calibration on the
 *		voltage values.
 * @dev:	Structure device for qpnp vadc
 * @param:	The input parameters that contain the low/high voltage
 *		threshold values.
 * @low_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 * @high_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 */
int32_t qpnp_adc_usb_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
/**
 * qpnp_adc_vbatt_rscaler() - Performs reverse calibration on the low/high
 *		voltage threshold values passed by the client.
 *		The function applies ratiometric calibration on the
 *		voltage values.
 * @dev:	Structure device for qpnp vadc
 * @param:	The input parameters that contain the low/high voltage
 *		threshold values.
 * @low_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 * @high_threshold: The low threshold value that needs to be updated with
 *		the above calibrated voltage value.
 */
int32_t qpnp_adc_vbatt_rscaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
/**
 * qpnp_vadc_iadc_sync_request() - Performs Voltage ADC read and
 *		locks the peripheral. When performing simultaneous
 *		voltage and current request the VADC peripheral is
 *		prepared for conversion and the IADC sync conversion
 *		is done from the IADC peripheral.
 * @dev:	Structure device for qpnp vadc
 * @channel:	Input channel to perform the voltage ADC read.
 */
int32_t qpnp_vadc_iadc_sync_request(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel);
/**
 * qpnp_vadc_iadc_sync_complete_request() - Reads the ADC result and
 *		unlocks the peripheral.
 * @dev:	Structure device for qpnp vadc
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t qpnp_vadc_iadc_sync_complete_request(struct qpnp_vadc_chip *dev,
	enum qpnp_vadc_channels channel, struct qpnp_vadc_result *result);
/**
 * qpnp_vadc_sns_comp_result() - Compensate vbatt readings based on temperature
 * @dev:	Structure device for qpnp vadc
 * @result:	Voltage in uV that needs compensation.
 */
int32_t qpnp_vbat_sns_comp_result(struct qpnp_vadc_chip *dev,
						int64_t *result);
/**
 * qpnp_adc_get_revid_version() - Obtain the PMIC number and revision.
 * @dev:	Structure device node.
 * returns internal mapped PMIC number and revision id.
 */
int qpnp_adc_get_revid_version(struct device *dev);
#else
static inline int32_t qpnp_vadc_read(struct qpnp_vadc_chip *dev,
				uint32_t channel,
				struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_conv_seq_request(struct qpnp_vadc_chip *dev,
			enum qpnp_vadc_trigger trigger_channel,
			enum qpnp_vadc_channels channel,
			struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_default(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_pmic_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_qrd_batt_therm(
			struct qpnp_vadc_chip *vadc, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_qrd_skuaa_batt_therm(
			struct qpnp_vadc_chip *vadc, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_id(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tdkntcg_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_therm_pu1(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_therm_pu2(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline struct qpnp_vadc_chip *qpnp_get_vadc(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
static inline int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_vadc_chip *dev,
			struct qpnp_vadc_linear_graph *param,
			enum qpnp_adc_calib_type calib_type)
{ return -ENXIO; }
static inline int32_t qpnp_adc_usb_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_vbatt_rscaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_btm_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_millidegc_pmic_voltage_thr(
		struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_scale_therm_voltage_pu2(
				struct qpnp_vadc_chip *dev,
				struct qpnp_adc_tm_config *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_scale_voltage_therm_pu2(
				struct qpnp_vadc_chip *dev,
				uint32_t reg, int64_t *result)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_iadc_sync_request(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_iadc_sync_complete_request(
				struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_vbat_sns_comp_result(struct qpnp_vadc_chip *dev,
						int64_t *result)
{ return -ENXIO; }
static inline int qpnp_adc_get_revid_version(struct device *dev)
{ return -ENXIO; }
#endif

/* Public API */
#if defined(CONFIG_SENSORS_QPNP_ADC_CURRENT)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_CURRENT_MODULE)
/**
 * qpnp_iadc_read() - Performs ADC read on the current channel.
 * @dev:	Structure device for qpnp iadc
 * @channel:	Input channel to perform the ADC read.
 * @result:	Current across rsense in mA.
 * @return:	0 on success.
 */
int32_t qpnp_iadc_read(struct qpnp_iadc_chip *dev,
				enum qpnp_iadc_channels channel,
				struct qpnp_iadc_result *result);
/**
 * qpnp_iadc_get_rsense() - Reads the RDS resistance value from the
			trim registers.
 * @dev:	Structure device for qpnp iadc
 * @rsense:	RDS resistance in nOhms.
 * @return:	0 on success.
 */
int32_t qpnp_iadc_get_rsense(struct qpnp_iadc_chip *dev, int32_t *rsense);
/**
 * qpnp_iadc_get_gain_and_offset() - Performs gain calibration
 *				over 17.8571mV and offset over selected
 *				channel. Channel can be internal rsense,
 *				external rsense and alternate lead pair.
 * @dev:	Structure device for qpnp iadc
 * @result:	result structure where the gain and offset is stored of
 *		type qpnp_iadc_calib.
 * @return:	0 on success.
 */
int32_t qpnp_iadc_get_gain_and_offset(struct qpnp_iadc_chip *dev,
					struct qpnp_iadc_calib *result);
/**
 * qpnp_get_iadc() - Clients need to register with the iadc with the
 *		corresponding device instance it wants to read the channels.
 *		Read the bindings document on how to pass the phandle for
 *		the corresponding vadc driver to register with.
 * @dev:	Clients device structure
 * @name:	Corresponding client's DT parser name. Read the DT bindings
 *		document on how to register with the iadc
 * @struct qpnp_iadc_chip * - On success returns the iadc device structure
 *		pointer used everytime client makes an ADC request.
 */
struct qpnp_iadc_chip *qpnp_get_iadc(struct device *dev, const char *name);
/**
 * qpnp_iadc_vadc_sync_read() - Performs synchronous VADC and IADC read.
 *		The api is to be used only by the BMS to perform
 *		simultaneous VADC and IADC measurement for battery voltage
 *		and current.
 * @dev:	Structure device for qpnp iadc
 * @i_channel:	Input battery current channel to perform the IADC read.
 * @i_result:	Current across the rsense in mA.
 * @v_channel:	Input battery voltage channel to perform VADC read.
 * @v_result:	Voltage on the vbatt channel with units in mV.
 * @return:	0 on success.
 */
int32_t qpnp_iadc_vadc_sync_read(struct qpnp_iadc_chip *dev,
	enum qpnp_iadc_channels i_channel, struct qpnp_iadc_result *i_result,
	enum qpnp_vadc_channels v_channel, struct qpnp_vadc_result *v_result);
/**
 * qpnp_iadc_calibrate_for_trim - Clients can use this API to re-calibrate
 *		IADC. The offset and gain values are programmed in the trim
 *		registers. The offset and the gain can be retrieved using
 *		qpnp_iadc_get_gain_and_offset
 * @dev:	Structure device for qpnp iadc
 * @batfet_closed: batfet is opened or closed. The IADC chooses proper
 *			channel (internal/external) based on batfet status
 *			for calibration.
 * RETURNS:	0 on success.
 */
int32_t qpnp_iadc_calibrate_for_trim(struct qpnp_iadc_chip *dev,
						bool batfet_closed);
/**
 * qpnp_iadc_comp_result() - Compensates the result of the current based on
 *		the gain and offset co-effients and rsense parameters.
 * @dev:	Structure device for qpnp iadc
 * @result:	Current value to perform the compensation.
 * @return:	0 on success.
 */
int32_t qpnp_iadc_comp_result(struct qpnp_iadc_chip *dev, int64_t *result);
/**
 * qpnp_iadc_skip_calibration() - Clients can use this API to ask the driver
 *				to skip iadc calibrations
 * @dev:	Structure device for qpnp iadc
 * @result:	0 on success and -EPROBE_DEFER when probe for the device
 *		has not occured.
 */
int qpnp_iadc_skip_calibration(struct qpnp_iadc_chip *dev);
/**
 * qpnp_iadc_resume_calibration() - Clients can use this API to ask the driver
 *				to resume iadc calibrations
 * @dev:	Structure device for qpnp iadc
 * @result:	0 on success and -EPROBE_DEFER when probe for the device
 *		has not occured.
 */
int qpnp_iadc_resume_calibration(struct qpnp_iadc_chip *dev);
#else
static inline int32_t qpnp_iadc_read(struct qpnp_iadc_chip *iadc,
	enum qpnp_iadc_channels channel, struct qpnp_iadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_rsense(struct qpnp_iadc_chip *iadc,
							int32_t *rsense)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_gain_and_offset(struct qpnp_iadc_chip *iadc,
				struct qpnp_iadc_calib *result)
{ return -ENXIO; }
static inline struct qpnp_iadc_chip *qpnp_get_iadc(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
static inline int32_t qpnp_iadc_vadc_sync_read(struct qpnp_iadc_chip *iadc,
	enum qpnp_iadc_channels i_channel, struct qpnp_iadc_result *i_result,
	enum qpnp_vadc_channels v_channel, struct qpnp_vadc_result *v_result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_calibrate_for_trim(struct qpnp_iadc_chip *iadc,
							bool batfet_closed)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_comp_result(struct qpnp_iadc_chip *iadc,
						int64_t *result, int32_t sign)
{ return -ENXIO; }
#endif

/* Public API */
#if defined(CONFIG_THERMAL_QPNP_ADC_TM)				\
			|| defined(CONFIG_THERMAL_QPNP_ADC_TM_MODULE)
/**
 * qpnp_adc_tm_usbid_configure() - Configures Channel 0 of VADC_BTM to
 *		monitor USB_ID channel using 100k internal pull-up.
 *		USB driver passes the high/low voltage threshold along
 *		with the notification callback once the set thresholds
 *		are crossed.
 * @param:	Structure pointer of qpnp_adc_tm_usbid_param type.
 *		Clients pass the low/high voltage along with the threshold
 *		notification callback.
 */
int32_t qpnp_adc_tm_usbid_configure(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
/**
 * qpnp_adc_tm_usbid_end() - Disables the monitoring of channel 0 thats
 *		assigned for monitoring USB_ID. Disables the low/high
 *		threshold activation for channel 0 as well.
 * @param:	none.
 */
int32_t qpnp_adc_tm_usbid_end(struct qpnp_adc_tm_chip *chip);
/**
 * qpnp_adc_tm_channel_measure() - Configures kernel clients a channel to
 *		monitor the corresponding ADC channel for threshold detection.
 *		Driver passes the high/low voltage threshold along
 *		with the notification callback once the set thresholds
 *		are crossed.
 * @param:	Structure pointer of qpnp_adc_tm_btm_param type.
 *		Clients pass the low/high temperature along with the threshold
 *		notification callback.
 */
int32_t qpnp_adc_tm_channel_measure(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
/**
 * qpnp_adc_tm_disable_chan_meas() - Disables the monitoring of channel thats
 *		assigned for monitoring kernel clients. Disables the low/high
 *		threshold activation for the corresponding channel.
 * @param:	Structure pointer of qpnp_adc_tm_btm_param type.
 *		This is used to identify the channel for which the corresponding
 *		channels high/low threshold notification will be disabled.
 */
int32_t qpnp_adc_tm_disable_chan_meas(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
/**
 * qpnp_get_adc_tm() - Clients need to register with the adc_tm using the
 *		corresponding device instance it wants to read the channels
 *		from. Read the bindings document on how to pass the phandle
 *		for the corresponding adc_tm driver to register with.
 * @name:	Corresponding client's DT parser name. Read the DT bindings
 *		document on how to register with the vadc
 * @struct qpnp_adc_tm_chip * - On success returns the vadc device structure
 *		pointer that needs to be used during an ADC TM request.
 */
struct qpnp_adc_tm_chip *qpnp_get_adc_tm(struct device *dev, const char *name);
#else
static inline int32_t qpnp_adc_tm_usbid_configure(
			struct qpnp_adc_tm_chip *chip,
			struct qpnp_adc_tm_btm_param *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_usbid_end(struct qpnp_adc_tm_chip *chip)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_channel_measure(
					struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_disable_chan_meas(
					struct qpnp_adc_tm_chip *chip)
{ return -ENXIO; }
static inline struct qpnp_adc_tm_chip *qpnp_get_adc_tm(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
#endif

#endif
