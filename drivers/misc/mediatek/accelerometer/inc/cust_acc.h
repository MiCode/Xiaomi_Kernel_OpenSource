#ifndef __CUST_ACC_H__
#define __CUST_ACC_H__

#include <linux/types.h>
#define G_CUST_I2C_ADDR_NUM 2

struct acc_hw {
	int i2c_num;		/*!< the i2c bus used by the chip */
	int direction;		/*!< the direction of the chip */
	int power_id;		/*!< the VDD LDO ID of the chip, MT6516_POWER_NONE means the power is always on */
	int power_vol;		/*!< the VDD Power Voltage used by the chip */
	int firlen;		/*!< the length of low pass filter */
	int (*power) (struct acc_hw *hw, unsigned int on, char *devname);
	unsigned char i2c_addr[G_CUST_I2C_ADDR_NUM];	/*!< i2c address list,for chips which has different addresses with different HW layout */
	int power_vio_id;	/*!< the VIO LDO ID of the chip, MT6516_POWER_NONE means the power is always on */
	int power_vio_vol;	/*!< the VIO Power Voltage used by the chip */
    bool is_batch_supported;
};

extern struct acc_hw *get_cust_acc_hw(void);
#endif
