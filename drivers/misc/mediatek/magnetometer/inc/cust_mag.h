#ifndef __CUST_MAG_H__
#define __CUST_MAG_H__

#include <linux/types.h>

#define M_CUST_I2C_ADDR_NUM 2

struct mag_hw {
    int i2c_num;
    int direction;
    int power_id;
    int power_vol;
    unsigned char	i2c_addr[M_CUST_I2C_ADDR_NUM]; /*!< i2c address list,for chips which has different addresses with different HW layout */
    int power_vio_id;
    int power_vio_vol;
    bool is_batch_supported;
};

extern struct mag_hw* get_cust_mag_hw(void);
#endif 
