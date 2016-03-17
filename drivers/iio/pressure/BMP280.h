/*!
 * @section LICENSE
 * @license$
 *
 * @filename bmp280.h
 * @date     2015/03/26
 * @id       "5d6d280"
 *
 * @brief
 * API Header
 *
 * Revision: 2.0.6(Pressure and Temperature compensation code revision is 1.1)
 */
/****************************************************************************/
#ifndef __BMP280_H__
#define __BMP280_H__

/*!
* @brief The following definition uses for define the data types
*
* @note While porting the API please consider the following
* @note Please check the version of C standard
* @note Are you using Linux platform
*/

/*!
* @brief For the Linux platform support
* Please use the types.h for your data types definitions
*/
#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/math64.h>
#define BMP280_U16_t u16
#define BMP280_S16_t s16
#define BMP280_U32_t u32
#define BMP280_S32_t s32
#define BMP280_U64_t u64
#define BMP280_S64_t s64
#define BMP280_64BITSUPPORT_PRESENT
#else
#include <limits.h>

/* Find correct data type for signed/unsigned 16 bit variables by
	checking max of unsigned variant */
#if USHRT_MAX == 0xFFFF
		/* 16 bit achieved with short */
		#define BMP280_U16_t unsigned short
		#define BMP280_S16_t signed short
#elif UINT_MAX == 0xFFFF
		/* 16 bit achieved with int */
		#define BMP280_U16_t unsigned int
		#define BMP280_S16_t signed int
#else
		#error BMP280_U16_t and BMP280_S16_t could not be \
		defined automatically, please do so manually
#endif

/* Find correct data type for 32 bit variables */
#if INT_MAX == 0x7FFFFFFF
		/* 32 bit achieved with int */
		#define BMP280_U32_t unsigned int
		#define BMP280_S32_t signed int
#elif LONG_MAX == 0x7FFFFFFF
		/* 32 bit achieved with long int */
		#define BMP280_U32_t unsigned long int
		#define BMP280_S32_t signed long int
#else
		#error BMP280_S32_t and BMP280_U32_t could not be \
		defined automatically, please do so manually
#endif

/* Find correct data type for 64 bit variables */
#if defined(LONG_MAX) && LONG_MAX == 0x7fffffffffffffffL
	#define BMP280_S64_t long int
	#define BMP280_64BITSUPPORT_PRESENT
#elif defined(LLONG_MAX) && (LLONG_MAX == 0x7fffffffffffffffLL)
	#define BMP280_S64_t long long int
	#define BMP280_64BITSUPPORT_PRESENT
#else
	#warning Either the correct data type for signed 64 bit \
		integer could not be found, or 64 bit integers are \
		not supported in your environment.
	#warning The API will only offer 32 bit pressure calculation. \
		This will slightly impede accuracy(noise of ~1 Pa RMS \
		will be added to output).
	#warning If 64 bit integers are supported on your platform, \
		please set BMP280_S64_t manually and
		#define BMP280_64BITSUPPORT_PRESENT manually.
#endif
#endif/*__linux__*/



/* If the user wants to support floating point calculations, please set \
	the following #define. If floating point calculation is not wanted \
	or allowed (e.g. in Linux kernel), please do not set the define. */
/*#define BMP280_ENABLE_FLOAT*/
/* If the user wants to support 64 bit integer calculation (needed for \
	optimal pressure accuracy) please set the following #define. If \
	int64 calculation is not wanted (e.g. because it would include \
	large libraries), please do not set the define. */
#define BMP280_ENABLE_INT64

/** defines the return parameter type of the BMP280_WR_FUNCTION */
#define BMP280_BUS_WR_RETURN_TYPE signed char

/**\brief links the order of parameters defined in
BMP280_BUS_WR_PARAM_TYPE to function calls used inside the API*/
#define BMP280_BUS_WR_PARAM_TYPES unsigned char, unsigned char,\
	unsigned char *, unsigned char

