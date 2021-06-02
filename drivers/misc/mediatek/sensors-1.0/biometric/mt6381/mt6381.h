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
/*
 * Definitions for MT6381 als/ps sensor chip.
 */
#ifndef __MT6381_H__
#define __MT6381_H__

/** @brief  This macro defines the SRAM length used
 *          in #vsm_driver_read_sram() function.
 */
#define VSM_SRAM_LEN    (384)

/**
 * @}
 */

/** @defgroup vsm_driver_enum Enum
 * @{
 */

/** @brief  This enum defines the API return type.  */
enum vsm_status_t {
	VSM_STATUS_ERROR = -3,		 /**<  The function call failed. */
	/**<  Invalid parameter was given. */
	VSM_STATUS_INVALID_PARAMETER = -2,
	VSM_STATUS_UNINITIALIZED = -1, /**<  The VSM is not initialized. */
	VSM_STATUS_OK = 0 /**<  This function call was successful. */
};


/** @brief This enum defines the VSM signal. */
enum vsm_signal_t {
	/* Voltage signal */
	VSM_SIGNAL_EKG = 0x00000001L,		/**<  EKG signal. */
	VSM_SIGNAL_EEG = 0x00000002L,		/**<  EEG signal. */
	VSM_SIGNAL_EMG = 0x00000004L,		/**<  EMG signal. */
	VSM_SIGNAL_GSR = 0x00000008L,		/**<  GSR signal. */

	/* Current signal */
	VSM_SIGNAL_PPG1 = 0x00000010L,		/**<  PPG1 signal. */
	VSM_SIGNAL_PPG2 = 0x00000020L,		/**<  PPG2 signal. */
	VSM_SIGNAL_BISI = 0x00000040L,		/**<  BISI signal. */
	/**<  PPG1 512 Hz signal for blood pressure. */
	VSM_SIGNAL_PPG1_512HZ = 0x00000080L,
};

/** @brief This enum defines the sram types. */
enum vsm_sram_type_t {
	VSM_SRAM_EKG = 0,    /**<  SRAM EKG type. */
	VSM_SRAM_PPG1 = 1,	 /**<  SRAM PPG1 type. */
	VSM_SRAM_PPG2 = 2,	 /**<  SRAM PPG2 type. */
	VSM_SRAM_BISI = 3,	 /**<  SRAM BISI type. */
	VSM_SRAM_NUMBER
};

/** @brief This enum defines the tia gain. */
enum vsm_tia_gain_t {
	VSM_TIA_GAIN_10_K = 5,	    /**<  TIA GAIN 10 kΩ. */
	VSM_TIA_GAIN_25_K = 4,	    /**<  TIA GAIN 25 kΩ. */
	VSM_TIA_GAIN_50_K = 3,	    /**<  TIA GAIN 50 kΩ. */
	VSM_TIA_GAIN_100_K = 2,	    /**<  TIA GAIN 100 kΩ. */
	VSM_TIA_GAIN_250_K = 1,	    /**<  TIA GAIN 250 kΩ. */
	VSM_TIA_GAIN_500_K = 0,	    /**<  TIA GAIN 500 kΩ. */
	VSM_TIA_GAIN_1000_K = 6	    /**<  TIA GAIN 1000 kΩ. */
};

/** @brief This enum defines the PGA gain. */
enum vsm_pga_gain_t {
	VSM_PGA_GAIN_1 = 0,	     /**<  PGA GAIN 1   V/V. */
	VSM_PGA_GAIN_1_DOT_5 = 1,    /**<  PGA GAIN 1.5 V/V. */
	VSM_PGA_GAIN_2 = 2,	     /**<  PGA GAIN 2   V/V. */
	VSM_PGA_GAIN_3 = 3,	     /**<  PGA GAIN 3   V/V. */
	VSM_PGA_GAIN_4 = 4,	     /**<  PGA GAIN 4   V/V. */
	VSM_PGA_GAIN_6 = 6,	     /**<  PGA GAIN 5   V/V. */
};


/** @brief This enum defines the AMBDAC type. */
enum vsm_ambdac_type_t {
	VSM_AMBDAC_1 = 1,	    /**<  AMBDAC1. */
	VSM_AMBDAC_2 = 2,	    /**<  AMBDAC2. */
};

/** @brief This enum defines the LED type. */
enum vsm_led_type_t {
	VSM_LED_1 = 1,		 /**<  LED 1. */
	VSM_LED_2 = 2,		 /**<  LED 2. */
};

/** @brief This enum defines the AMBDAC current. */
enum vsm_ambdac_current_t {
	VSM_AMBDAC_CURR_00_MA = 0x0,	   /**<  AMBDAC CURRENT 00 mA. */
	VSM_AMBDAC_CURR_01_MA = 0x1,	   /**<  AMBDAC CURRENT 01 mA. */
	VSM_AMBDAC_CURR_02_MA = 0x2,	   /**<  AMBDAC CURRENT 02 mA. */
	VSM_AMBDAC_CURR_03_MA = 0x3,	   /**<  AMBDAC CURRENT 03 mA. */
	VSM_AMBDAC_CURR_04_MA = 0x4,	   /**<  AMBDAC CURRENT 04 mA. */
	VSM_AMBDAC_CURR_05_MA = 0x5,	   /**<  AMBDAC CURRENT 05 mA. */
	VSM_AMBDAC_CURR_06_MA = 0x6,	   /**<  AMBDAC CURRENT 06 mA. */
};

/** @brief This enum defines the EKG sample rate. */
enum vsm_ekg_fps_t {
	VSM_EKG_FPS_256_HZ = 0,	   /**<  EKG 256Hz. */
	VSM_EKG_FPS_512_HZ = 1	   /**<  EKG 512Hz. */
};

/**
 * @}
 */

/** @defgroup hal_wdt_struct Struct
 * @{
 */

/**
 * @brief This structure defines the bus data structure. For more information,
 * please refer to #vsm_driver_read_register().
 */
struct bus_data_t {
	uint8_t addr;	   /**< Device address.*/
	uint8_t reg;	   /**< Device register value.*/
	uint8_t *data_buf; /**< A pointer to a data buffer.*/
	uint8_t length;	       /**< Length of the data buffer.*/
};

enum vsm_status_t vsm_driver_update_register(void);
enum vsm_status_t vsm_driver_write_register(struct bus_data_t *data);
enum vsm_status_t vsm_driver_read_register(struct bus_data_t *data);

#endif
