/*
 * Definitions for qmc5983 magnetic sensor chip.
 */
	 
#ifndef __QMC5983_H__
#define __QMC5983_H__
	 
#include <linux/ioctl.h>  /* For IOCTL macros */
	 
#define QMC5983_IOCTL_BASE 'm'
	 /* The following define the IOCTL command values via the ioctl macros */
#define QMC5983_SET_RANGE		_IOW(QMC5983_IOCTL_BASE, 1, int)
#define QMC5983_SET_MODE		_IOW(QMC5983_IOCTL_BASE, 2, int)
#define QMC5983_SET_BANDWIDTH	_IOW(QMC5983_IOCTL_BASE, 3, int)
#define QMC5983_READ_MAGN_XYZ	_IOR(QMC5983_IOCTL_BASE, 4, int)	
#define QMC5983_SET_REGISTER_A	_IOW(QMC5983_IOCTL_BASE, 5, char *)
#define QMC5983_SELF_TEST	   _IOWR(QMC5983_IOCTL_BASE, 6, char *)

/*-------------------------------------------------------------------*/
	 /* Magnetometer registers mapping */
#define CRA_REG_M	0x00  /* Configuration register A */
#define CRB_REG_M	0x01  /* Configuration register B */
#define MR_REG_M	0x02  /* Mode register */
	 
	 /* Output register start address*/
#define OUT_X_M		0x03
#define OUT_X_L		0x04
#define OUT_Z_M		0x05
#define OUT_Z_L		0x06
#define OUT_Y_M		0x07
#define OUT_Y_L		0x08





	 
	 /* QMC5983 magnetometer identification registers */
#define IRA_REG_M	0x0A
#define IRB_REG_M   0x0B
#define IRC_REG_M   0x0C
	 
	 /* Magnetometer XYZ sensitivity  */
#define GAIN_0	1370	/* XYZ sensitivity at 0.88G */
#define GAIN_1	1090	/* XYZ sensitivity at 1.3G */
#define GAIN_2	820		/* XYZ sensitivity at 1.9G */
#define GAIN_3	660		/* XYZ sensitivity at 2.5G */
#define GAIN_4	440		/* XYZ sensitivity at 4.0G */
#define GAIN_5	390		/* XYZ sensitivity at 4.7G */
#define GAIN_6	330		/* XYZ sensitivity at 5.6G */
#define GAIN_7	230		/* XYZ sensitivity at 8.1G */
	 
	 /*Status registers */
#define SR_REG_M    0x09
	 
	 /* Temperature registers */
#define TO_MSB_T 0x31
#define TO_LSB_T 0x32
	 
	 /* Average per measurement output and output rate 15hz*/
#define AE_REG_0 0x10 /* XYZ output average at 1*/
#define AE_REG_1 0x30 /* XYZ output average at 2*/
#define AE_REG_2 0x50 /* XYZ output average at 4*/
#define AE_REG_3 0x70 /* XYZ output average at 8*/
	 
	 /* I2C typical output rate and average per 8 */
#define RATE_REG_0 0x60  /* Typical output rate at 0.75HZ */
#define RATE_REG_1 0x64  /* Typical output rate at 1.5HZ */
#define RATE_REG_2 0x68  /* Typical output rate at 3HZ */
#define RATE_REG_3 0x6c  /* Typical output rate at 7.5HZ */
#define RATE_REG_4 0x70  /* Typical output rate at 15HZ */
#define RATE_REG_5 0x74  /* Typical output rate at 30HZ */
#define RATE_REG_6 0x78  /* Typical output rate at 75HZ */
#define RATE_REG_7 0x7c  /* Typical output rate at 220HZ */


	 
	 /************************************************/
	 /*  Magnetometer section defines		 */
	 /************************************************/
	 
	 /* Magnetometer Sensor Full Scale */
#define QMC5983_0_88G		0x00
#define QMC5983_1_3G		0x20
#define QMC5983_1_9G		0x40
#define QMC5983_2_5G		0x60
#define QMC5983_4_0G		0x80
#define QMC5983_4_7G		0xA0
#define QMC5983_5_6G		0xC0
#define QMC5983_8_1G		0xE0
	 
	 /* Magnetic Sensor Operating Mode */
#define QMC5983_NORMAL_MODE	0x00
#define QMC5983_POS_BIAS	0x01
#define QMC5983_NEG_BIAS	0x02
#define QMC5983_CC_MODE		0x00
#define QMC5983_SC_MODE		0x01
#define QMC5983_IDLE_MODE	0x02
#define QMC5983_SLEEP_MODE	0x03
	 
	 /* Magnetometer output data rate  */
#define QMC5983_ODR_75		0x00	/* 0.75Hz output data rate */
#define QMC5983_ODR1_5		0x04	/* 1.5Hz output data rate */
#define QMC5983_ODR3_0		0x08	/* 3Hz output data rate */
#define QMC5983_ODR7_5		0x0C	/* 7.5Hz output data rate */
#define QMC5983_ODR15		0x10	/* 15Hz output data rate */
#define QMC5983_ODR30		0x14	/* 30Hz output data rate */
#define QMC5983_ODR75		0x18	/* 75Hz output data rate */
#define QMC5983_ODR220		0x1C	/* 220Hz output data rate */
	 
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
#define CONVERT_O_DIV		1			// 1/64 = CONVERT_O

  
#ifdef __KERNEL__
	 
#if 0 /*use mediatek's layout setting*/
	 struct QMC5983_platform_data {
	 
		 u8 h_range;
	 
		 u8 axis_map_x;
		 u8 axis_map_y;
		 u8 axis_map_z;
	 
		 u8 negate_x;
		 u8 negate_y;
		 u8 negate_z;
	 
		 int (*init)(void);
		 void (*exit)(void);
		 int (*power_on)(void);
		 int (*power_off)(void);
	 
	 };
#endif

#endif /* __KERNEL__ */
	 
	 
#endif  /* __QMC5983_H__ */