/**\brief links the order of parameters defined in
BMP280_BUS_WR_PARAM_TYPE to function calls used inside the API*/
#define BMP280_BUS_WR_PARAM_ORDER(device_addr, register_addr,\
	register_data, wr_len)

/* never change this line */
#define BMP280_BUS_WRITE_FUNC(device_addr, register_addr,\
register_data, wr_len) bus_write(device_addr, register_addr,\
	register_data, wr_len)

/**\brief defines the return parameter type of the BMP280_RD_FUNCTION
*/
#define BMP280_BUS_RD_RETURN_TYPE signed char

/**\brief defines the calling parameter types of the BMP280_RD_FUNCTION
*/
#define BMP280_BUS_RD_PARAM_TYPES unsigned char, unsigned char,\
	unsigned char *, unsigned char

/**\brief links the order of parameters defined in \
BMP280_BUS_RD_PARAM_TYPE to function calls used inside the API
*/
#define BMP280_BUS_RD_PARAM_ORDER device_addr, register_addr,\
	register_data

/* never change this line */
#define BMP280_BUS_READ_FUNC(device_addr, register_addr,\
	register_data, rd_len)bus_read(device_addr, register_addr,\
	register_data, rd_len)

/**\brief defines the return parameter type of the BMP280_DELAY_FUNCTION
*/
#define BMP280_DELAY_RETURN_TYPE void

/**\brief defines the calling parameter types of the BMP280_DELAY_FUNCTION
*/
#define BMP280_DELAY_PARAM_TYPES BMP280_U16_t

/* never change this line */
#define BMP280_DELAY_FUNC(delay_in_msec)\
		delay_func(delay_in_msec)

#define BMP280_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMP280_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


/* Constants */
#define BMP280_NULL                          0
#define BMP280_RETURN_FUNCTION_TYPE          signed char

#define SHIFT_RIGHT_4_POSITION				 4
#define SHIFT_LEFT_2_POSITION                2
#define SHIFT_LEFT_4_POSITION                4
#define SHIFT_LEFT_5_POSITION                5
#define SHIFT_LEFT_8_POSITION                8
#define SHIFT_LEFT_12_POSITION               12
#define SHIFT_LEFT_16_POSITION               16
#define BMA280_Four_U8X                      4
#define BMA280_Eight_U8X                     8

#define E_BMP280_NULL_PTR                    ((signed char)-127)
#define E_BMP280_COMM_RES                    ((signed char)-1)
#define E_BMP280_OUT_OF_RANGE                ((signed char)-2)

#define BMP280_I2C_ADDRESS1                  0x76
#define BMP280_I2C_ADDRESS2                  0x77
#define BMP280_I2C_ADDRESS                   BMP280_I2C_ADDRESS2

/* Sensor Specific constants */
#define BMP280_SLEEP_MODE                    0x00
#define BMP280_FORCED_MODE                   0x01
#define BMP280_NORMAL_MODE                   0x03
#define BMP280_SOFT_RESET                    0xB6

#define BMP280_DELAYTIME_MS_NONE             0
#define BMP280_DELAYTIME_MS_5                5
#define BMP280_DELAYTIME_MS_6                6
#define BMP280_DELAYTIME_MS_8                8
#define BMP280_DELAYTIME_MS_12               12
#define BMP280_DELAYTIME_MS_22               22
#define BMP280_DELAYTIME_MS_38               38

#define BMP280_STANDBYTIME_1_MS              1
#define BMP280_STANDBYTIME_63_MS             63
#define BMP280_STANDBYTIME_125_MS            125
#define BMP280_STANDBYTIME_250_MS            250
#define BMP280_STANDBYTIME_500_MS            500
#define BMP280_STANDBYTIME_1000_MS           1000
#define BMP280_STANDBYTIME_2000_MS           2000
#define BMP280_STANDBYTIME_4000_MS           4000

