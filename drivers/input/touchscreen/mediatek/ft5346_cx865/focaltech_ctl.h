#ifndef __FOCALTECH_CTL_H__
#define __FOCALTECH_CTL_H__
/*****************************************************************************/
#define FT_RW_IIC_DRV                        "ft_rw_iic_drv"
#define FT_RW_IIC_DRV_MAJOR             210   /*预设的ft_rw_iic_drv的主设备号*/

#define FT_I2C_RDWR_MAX_QUEUE        36
#define FT_I2C_SLAVEADDR                    11
#define FT_I2C_RW                                  12

typedef struct ft_rw_i2c
{
    u8 *buf;    
    u8 flag;                            /*0-write 1-read*/
    __u16 length;                       /*the length of data*/
}*pft_rw_i2c;

typedef struct ft_rw_i2c_queue
{
    struct ft_rw_i2c __user *i2c_queue;
    int queuenum;   
}*pft_rw_i2c_queue;

int ft_rw_iic_drv_init(struct i2c_client *client);
void  ft_rw_iic_drv_exit(void);
/*****************************************************************************/
#endif