/*
 * Definitions for qmc6983 magnetic sensor chip.
 */
	 
#ifndef __QMC6983_H__
#define __QMC6983_H__
	 
#include <linux/ioctl.h>  /* For IOCTL macros */

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define QMC6983_IOCTL_BASE 'm'
	 /* The following define the IOCTL command values via the ioctl macros */
#define QMCX983_SET_RANGE		_IOW(QMC6983_IOCTL_BASE, 1, int)
#define QMCX983_SET_MODE		_IOW(QMC6983_IOCTL_BASE, 2, int)
#define QMCX983_SET_BANDWIDTH	_IOW(QMC6983_IOCTL_BASE, 3, int)
#define QMCX983_READ_MAGN_XYZ	_IOR(QMC6983_IOCTL_BASE, 4, int)	
#define QMCX983_SET_REGISTER_A	_IOW(QMC6983_IOCTL_BASE, 5, char *)
#define QMCX983_SELF_TEST	   _IOWR(QMC6983_IOCTL_BASE, 6, char *)

#ifdef CONFIG_COMPAT
#define COMPAT_QMCX983_SET_RANGE		_IOW(QMC6983_IOCTL_BASE, 1, compat_int_t)
#define COMPAT_QMCX983_SET_MODE		_IOW(QMC6983_IOCTL_BASE, 2, compat_int_t)
#define COMPAT_QMCX983_SET_BANDWIDTH	_IOW(QMC6983_IOCTL_BASE, 3, compat_int_t)
#define COMPAT_QMCX983_READ_MAGN_XYZ	_IOR(QMC6983_IOCTL_BASE, 4, compat_int_t)	
#define COMPAT_QMCX983_SET_REGISTER_A	_IOW(QMC6983_IOCTL_BASE, 5, compat_uptr_t)
#define COMPAT_QMCX983_SELF_TEST	   _IOWR(QMC6983_IOCTL_BASE, 6, compat_uptr_t)
#endif
//extern struct mag_hw* qmcX983_get_cust_mag_hw(void);


/*-------------------------------------------------------------------*/
	 /* Magnetometer registers mapping */

#define QMCX983_SETRESET_FREQ_FAST  1
#define RWBUF_SIZE      16
/* Magnetometer registers */
#define CTL_REG_ONE	0x09  /* Contrl register one */
#define CTL_REG_TWO	0x0a  /* Contrl register two */

/* Output register start address*/
#define OUT_X_REG		0x00

/*Status registers */
#define STA_REG_ONE    0x06
#define STA_REG_TWO    0x0c

/* Temperature registers */
#define TEMP_H_REG 		0x08
#define TEMP_L_REG 		0x07

/*different from qmc6983,the ratio register*/
#define RATIO_REG		0x0b

 
/************************************************/
/* 	Magnetometer section defines	 	*/
/************************************************/

/* Magnetic Sensor Operating Mode */
#define QMCX983_STANDBY_MODE	0x00
#define QMCX983_CC_MODE			0x01
#define QMCX983_SELFTEST_MODE	0x02
#define QMCX983_RESERVE_MODE	0x03


/* Magnetometer output data rate  */
#define QMCX983_ODR_10		0x00	/* 0.75Hz output data rate */
#define QMCX983_ODR_50		0x01	/* 1.5Hz output data rate */
#define QMCX983_ODR_100		0x02	/* 3Hz output data rate */
#define QMCX983_ODR7_200	0x03	/* 7.5Hz output data rate */


/* Magnetometer full scale  */
#define QMCX983_RNG_2G		0x00
#define QMCX983_RNG_8G		0x01
#define QMCX983_RNG_12G		0x02
#define QMCX983_RNG_20G		0x03

#define RNG_2G		2
#define RNG_8G		8
#define RNG_12G		12
#define RNG_20G		20

/*data output register*/
#define OUT_X_M		0x01
#define OUT_X_L		0x00
#define OUT_Z_M		0x05
#define OUT_Z_L		0x04
#define OUT_Y_M		0x03
#define OUT_Y_L		0x02

#define SET_RATIO_REG   0x0b

/*data output rate HZ*/
#define DATA_OUTPUT_RATE_10HZ 	0x00
#define DATA_OUTPUT_RATE_50HZ 	0x01
#define DATA_OUTPUT_RATE_100HZ 	0x02
#define DATA_OUTPUT_RATE_200HZ 	0x03

/*oversample Ratio */
#define OVERSAMPLE_RATIO_512 	0x00
#define OVERSAMPLE_RATIO_256 	0x01
#define OVERSAMPLE_RATIO_128 	0x02
#define OVERSAMPLE_RATIO_64 	0x03


  #define SAMPLE_AVERAGE_8		(0x3 << 5)
  #define OUTPUT_RATE_75		(0x6 << 2)
  #define MEASURE_NORMAL		0
  #define MEASURE_SELFTEST		0x1
#define GAIN_DEFAULT		  (3 << 5)


// conversion of magnetic data (for bmm050) to uT units
// conversion of magnetic data to uT units
// 32768 = 1Guass = 100 uT
// 100 / 32768 = 25 / 8096
// 65536 = 360Degree
// 360 / 65536 = 45 / 8192
#define CONVERT_M			6
#define CONVERT_M_DIV		100			// 6/100 = CONVERT_M
#define CONVERT_O			1
#define CONVERT_O_DIV		100			// 1/64 = CONVERT_O
#define CONVERT_Q16			1
#define CONVERT_Q16_DIV		65536		// 1/64 = CONVERT_Gyro
	 
	 
#endif  /* __QMC6983_H__ */