#define BMP280_OVERSAMPLING_SKIPPED          0x00
#define BMP280_OVERSAMPLING_1X               0x01
#define BMP280_OVERSAMPLING_2X               0x02
#define BMP280_OVERSAMPLING_4X               0x03
#define BMP280_OVERSAMPLING_8X               0x04
#define BMP280_OVERSAMPLING_16X              0x05

#define BMP280_ULTRALOWPOWER_MODE            0x00
#define BMP280_LOWPOWER_MODE	             0x01
#define BMP280_STANDARDRESOLUTION_MODE       0x02
#define BMP280_HIGHRESOLUTION_MODE           0x03
#define BMP280_ULTRAHIGHRESOLUTION_MODE      0x04

#define BMP280_ULTRALOWPOWER_OSRS_P          BMP280_OVERSAMPLING_1X
#define BMP280_ULTRALOWPOWER_OSRS_T          BMP280_OVERSAMPLING_1X

#define BMP280_LOWPOWER_OSRS_P	             BMP280_OVERSAMPLING_2X
#define BMP280_LOWPOWER_OSRS_T	             BMP280_OVERSAMPLING_1X

#define BMP280_STANDARDRESOLUTION_OSRS_P     BMP280_OVERSAMPLING_4X
#define BMP280_STANDARDRESOLUTION_OSRS_T     BMP280_OVERSAMPLING_1X

#define BMP280_HIGHRESOLUTION_OSRS_P         BMP280_OVERSAMPLING_8X
#define BMP280_HIGHRESOLUTION_OSRS_T         BMP280_OVERSAMPLING_1X

#define BMP280_ULTRAHIGHRESOLUTION_OSRS_P    BMP280_OVERSAMPLING_16X
#define BMP280_ULTRAHIGHRESOLUTION_OSRS_T    BMP280_OVERSAMPLING_2X

#define BMP280_FILTERCOEFF_OFF               0x00
#define BMP280_FILTERCOEFF_2                 0x01
#define BMP280_FILTERCOEFF_4                 0x02
#define BMP280_FILTERCOEFF_8                 0x03
#define BMP280_FILTERCOEFF_16                0x04

#define T_INIT_MAX							20
				/* 20/16 = 1.25 ms */
#define T_MEASURE_PER_OSRS_MAX				37
				/* 37/16 = 2.3125 ms*/
#define T_SETUP_PRESSURE_MAX				10
				/* 10/16 = 0.625 ms */

/*calibration parameters */
#define BMP280_DIG_T1_LSB_REG                0x88
#define BMP280_DIG_T1_MSB_REG                0x89
#define BMP280_DIG_T2_LSB_REG                0x8A
#define BMP280_DIG_T2_MSB_REG                0x8B
#define BMP280_DIG_T3_LSB_REG                0x8C
#define BMP280_DIG_T3_MSB_REG                0x8D
#define BMP280_DIG_P1_LSB_REG                0x8E
#define BMP280_DIG_P1_MSB_REG                0x8F
#define BMP280_DIG_P2_LSB_REG                0x90
#define BMP280_DIG_P2_MSB_REG                0x91
#define BMP280_DIG_P3_LSB_REG                0x92
#define BMP280_DIG_P3_MSB_REG                0x93
#define BMP280_DIG_P4_LSB_REG                0x94
#define BMP280_DIG_P4_MSB_REG                0x95
#define BMP280_DIG_P5_LSB_REG                0x96
#define BMP280_DIG_P5_MSB_REG                0x97
#define BMP280_DIG_P6_LSB_REG                0x98
#define BMP280_DIG_P6_MSB_REG                0x99
#define BMP280_DIG_P7_LSB_REG                0x9A
#define BMP280_DIG_P7_MSB_REG                0x9B
#define BMP280_DIG_P8_LSB_REG                0x9C
#define BMP280_DIG_P8_MSB_REG                0x9D
#define BMP280_DIG_P9_LSB_REG                0x9E
#define BMP280_DIG_P9_MSB_REG                0x9F

