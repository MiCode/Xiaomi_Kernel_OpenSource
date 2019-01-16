#ifndef __CUST_GYRO_H__
#define __CUST_GYRO_H__

#include <linux/types.h>

#define GYRO_CUST_I2C_ADDR_NUM 2

struct gyro_hw {
    unsigned short addr;
    int i2c_num;    /*!< the i2c bus used by the chip */
    int direction;  /*!< the direction of the chip */
    int power_id;   /*!< the LDO ID of the chip, MT6516_POWER_NONE means the power is always on*/
    int power_vol;  /*!< the Power Voltage used by the chip */
    int firlen;     /*!< the length of low pass filter */
    int (*power)(struct gyro_hw *hw, unsigned int on, char *devname);
    unsigned char	i2c_addr[GYRO_CUST_I2C_ADDR_NUM]; /*!< i2c address list,for chips which has different addresses with different HW layout */
    int power_vio_id;   /*!< the LDO ID of the chip, MT6516_POWER_NONE means the power is always on*/
    int power_vio_vol;  /*!< the Power Voltage used by the chip */
    bool is_batch_supported;
};

extern struct gyro_hw* get_cust_gyro_hw(void);
#endif 
