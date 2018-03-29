/************************************************************************
* File Name: focaltech_ctl.h
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: Function for APK tool
*
************************************************************************/
#ifndef __FOCALTECH_CTL_H__
#define __FOCALTECH_CTL_H__

#define FTS_RW_IIC_DRV		"ft_rw_iic_drv"
#define FTS_RW_IIC_DRV_MAJOR	210
#define FTS_I2C_RDWR_MAX_QUEUE	36
#define FTS_I2C_SLAVEADDR	11
#define FTS_I2C_RW		12

typedef struct fts_rw_i2c {
	u8 *buf;
	u8 flag;	/*0-write 1-read*/
	__u16 length;   /*the length of data*/
} *pfts_rw_i2c;

typedef struct fts_rw_i2c_queue {
	struct fts_rw_i2c __user *i2c_queue;
	int queuenum;
} *pfts_rw_i2c_queue;

int fts_rw_iic_drv_init(struct i2c_client *client);
void  fts_rw_iic_drv_exit(void);
#endif
