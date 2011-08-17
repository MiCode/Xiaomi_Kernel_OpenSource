/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm PMIC 8921 ADC driver header file
 *
 */

#ifndef __MFD_PM8921_ADC_H
#define __MFD_PM8921_ADC_H

#include <linux/kernel.h>
#include <linux/list.h>

/**
 * enum pm8921_adc_channels - PM8921 AMUX arbiter channels
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
enum pm8921_adc_channels {
	CHANNEL_VCOIN = 0,
	CHANNEL_VBAT,
	CHANNEL_DCIN,
	CHANNEL_ICHG,
	CHANNEL_VPH_PWR,
	CHANNEL_IBAT,
	CHANNEL_MPP_1,
	CHANNEL_MPP_2,
	CHANNEL_BATT_THERM,
	CHANNEL_BATT_ID,
	CHANNEL_USBIN,
	CHANNEL_DIE_TEMP,
	CHANNEL_625MV,
	CHANNEL_125V,
	CHANNEL_CHG_TEMP,
	CHANNEL_MUXOFF,
	CHANNEL_NONE,
};

/**
 * enum pm8921_adc_mpp_channels - PM8921 AMUX arbiter MPP channels
 * Yet to be defined, each of the value is representative
 * of the device connected to the MPP
 * %ADC_MPP_AMUX8: Fixed mappaing to PA THERM
 */
enum pm8921_adc_mpp_channels {
	ADC_MPP_ATEST_8 = 0,
	ADC_MPP_USB_SNS_DIV20,
	ADC_MPP_DCIN_SNS_DIV20,
	ADC_MPP_AMUX3,
	ADC_MPP_AMUX4,
	ADC_MPP_AMUX5,
	ADC_MPP_AMUX6,
	ADC_MPP_AMUX7,
	ADC_MPP_AMUX8,
	ADC_MPP_ATEST_1,
	ADC_MPP_ATEST_2,
	ADC_MPP_ATEST_3,
	ADC_MPP_ATEST_4,
	ADC_MPP_ATEST_5,
	ADC_MPP_ATEST_6,
	ADC_MPP_ATEST_7,
	ADC_MPP_CHANNEL_NONE,
};

#define PM8921_ADC_PMIC_0	0x0

#define PM8921_CHANNEL_ADC_625_MV	625

#define PM8921_AMUX_MPP_3	0x3
#define PM8921_AMUX_MPP_4	0x4
#define PM8921_AMUX_MPP_5	0x5
#define PM8921_AMUX_MPP_6	0x6
#define PM8921_AMUX_MPP_8	0x8

#define PM8921_ADC_DEV_NAME	"pm8921-adc"

/**
 * enum pm8921_adc_decimation_type - Sampling rate supported
 * %ADC_DECIMATION_TYPE1: 512
 * %ADC_DECIMATION_TYPE2: 1K
 * %ADC_DECIMATION_TYPE3: 2K
 * %ADC_DECIMATION_TYPE4: 4k
 * %ADC_DECIMATION_NONE: Do not use this Sampling type
 *
 * The Sampling rate is specific to each channel of the PM8921 ADC arbiter.
 */
enum pm8921_adc_decimation_type {
	ADC_DECIMATION_TYPE1 = 0,
	ADC_DECIMATION_TYPE2,
	ADC_DECIMATION_TYPE3,
	ADC_DECIMATION_TYPE4,
	ADC_DECIMATION_NONE,
};

/**
 * enum pm8921_adc_calib_type - PM8921 ADC Calibration type
 * %ADC_CALIB_ABSOLUTE: Use 625mV and 1.25V reference channels
 * %ADC_CALIB_RATIOMETRIC: Use reference Voltage/GND
 * %ADC_CALIB_CONFIG_NONE: Do not use this calibration type
 *
 * Use the input reference voltage depending on the calibration type
 * to calcluate the offset and gain parameters. The calibration is
 * specific to each channel of the PM8921 ADC.
 */
enum pm8921_adc_calib_type {
	ADC_CALIB_ABSOLUTE = 0,
	ADC_CALIB_RATIOMETRIC,
	ADC_CALIB_CONFIG_NONE,
};

/**
 * enum pm8921_adc_channel_scaling_param - pre-scaling AMUX ratio
 * %CHAN_PATH_SCALING1: ratio of {1, 1}
 * %CHAN_PATH_SCALING2: ratio of {1, 3}
 * %CHAN_PATH_SCALING3: ratio of {1, 4}
 * %CHAN_PATH_SCALING4: ratio of {1, 6}
 * %CHAN_PATH_NONE: Do not use this pre-scaling ratio type
 *
 * The pre-scaling is applied for signals to be within the voltage range
 * of the ADC.
 */