#define BMP280_CHIPID_REG                    0xD0  /*Chip ID Register */
#define BMP280_RESET_REG                     0xE0  /*Softreset Register */
#define BMP280_STATUS_REG                    0xF3  /*Status Register */
#define BMP280_CTRLMEAS_REG                  0xF4  /*Ctrl Measure Register */
#define BMP280_CONFIG_REG                    0xF5  /*Configuration Register */
#define BMP280_PRESSURE_MSB_REG              0xF7  /*Pressure MSB Register */
#define BMP280_PRESSURE_LSB_REG              0xF8  /*Pressure LSB Register */
#define BMP280_PRESSURE_XLSB_REG             0xF9  /*Pressure XLSB Register */
#define BMP280_TEMPERATURE_MSB_REG           0xFA  /*Temperature MSB Reg */
#define BMP280_TEMPERATURE_LSB_REG           0xFB  /*Temperature LSB Reg */
#define BMP280_TEMPERATURE_XLSB_REG          0xFC  /*Temperature XLSB Reg */

/* Status Register */
#define BMP280_STATUS_REG_MEASURING__POS           3
#define BMP280_STATUS_REG_MEASURING__MSK           0x08
#define BMP280_STATUS_REG_MEASURING__LEN           1
#define BMP280_STATUS_REG_MEASURING__REG           BMP280_STATUS_REG

#define BMP280_STATUS_REG_IMUPDATE__POS            0
#define BMP280_STATUS_REG_IMUPDATE__MSK            0x01
#define BMP280_STATUS_REG_IMUPDATE__LEN            1
#define BMP280_STATUS_REG_IMUPDATE__REG            BMP280_STATUS_REG

/* Control Measurement Register */
#define BMP280_CTRLMEAS_REG_OSRST__POS             5
#define BMP280_CTRLMEAS_REG_OSRST__MSK             0xE0
#define BMP280_CTRLMEAS_REG_OSRST__LEN             3
#define BMP280_CTRLMEAS_REG_OSRST__REG             BMP280_CTRLMEAS_REG

#define BMP280_CTRLMEAS_REG_OSRSP__POS             2
#define BMP280_CTRLMEAS_REG_OSRSP__MSK             0x1C
#define BMP280_CTRLMEAS_REG_OSRSP__LEN             3
#define BMP280_CTRLMEAS_REG_OSRSP__REG             BMP280_CTRLMEAS_REG

#define BMP280_CTRLMEAS_REG_MODE__POS              0
#define BMP280_CTRLMEAS_REG_MODE__MSK              0x03
#define BMP280_CTRLMEAS_REG_MODE__LEN              2
#define BMP280_CTRLMEAS_REG_MODE__REG              BMP280_CTRLMEAS_REG

/* Configuation Register */
#define BMP280_CONFIG_REG_TSB__POS                 5
#define BMP280_CONFIG_REG_TSB__MSK                 0xE0
#define BMP280_CONFIG_REG_TSB__LEN                 3
#define BMP280_CONFIG_REG_TSB__REG                 BMP280_CONFIG_REG

#define BMP280_CONFIG_REG_FILTER__POS              2
#define BMP280_CONFIG_REG_FILTER__MSK              0x1C
#define BMP280_CONFIG_REG_FILTER__LEN              3
#define BMP280_CONFIG_REG_FILTER__REG              BMP280_CONFIG_REG

#define BMP280_CONFIG_REG_SPI3WEN__POS             0
#define BMP280_CONFIG_REG_SPI3WEN__MSK             0x01
#define BMP280_CONFIG_REG_SPI3WEN__LEN             1
#define BMP280_CONFIG_REG_SPI3WEN__REG             BMP280_CONFIG_REG

/* Data Register */
#define BMP280_PRESSURE_XLSB_REG_DATA__POS         4
#define BMP280_PRESSURE_XLSB_REG_DATA__MSK         0xF0
#define BMP280_PRESSURE_XLSB_REG_DATA__LEN         4
#define BMP280_PRESSURE_XLSB_REG_DATA__REG         BMP280_PRESSURE_XLSB_REG

