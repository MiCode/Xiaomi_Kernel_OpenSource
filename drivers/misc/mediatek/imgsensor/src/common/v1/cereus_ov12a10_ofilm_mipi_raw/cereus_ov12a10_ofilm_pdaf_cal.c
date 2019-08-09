#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/proc_fs.h> 
#include <linux/dma-mapping.h>
#include <linux/types.h>

#include "cereus_ov12a10_ofilm_mipiraw_Sensor.h"

#define PFX "cereus_ov12a10_ofilm_pdaf_cal"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
//extern void kdSetI2CSpeed(u16 i2cSpeed);
#define EEPROM_READ_ID  0xA1
#define EEPROM_WRITE_ID   0xA0
#define I2C_SPEED        400  //CAT24C512 can support 1Mhz
#define START_OFFSET     0x0801
#define DATA_SIZE        (496+876)

static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;
static bool cereus_ov12a10_ofilm_selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	//kdSetI2CSpeed(I2C_SPEED);
	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, EEPROM_WRITE_ID)<0)
		return false;
        return true;
}

static bool cereus_ov12a10_ofilm_read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size ){
	int i = 0;
	int offset = START_OFFSET;
	int dataSize = DATA_SIZE;
	if (dataSize > size)
		dataSize = size;

	LOG_INF("BUFFER SIZE: 0x%x\n",size);
	for(i = 0; i < dataSize; i++) {
		if(!cereus_ov12a10_ofilm_selective_read_eeprom(offset, &data[i])){
			printk("read_eeprom 0x%0x %d fail \n",offset, data[i]);
			return false;
		}
		offset++;
	}
	if (size > dataSize)
		memset(&data[i],0x00,size-dataSize);
	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_otp_pdaf_data( kal_uint16 addr, BYTE* data, kal_uint32 size){
	
	LOG_INF("read_otp_pdaf_data enter");
	if(!get_done || last_size != size || last_offset != addr) {
		if(!cereus_ov12a10_ofilm_read_eeprom(addr, data, size)){
			get_done = 0;
           		last_size = 0;
                        last_offset = 0;
			printk("read_otp_pdaf_data fail");
			return false;
		}
	}
	LOG_INF("read_otp_pdaf_data end");
    return true;
}
