/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef PPG_CONTROL_LIB_H
#define PPG_CONTROL_LIB_H
/**
 * @file    ppg_control.h
 * @brief   Automatic PPG configuration control
 * @author  Mediatek
 * @version 1.0.1.1
 * @date    2016.04.17
 */

/**
 * @addtogroup PPG_CONTROL
 * @{
 * This section introduces the PPG control APIs including
 * terms and acronyms, control flow, supported features,
 * PPG function groups, enums, structures and functions.
 * The PPG control library is applied to enhance the PPG
 * signal quality acquired from MediaTek MT2511.
 *
 * @section Terms_Chapter Terms and acronyms
 *
 * |Terms                   |Details                                    |
 * |------------------------------|-------------------------------------|
 * |\b PPG  | Photoplethysmogram. For more information, please refer to
 *            <a href="https://en.wikipedia.org/wiki/Photoplethysmogram">
 *            Photoplethysmogram in Wikipedia </a>.|
 *
 * @section Function_Block_Diagram Control Flow: PPG control
 * Initialization
 * @image html Initial.png
 * Measurement
 * @image html Measurement.png
 *
 * @section PPG_Control_Usage_Chapter How to use PPG APIs
 *
 * Step1: Call #ppg_control_init() to initialize the PPG control library. \n
 * Step2: Call #ppg_control_process() to analyze the incoming PPG data and
 *             adjust the setting of analog front-end on MediaTek MT2511. \n
 *
 *  - sample code:
 *    @code
 *
 * void example(int32_t state)
 * {
 *     // Initialization
 *     ppg_control_init(); // Initialize the library.
 *     // The sampling frequency of PPG input (Support 125Hz only).
 *     int32_t ppg_control_fs = 512;
 *     int32_t ppg_control_bit_width = 23;      // The PPG signal bit width.
 *     // The input configuration, default value is 1.
 *     int32_t ppg_control_cfg = 1;
 *     // The input source, default value is 1 (PPG channel 1).
 *     int32_t ppg_control_src = 1;
 *     // The PPG control mode.
 *     int32_t ppg_control_mode = PPG_CONTROL_MODE_LEDS;
 *     // Input structure for the #ppg_control_process().
 *     ppg_control_t ppg1_control_input;
 *     int32_t ppg_control_flag;
 *     int32_t *ppg_control_output;    // Pointer to the result for debugging.
 *
 *     switch(state)
 *     {
 *         case PPG_DATA_READY:
 *            ppg1_control_input.input = data_input_ppg_buf;
 *            ppg1_control_input.input_fs = ppg_control_in_fs;
 *            // max length = ppg_control_in_fs *2 = 250.
 *            ppg1_control_input.input_length = 250;
 *            ppg1_control_input.input_bit_width = ppg_control_in_bit_width;
 *            ppg1_control_input.input_config = ppg_control_in_cfg;
 *            ppg1_control_input.input_source = ppg_control_in_src;
 *
 *            ppg_control_flag = ppg_control_process(
 *                &ppg1_control_input,        // The input signal data.
 *                ppg_control_mode,           // The control mode.
 *                ppg_control_output,// Pointer to the result for debugging.
 *            );
 *        break;
 *      }
 * }
 *    @endcode
 *
 */

/** @defgroup ppg_control_define Define
 * @{
 */

/* #include <stdint.h> */
/*
 * #define INT32 int32_t
 * #define INT16 int16_t
 * #define UINT32 uint32_t
 * #define UINT16 uint16_t
 */

#define INT32 int
#define INT16 short
#define UINT32 unsigned int
#define UINT16 unsigned short

/**
 * @}
 */

/** @defgroup ppg_control_enum Enum
 * @{
 */

/** @brief  This enum defines the return type of PPG control library.  */
enum ppg_control_status_t {
	PPG_CONTROL_STATUS_ERROR = -2,	   /**<  The function call failed. */
	/**<  Invalid parameter is given. */
	PPG_CONTROL_STATUS_INVALID_PARAMETER = -1,
	/**<  This function call is successful. */
	PPG_CONTROL_STATUS_OK = 0
};