enum pm8921_adc_channel_scaling_param {
	CHAN_PATH_SCALING1 = 0,
	CHAN_PATH_SCALING2,
	CHAN_PATH_SCALING3,
	CHAN_PATH_SCALING4,
	CHAN_PATH_SCALING_NONE,
};

/**
 * enum pm8921_adc_amux_input_rsv - HK/XOADC reference voltage
 * %AMUX_RSV0: XO_IN/XOADC_GND
 * %AMUX_RSV1: PMIC_IN/XOADC_GND
 * %AMUX_RSV2: PMIC_IN/BMS_CSP
 * %AMUX_RSV3: not used
 * %AMUX_RSV4: XOADC_GND/XOADC_GND
 * %AMUX_RSV5: XOADC_VREF/XOADC_GND
 * %AMUX_NONE: Do not use this input reference voltage selection
 */
enum pm8921_adc_amux_input_rsv {
	AMUX_RSV0 = 0,
	AMUX_RSV1,
	AMUX_RSV2,
	AMUX_RSV3,
	AMUX_RSV4,
	AMUX_RSV5,
	AMUX_NONE,
};

/**
 * enum pm8921_adc_premux_mpp_scale_type - 16:1 pre-mux scale ratio
 * %PREMUX_MPP_SCALE_0: No scaling to the input signal
 * %PREMUX_MPP_SCALE_1: Unity scaling selected by the user for MPP input
 * %PREMUX_MPP_SCALE_1_DIV3: 1/3 pre-scale to the input MPP signal
 * %PREMUX_MPP_NONE: Do not use this pre-scale mpp type
 */
enum pm8921_adc_premux_mpp_scale_type {
	PREMUX_MPP_SCALE_0 = 0,
	PREMUX_MPP_SCALE_1,
	PREMUX_MPP_SCALE_1_DIV3,
	PREMUX_MPP_NONE,
};

/**
 * enum pm8921_adc_scale_fn_type - Scaling function for pm8921 pre calibrated
 *				   digital data relative to ADC reference
 * %ADC_SCALE_DEFAULT: Default scaling to convert raw adc code to voltage
 * %ADC_SCALE_BATT_THERM: Conversion to temperature based on btm parameters
 * %ADC_SCALE_PMIC_THERM: Returns result in milli degree's Centigrade
 * %ADC_SCALE_XTERN_CHGR_CUR: Returns current across 0.1 ohm resistor
 * %ADC_SCALE_NONE: Do not use this scaling type
 */
enum pm8921_adc_scale_fn_type {
	ADC_SCALE_DEFAULT = 0,
	ADC_SCALE_BATT_THERM,
	ADC_SCALE_PMIC_THERM,
	ADC_SCALE_XTERN_CHGR_CUR,
	ADC_SCALE_NONE,
};

/**
 * struct pm8921_adc_linear_graph - Represent ADC characteristics
 * @offset: Offset with respect to the actual curve
 * @dy: Numerator slope to calculate the gain
 * @dx: Denominator slope to calculate the gain
 *
 * Each ADC device has different offset and gain parameters which are computed
 * to calibrate the device.
 */
struct pm8921_adc_linear_graph {
	int32_t offset;
	int32_t dy;
	int32_t dx;
};

/**
 * struct pm8921_adc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance
 */
struct pm8921_adc_map_pt {
	int32_t x;
	int32_t y;
};

/**
 * struct pm8921_adc_scaling_ratio - Represent scaling ratio for adc input
 * @num: Numerator scaling parameter
 * @den: Denominator scaling parameter
 */
struct pm8921_adc_scaling_ratio {
	int32_t num;
	int32_t den;
};

/**
 * struct pm8921_adc_properties - Represent the ADC properties
 * @adc_reference: Reference voltage for PM8921 ADC
 * @bitresolution: ADC bit resolution for PM8921 ADC
 * @biploar: Polarity for PM8921 ADC
 */
struct pm8921_adc_properties {
	uint32_t	adc_vdd_reference;
	uint32_t	bitresolution;
	bool		bipolar;
};

/**
 * struct pm8921_adc_chan_properties - Represent channel properties of the ADC
 * @offset_gain_numerator: The inverse numerator of the gain applied to the
 *			   input channel
 * @offset_gain_denominator: The inverse denominator of the gain applied to the
 *			     input channel
 * @adc_graph: ADC graph for the channel of struct type pm8921_adc_linear_graph
 */
struct pm8921_adc_chan_properties {
	uint32_t			offset_gain_numerator;
	uint32_t			offset_gain_denominator;
	struct pm8921_adc_linear_graph	adc_graph[2];
};

/**
 * struct pm8921_adc_chan_result - Represent the result of the PM8921 ADC
 * @chan: The channel number of the requested conversion
 * @adc_code: The pre-calibrated digital output of a given ADC relative to the
 *	      the ADC reference
 * @measurement: In units specific for a given ADC; most ADC uses reference
 *		 voltage but some ADC uses reference current. This measurement
 *		 here is a number relative to a reference of a given ADC
 * @physical: The data meaningful for each individual channel whether it is
 *	      voltage, current, temperature, etc.
 */
