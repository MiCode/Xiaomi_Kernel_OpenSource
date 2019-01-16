/*
 * vl6180x_port_i2c.c
 *
 *  Created on: Oct 22, 2014
 *      Author:  Teresa Tao
 */

#include "vl6180x_i2c.h"
#include <linux/i2c.h>

#if 1

#define I2C_M_WR			0x00
static struct i2c_client *pclient=NULL;

void i2c_setclient(struct i2c_client *client)
{
	pclient = client;

}
struct i2c_client* i2c_getclient()
{
	return pclient;
}

/** int  VL6180x_I2CWrite(VL6180xDev_t dev, void *buff, uint8_t len);
 * @brief       Write data buffer to VL6180x device via i2c
 * @param dev   The device to write to
 * @param buff  The data buffer
 * @param len   The length of the transaction in byte
 * @return      0 on success
 */
int VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,msg,1); //return the actual messages transfer
	if(err != 1)
	{
		pr_err("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n", __func__, err, pclient->addr, 
																				(buff[0]<<8|buff[1]));
		return -1;
	}
    return 0;
}


/** int VL6180x_I2CRead(VL6180xDev_t dev, void *buff, uint8_t len);
 * @brief       Read data buffer from VL6180x device via i2c
 * @param dev   The device to read from
 * @param buff  The data buffer to fill
 * @param len   The length of the transaction in byte
 * @return      transaction status
 */
int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
 	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_RD|pclient->flags;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,&msg[0],1); //return the actual mesage transfer
	if(err != 1)
	{
		pr_err("%s: Read i2c_transfer err:%d, addr:0x%x\n", __func__, err, pclient->addr);
		return -1;
	}
    return 0;
}

#endif