/** @brief This enum defines the PPG control mode. */
enum {
	PPG_CONTROL_MODE_LED1 = 1,
			       /**< Single channel adjustment. */
	PPG_CONTROL_MODE_LEDS = 3
			       /**< Dual channel adjustment. */
};

/**
 * @}
 */

/** @defgroup ppg_control_struct Struct
 * @{
 */

/** @struct ppg_control_t
 * @brief This structure defines the data
 *        input structure for PPG control library.
 */
struct ppg_control_t {
	INT32 *input;	    /**< A pointer to the input PPG signal. */
	INT32 *input_amb;   /**< A pointer to the input AMB signal. */
	/**< The sampling frequency (fs) of
	 *the input data. (supports 125Hz only).
	 */
	INT32 input_fs;
	/**< The number of input samples.
	 *The maximum length = input_fs * 2= 250.
	 */
	INT32 input_length;
	/* INT32 input_bit_width; */ /**< The bit width of the input data. */
	/* If the data is already converted to signed
	 *representation, set bit width to 32.
	 */
	/**< Configuration of the input signal (Please set to 1). */
	INT32 input_config;
	/**< 1: this is the data from MT2511 PPG channel 1 (PPG1). */
	/* 2: this is the data from MT2511 PPG channel 2 (PPG2). */
	INT32 input_source;
};

/**
 * @}
 */

/* Function definition */
/**
 * @brief Call this function to get the library version code.
 * @return The return value of 0xAABBCCDD corresponds to version AA.BB.CC.DD.
 *         The integer value should be converted to
 *         heximal representation to get the version number.
 *         For example, 0x01000000 is for verion 1.0.0.0.
 */
UINT32 ppg_control_get_version(void);

/**
 * @brief This function should be called at the initialization stage.
 * @return #PPG_CONTROL_STATUS_ERROR, if the operation failed. \n
 *         #PPG_CONTROL_STATUS_INVALID_PARAMETER,
 *             if an invalid parameter was given. \n
 *         #PPG_CONTROL_STATUS_OK, if the operation completed successfully. \n
 */
enum ppg_control_status_t ppg_control_init(void);

 /**
  * @brief Call this function to analyze the incoming PPG
  *        data and automatically adjust the setting of analog
  *        front-end on MediaTek MT2511.
  * @param[in] *ppg_control_input is the input data structure
  *            containing the actual data to be processed by this API,
  *            in order to adjust the setting of the the
  *            analog front-end on MediaTek MT2511.
  * @param[in] ppg_control_mode is to set the library
  *                according to the LED/PPG configuration. \n
  *            #PPG_CONTROL_MODE_LED1: Adjust only LED1, according to PPG1. \n
  *            #PPG_CONTROL_MODE_DUAL1: Adjust both LED1 and
  *                LED2 with equal current, according to PPG1.
  * @param[out] *ppg_control_output is the output signal data buffer
  * @return    Return the flag to indicate whether the setting of
  *                the MT2511 analog front-end is adjusted. \n
  *            #PPG_CONTROL_STATUS_ERROR, if the operation failed. \n
  *            #PPG_CONTROL_STATUS_INVALID_PARAMETER,
  *                if an invalid parameter was given. \n
  *            #PPG_CONTROL_STATUS_OK,
  *                if the operation completed successfully. \n
  *
  */
INT32 ppg_control_process(struct ppg_control_t *ppg_control_input);

/**
 * @brief This function acesses the internal status of the PPG control library.
 * @param[in] ppg_control_internal_config is to
 *            select the internal status of target module.
 * @return 32-bit internal status of library for debugging.
 */
INT32 ppg_control_get_status(INT32 ppg_control_internal_config);

/**
 * @}
 * @}
 */





#endif				/* PPG_CONTROL_LIB_H */