struct pm8921_adc_chan_result {
	uint32_t	chan;
	int32_t		adc_code;
	int64_t		measurement;
	int64_t		physical;
};

#if defined(CONFIG_MFD_PM8921_ADC) || defined(CONFIG_MFD_PM8921_ADC_MODULE)
/**
 * pm8921_adc_scale_default() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8921 adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	Physical result to be stored.
 */
int32_t pm8921_adc_scale_default(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt);
/**
 * pm8921_adc_scale_tdkntcg_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature of the xo therm in mili
		degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8921 adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t pm8921_adc_tdkntcg_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt);
/**
 * pm8921_adc_scale_batt_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the temperature in degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8921 adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t pm8921_adc_scale_batt_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt);
/**
 * pm8921_adc_scale_pmic_therm() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Performs the AMUX out as 2mv/K and returns
 *		the temperature in mili degC.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8921 adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t pm8921_adc_scale_pmic_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt);
/**
 * pm8921_adc_scale_xtern_chgr_cur() - Scales the pre-calibrated digital output
 *		of an ADC to the ADC reference and compensates for the
 *		gain and offset. Returns the current across the 10m ohm
 *		resistor.
 * @adc_code:	pre-calibrated digital ouput of the ADC.
 * @adc_prop:	adc properties of the pm8921 adc such as bit resolution,
 *		reference voltage.
 * @chan_prop:	individual channel properties to compensate the i/p scaling,
 *		slope and offset.
 * @chan_rslt:	physical result to be stored.
 */
int32_t pm8921_adc_scale_xtern_chgr_cur(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt);

