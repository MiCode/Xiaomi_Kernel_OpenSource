#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
//#include <asm/system.h>

#include <linux/proc_fs.h> 


#include <linux/dma-mapping.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#include "ov13853mipiraw_Sensor.h"
/****************************Modify Following Strings for Debug****************************/
#define PFX "OV13853_camera_pdaf"
//#define LOG_1 LOG_INF("OV13853,MIPI 4LANE\n")
//#define LOG_2 LOG_INF("preview 2096*1552@30fps,640Mbps/lane; video 4192*3104@30fps,1.2Gbps/lane; capture 13M@30fps,1.2Gbps/lane\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

//#include "ov13853_otp.h"
struct otp_pdaf_struct {
unsigned char pdaf_flag; //bit[7]--0:empty; 1:Valid
unsigned char data1[496];//output data1
unsigned char data2[806];//output data2
unsigned char data3[102];//output data3
unsigned char pdaf_checksum;//checksum of pd, SUM(0x0801~0x0D7C)%255+1

};




extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
#define EEPROM_READ_ID  0xA1
#define EEPROM_WRITE_ID   0xA0
#define I2C_SPEED        400  //CAT24C512 can support 1Mhz

#define START_OFFSET     0x79B

#define Delay(ms)  mdelay(ms)
//static unsigned char OV13853MIPI_WRITE_ID = (0xA0 >> 1);
//#define EEPROM_READ_ID  0xA3
//#define EEPROM_WRITE_ID   0xA2
#define MAX_OFFSET       0xd7b
#define DATA_SIZE 4096
//BYTE eeprom_data[DATA_SIZE]= {0};
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;
/*
kal_uint16 OV13853_R2A_read_i2c(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,EEPROM_READ_ID);
    return get_byte;
}



kal_uint16 OV13853_R2A_write_i2c(addr, para)
{
		iWriteReg((u16) addr , (u32) para , 1, EEPROM_WRITE_ID);
		return 1;
}




//read pdaf data
int read_otp_pdaf_data(kal_uint16 addr, BYTE* data, kal_uint32 size)
{
	int i = 0;
	LOG_INF("addr :%x, size:%d, \n", addr, size);
	addr = 0x800;
	for (i=0; i<1404;i++)
	{
		data[i] = OV13853_R2A_read_i2c(addr);
		addr++;
		LOG_INF("data[%d] :%x, addr:%x, \n", i, data[i], addr);
    }
	return 0;
}
*/

////

static bool OV13853_selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > MAX_OFFSET)
        return false;
	kdSetI2CSpeed(I2C_SPEED);

	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, EEPROM_WRITE_ID)<0)
		return false;
    return true;
}

int OV13853_selective_read_eeprom_VCM_ID(kal_uint16 addr)
{
	int data = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > MAX_OFFSET)
        return false;
	kdSetI2CSpeed(I2C_SPEED);
	iReadRegI2C(pu_send_cmd, 2, (u8*)&data, 1, EEPROM_WRITE_ID);
		return data;
}


static bool OV13853_read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size ){
	int i = 0;
	//int offset = addr;
	int offset = 0x79B;
	//for(i = 0; i < 1404; i++) {
	for(i = 0; i < 1372; i++) {
		if(!OV13853_selective_read_eeprom(offset, &data[i])){
			LOG_INF("read_eeprom 0x%0x %d fail \n",offset, data[i]);
			return false;
		}
		LOG_INF("read_eeprom 0x%0x 0x%x\n",offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_otp_pdaf_data( kal_uint16 addr, BYTE* data, kal_uint32 size){
	
	LOG_INF("read_otp_pdaf_data enter");
	if(!get_done || last_size != size || last_offset != addr) {
		//if(!_read_eeprom(addr, eeprom_data, size)){
		if(!OV13853_read_eeprom(addr, data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			LOG_INF("read_otp_pdaf_data fail");
			return false;
		}
	}
	//memcpy(data, eeprom_data, size);
	LOG_INF("read_otp_pdaf_data end");
    return true;
}

//
