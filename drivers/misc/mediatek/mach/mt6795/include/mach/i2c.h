#ifndef __ARCH_I2C_H_
#define __ARCH_I2C_H_

/* #include <mach/mt_platform.h> */

/* //36000	kHz for wrapper I2C work frequency  6397 should be 24000K*/
#define I2C_CLK_WRAPPER_RATE	24000

#if (defined(CONFIG_MT8135_FPGA))
#define	CONFIG_MT_I2C_FPGA_ENABLE
#endif

#if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
#define I2C_CLK_RATE	12000	/* kHz for FPGA I2C work frequency */
#else
#define I2C_CLK_RATE	I2C_CLK_WRAPPER_RATE
#endif
/*
enum {
	MT_I2C_HS = (MT_DRIVER_FIRST << 0),
};
*/
struct mt_i2c_data {
	u16 pdn;	/* MTK clock id */
	u16 speed;	/* bus speed in kHz */
	u32 flags;
};

struct mt_i2c_msg {
	u16 addr;		/* slave address                        */
	u16 flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	u16 len;		/* msg length                           */
	u8 *buf;		/* pointer to msg data                  */
	u32 timing;
	u32 ext_flag;
};

#define I2C_A_FILTER_MSG	0x8000	/* filer out error messages     */
#define I2C_A_CHANGE_TIMING	0x4000	/* change timing paramters      */
#define I2C_MASK_FLAG	(0x00ff)
#define I2C_DMA_FLAG	(0xdead2000)
#define I2C_WR_FLAG		(0x1000)
#define I2C_RS_FLAG		(0x0800)
#define I2C_HS_FLAG   (0x0400)
#define I2C_ENEXT_FLAG (0x0200)
#define I2C_DISEXT_FLAG (0x0000)
#define I2C_POLL_FLAG (0x4000)
#define I2C_CH2_FLAG	(0x8000)

#define I2C_PUSHPULL_FLAG (0x00000002)
#define I2C_3DCAMERA_FLAG (0x00000004)
#define I2C_DIRECTION_FLAG (0x00000008)

int mtk_i2c_master_send(const struct i2c_client *client,
			const char *buf, int count, u32 ext_flag, u32 timing);

int mtk_i2c_master_recv(const struct i2c_client *client, char *buf, int count, u32 ext_flag,
			u32 timing);
int mtk_i2c_transfer(struct i2c_adapter *adap, struct mt_i2c_msg *msgs, int num);


#endif				/* __ARCH_I2C_H_ */
