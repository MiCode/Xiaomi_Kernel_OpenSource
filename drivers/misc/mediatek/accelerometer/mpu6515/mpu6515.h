#ifndef MPU6515_H
#define MPU6515_H

#include <linux/ioctl.h>

#define MPU6515_I2C_SLAVE_ADDR		0xD0 // or 0xD1


/* MPU6515 Register Map  (Please refer to MPU6515 Specifications) */
#define MPU6515_REG_DEVID			0x75
#define	MPU6515_REG_BW_RATE			0x1A
#define MPU6515_REG_POWER_CTL       0x6B
#define MPU6515_REG_POWER_CTL2      0x6C
#define MPU6515_REG_INT_ENABLE		0x38
#define MPU6515_REG_DATA_FORMAT		0x1C
#define MPU6515_REG_DATAX0			0x3B
#define MPU6515_REG_DATAY0			0x3D
#define MPU6515_REG_DATAZ0			0x3F
#define MPU6515_REG_RESET               0x68

/* register Value */ 
#define MPU6515_FIXED_DEVID         0x74
													
                                           // delay(ms)	
#define MPU6515_BW_460HZ            0x00   //1.94
#define MPU6515_BW_184HZ            0x01   //5.8
#define MPU6515_BW_92HZ             0x02   //7.8
#define MPU6515_BW_41HZ             0x03   //11.8
#define MPU6515_BW_20HZ             0x04   //19.8
#define MPU6515_BW_10HZ             0x05   //35.7
#define MPU6515_BW_5HZ              0x06   //66.96

#define MPU6515_DEV_RESET           0x80

//#define MPU6515_FULL_RES			0x08
#define MPU6515_RANGE_2G			(0x00 << 3)
#define MPU6515_RANGE_4G			(0x01 << 3)
#define MPU6515_RANGE_8G			(0x02 << 3)
#define MPU6515_RANGE_16G			(0x03 << 3)
//#define MPU6515_SELF_TEST         0x80


#define MPU6515_SLEEP				0x40	//enable low power sleep mode



// below do not modify	 
#define MPU6515_SUCCESS                     0
#define MPU6515_ERR_I2C                     -1
#define MPU6515_ERR_STATUS                  -3
#define MPU6515_ERR_SETUP_FAILURE           -4
#define MPU6515_ERR_GETGSENSORDATA          -5
#define MPU6515_ERR_IDENTIFICATION          -6

#define MPU6515_BUFSIZE				256

#define MPU6515_AXES_NUM        3

/*----------------------------------------------------------------------------*/
typedef enum{
    MPU6515_CUST_ACTION_SET_CUST = 1,
    MPU6515_CUST_ACTION_SET_CALI,
    MPU6515_CUST_ACTION_RESET_CALI
}CUST_ACTION;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
}MPU6515_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    part;
    int32_t     data[0];
}MPU6515_SET_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     data[MPU6515_AXES_NUM];
}MPU6515_SET_CALI;
/*----------------------------------------------------------------------------*/
typedef MPU6515_CUST MPU6515_RESET_CALI;
/*----------------------------------------------------------------------------*/
typedef union
{
    uint32_t                data[10];
    MPU6515_CUST         cust;
    MPU6515_SET_CUST     setCust;
    MPU6515_SET_CALI     setCali;
    MPU6515_RESET_CALI   resetCali;
}MPU6515_CUST_DATA;
/*----------------------------------------------------------------------------*/

#endif