#else
static inline int32_t pm8921_adc_scale_default(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8921_adc_tdkntcg_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8921_adc_scale_batt_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8921_adc_scale_pmic_therm(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t pm8921_adc_scale_xtern_chgr_cur(int32_t adc_code,
			const struct pm8921_adc_properties *adc_prop,
			const struct pm8921_adc_chan_properties *chan_prop,
			struct pm8921_adc_chan_result *chan_rslt)
{ return -ENXIO; }
#endif

/**
 * struct pm8921_adc_scale_fn - Scaling function prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties, channel properties,
 *	and returns the physical result
 */
struct pm8921_adc_scale_fn {
	int32_t (*chan) (int32_t,
		const struct pm8921_adc_properties *,
		const struct pm8921_adc_chan_properties *,
		struct pm8921_adc_chan_result *);
};

/**
 * struct pm8921_adc_amux - AMUX properties for individual channel
 * @name: Channel name
 * @channel_name: Channel in integer used from pm8921_adc_channels
 * @chan_path_prescaling: Channel scaling performed on the input signal
 * @adc_rsv: Input reference Voltage/GND selection to the ADC
 * @adc_decimation: Sampling rate desired for the channel
 * adc_scale_fn: Scaling function to convert to the data meaningful for
 *		 each individual channel whether it is voltage, current,
 *		 temperature, etc and compensates the channel properties
 */
struct pm8921_adc_amux {
	char					*name;
	enum pm8921_adc_channels		channel_name;
	enum pm8921_adc_channel_scaling_param	chan_path_prescaling;
	enum pm8921_adc_amux_input_rsv		adc_rsv;
	enum pm8921_adc_decimation_type		adc_decimation;
	enum pm8921_adc_scale_fn_type		adc_scale_fn;
};

/**
 * struct pm8921_adc_arb_btm - PM8921 ADC BTM parameters
 * @btm_prop: BTM parameters such as input resistance, voltage and Rtherm across
 * the thermistor
 * @btm_param: BTM temperature thresholds and interval to program the BTM
 * @btm_channel_prop: Channel specific properties of the BTM channel
 */
struct pm8921_adc_arb_btm {
	struct pm8921_adc_btm_prop			*btm_prop;
	struct pm8921_adc_arb_btm_param			*btm_param;
	struct pm8921_adc_btm_channel_properties	*btm_channel_prop;
};

/**
 * struct pm8921_adc_btm_channel_properties - PM8921 ADC BTM channel properties
 * @btm_channel: Channel name
 * @decimation: Sampling rate
 * @btm_rsv: Input selection of Vref/GND
 * @chan_prop: Channel properties for the BTM channel
 */
struct pm8921_adc_btm_channel_properties {
	enum pm8921_adc_channels		btm_channel;
	enum pm8921_adc_decimation_type		decimation;
	enum pm8921_adc_amux_input_rsv		btm_rsv;
	struct pm8921_adc_chan_properties	*chan_prop;
};

/**
 * struct pm8921_adc_btm_prop - BTM specific resistors, voltage reference to
 *				calcluate the temperature across Rthm
 * @rs1: Resistor of the Vref_therm
 * @rs2: Resistor of BTM
 * @r_thm: Resistance of the thermistor
 * vref_thm: Voltage of vref_therm
 */
struct pm8921_adc_btm_prop {
	uint32_t rs_1;
	uint32_t rs_2;
	uint32_t r_thm;
	uint32_t vref_thm;
};

/**
 * struct pm8921_adc_arb_btm_param - PM8921 ADC BTM parameters to set threshold
 *				     temperature for client notification
 * @low_thr_temp: low temperature threshold request for notification
 * @high_thr_temp: high temperature threshold request for notification
 * @low_thr_voltage: low temperature converted to voltage by arbiter driver
 * @high_thr_voltage: high temperature converted to voltage by arbiter driver
 * @interval: Interval period to check for temperature notification
 * @btm_warm_fn: Remote function call for warm threshold
 * @btm_cold_fn: Remote function call for cold threshold
 *
 * BTM client passes the parameters to be set for the
 * temperature threshold notifications. The client is
 * responsible for setting the new threshold
 * levels once the thresholds are reached
 */
struct pm8921_adc_arb_btm_param {
	uint32_t	low_thr_temp;
	uint32_t	high_thr_temp;
	uint32_t	low_thr_voltage;
	uint32_t	high_thr_voltage;
	int32_t		interval;
	void		(*btm_warm_fn) (void);
	void		(*btm_cold_fn) (void);
};

int32_t pm8921_adc_batt_scaler(struct pm8921_adc_arb_btm_param *);

/**
 * struct pm8921_adc_platform_data - PM8921 ADC platform data
 * @adc_prop: ADC specific parameters, voltage and channel setup
 * @adc_channel: Channel properties of the ADC arbiter
 * @adc_num_channel: Total number of chanels supported
 */
struct pm8921_adc_platform_data {
	struct pm8921_adc_properties	*adc_prop;
	struct pm8921_adc_amux		*adc_channel;
	uint32_t			adc_num_channel;
	u32				adc_wakeup;
};

/* Public API */
#if defined(CONFIG_MFD_PM8921_ADC) || defined(CONFIG_MFD_PM8921_ADC_MODULE)
/**
 * pm8921_adc_read() - Performs ADC read on the channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
uint32_t pm8921_adc_read(enum pm8921_adc_channels channel,
				struct pm8921_adc_chan_result *result);
/**
 * pm8921_mpp_adc_read() - Performs ADC read on the channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 * @mpp_scale:	The pre scale value to be performed to the input signal
 *		passed. Currently the pre-scale support is for 1 and 1/3.
 */
uint32_t pm8921_adc_mpp_read(enum pm8921_adc_mpp_channels channel,
			struct pm8921_adc_chan_result *result,
			enum pm8921_adc_premux_mpp_scale_type);
/**
 * pm8921_adc_btm_start() - Configure the BTM registers and start
			monitoring the BATT_THERM channel for
			threshold warm/cold temperature set
			by the Battery client. The btm_start
			api is to be used after calling the
			pm8921_btm_configure() api which sets
			the temperature thresholds, interval
			and functions to call when warm/cold
			events are triggered.
 * @param:	none.
 */
uint32_t pm8921_adc_btm_start(void);

/**
 * pm8921_adc_btm_end() - Configures the BTM registers to stop
 *			monitoring the BATT_THERM channel for
 *			warm/cold events and disables the
 *			interval timer.
 * @param:	none.
 */
uint32_t pm8921_adc_btm_end(void);

/**
 * pm8921_adc_btm_configure() - Configures the BATT_THERM channel
 *			parameters for warm/cold thresholds.
 *			Sets the interval timer for perfoming
 *			reading the temperature done by the HW.
 * @btm_param:		Structure pointer of type adc_arb_btm_param *
 *			which client provides for threshold warm/cold,
 *			interval and functions to call when warm/cold
 *			events are triggered.
 */
uint32_t pm8921_adc_btm_configure(struct pm8921_adc_arb_btm_param *);
#else
static inline uint32_t pm8921_adc_read(uint32_t channel,
				struct pm8921_adc_chan_result *result)
{ return -ENXIO; }
static inline uint32_t pm8921_mpp_adc_read(uint32_t channel,
		struct pm8921_adc_chan_result *result,
		enum pm8921_adc_premux_mpp_scale_type scale)
{ return -ENXIO; }
static inline uint32_t pm8921_adc_btm_start(void)
{ return -ENXIO; }
static inline uint32_t pm8921_adc_btm_end(void)
{ return -ENXIO; }
static inline uint32_t pm8921_adc_btm_configure(
		struct pm8921_adc_arb_btm_param *param)
{ return -ENXIO; }
#endif

#endif /* MFD_PM8921_ADC_H */
