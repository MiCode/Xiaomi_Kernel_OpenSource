#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>

#include "cactus_ov13855_ofilmmipiraw_Sensor.h"
#define PFX "CACTUS_OV13855_OFILM_eeprom"
#define LOG_INF(format, args...)	pr_err(PFX "[%s] " format, __FUNCTION__, ##args)

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

extern int iMultiReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId, u8 number);

#define USHORT             unsigned short
//#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define CACTUS_OV13855_OFILM_EEPROM_READ_ID  0xA1
#define CACTUS_OV13855_OFILM_EEPROM_WRITE_ID   0xA2//eeprom id 0xA0

#define CACTUS_OV13855_OFILM_I2C_SPEED        100
#define CACTUS_OV13855_OFILM_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048
unsigned char CACTUS_OV13855_OFILM_eeprom_data[DATA_SIZE]= {0};
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;

static bool selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > CACTUS_OV13855_OFILM_MAX_OFFSET)
        return false;
	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, CACTUS_OV13855_OFILM_EEPROM_WRITE_ID)<0)
		return false;
    return true;
}

static bool _read_cactus_ov13855_ofilm_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size ){
	int i = 0;
	int offset = addr;
	for(i = 0; i < size; i++) {
		if(!selective_read_eeprom(offset, &data[i])){
			return false;
		}

//		LOG_INF("read_eeprom proc[%d] %d\n",offset, data[i]);
//		if (i >= 1360)
//		LOG_INF("read_eeprom proc[%d] %d\n",offset, data[i]);

		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_cactus_ov13855_ofilm_eeprom( kal_uint16 addr, unsigned char* data, kal_uint32 size){
	addr = 0x801;//from the first valid data on
	size = 1372;//0x57c//the total valid data size
//	unsigned char header[9]= {0};
//	_read_cactus_ov13855_ofilm_eeprom(0x801, header, 9);

	LOG_INF("read_cactus_ov13855_ofilm_eeprom, size = %d\n", size);

	if(!get_done || last_size != size || last_offset != addr) {
		if(!_read_cactus_ov13855_ofilm_eeprom(addr, CACTUS_OV13855_OFILM_eeprom_data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			return false;
		}
	}

	memcpy(data, CACTUS_OV13855_OFILM_eeprom_data, size);
    return true;
}