#define BMP280_TEMPERATURE_XLSB_REG_DATA__POS      4
#define BMP280_TEMPERATURE_XLSB_REG_DATA__MSK      0xF0
#define BMP280_TEMPERATURE_XLSB_REG_DATA__LEN      4
#define BMP280_TEMPERATURE_XLSB_REG_DATA__REG      BMP280_TEMPERATURE_XLSB_REG

#define BMP280_WR_FUNC_PTR\
	char (*bus_write)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

#define BMP280_RD_FUNC_PTR\
	char (*bus_read)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

#define BMP280_MDELAY_DATA_TYPE BMP280_U16_t

/** this structure holds all device specific calibration parameters */
struct bmp280_calibration_param_t {
	BMP280_U16_t dig_T1;
	BMP280_S16_t dig_T2;
	BMP280_S16_t dig_T3;
	BMP280_U16_t dig_P1;
	BMP280_S16_t dig_P2;
	BMP280_S16_t dig_P3;
	BMP280_S16_t dig_P4;
	BMP280_S16_t dig_P5;
	BMP280_S16_t dig_P6;
	BMP280_S16_t dig_P7;
	BMP280_S16_t dig_P8;
	BMP280_S16_t dig_P9;

	BMP280_S32_t t_fine;
};
/** BMP280 image registers data structure */
struct bmp280_t {
	struct bmp280_calibration_param_t cal_param;

	unsigned char chip_id;
	unsigned char dev_addr;

	unsigned char osrs_t;
	unsigned char osrs_p;

	BMP280_WR_FUNC_PTR;
	BMP280_RD_FUNC_PTR;
	void(*delay_msec)(BMP280_MDELAY_DATA_TYPE);
};

/* Function Declarations */
BMP280_RETURN_FUNCTION_TYPE bmp280_init(struct bmp280_t *bmp280);
BMP280_RETURN_FUNCTION_TYPE bmp280_read_ut(BMP280_S32_t *utemperature);
BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T);
BMP280_RETURN_FUNCTION_TYPE bmp280_read_up(BMP280_S32_t *upressure);
BMP280_U32_t bmp280_compensate_P_int32(BMP280_S32_t adc_P);
BMP280_RETURN_FUNCTION_TYPE bmp280_read_uput(BMP280_S32_t *upressure,\
		BMP280_S32_t *utemperature);
BMP280_RETURN_FUNCTION_TYPE bmp280_read_pt(BMP280_U32_t *pressure,\
		BMP280_S32_t *temperature);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_calib_param(void);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_osrs_t(unsigned char *value);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_osrs_t(unsigned char value);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_osrs_p(unsigned char *value);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_osrs_p(unsigned char value);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_mode(unsigned char *mode);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_mode(unsigned char mode);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_softreset(void);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_spi3(unsigned char *enable_disable);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_spi3(unsigned char enable_disable);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_filter(unsigned char *value);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_filter(unsigned char value);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_standbydur(unsigned char *time);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_standbydur(unsigned char time);
BMP280_RETURN_FUNCTION_TYPE bmp280_set_workmode(unsigned char mode);
BMP280_RETURN_FUNCTION_TYPE bmp280_get_forced_uput(BMP280_S32_t *upressure,\
		BMP280_S32_t *utemperature);
BMP280_RETURN_FUNCTION_TYPE bmp280_write_register(unsigned char addr, \
	unsigned char *data, unsigned char len);
BMP280_RETURN_FUNCTION_TYPE bmp280_read_register(unsigned char addr, \
	unsigned char *data, unsigned char len);
#ifdef BMP280_ENABLE_FLOAT
double bmp280_compensate_T_double(BMP280_S32_t adc_T);
double bmp280_compensate_P_double(BMP280_S32_t adc_P);
#endif
#if defined(BMP280_ENABLE_INT64) && defined(BMP280_64BITSUPPORT_PRESENT)
BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P);
#endif
BMP280_RETURN_FUNCTION_TYPE bmp280_compute_wait_time(unsigned char \
	*v_delaytime_u8r);
#endif
