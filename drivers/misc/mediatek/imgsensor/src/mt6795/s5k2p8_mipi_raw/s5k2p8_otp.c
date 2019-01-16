#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>


#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
//extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length);
extern int iMultiReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId, u8 number);


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define EEPROM           CAT24C512
#define EEPROM_READ_ID  0xA0
#define EEPROM_WRITE_ID   0xA1
#define I2C_SPEED        400  //CAT24C512 can support 1Mhz

#define START_OFFSET     0
#define PAGE_NUM         512
#define EEPROM_PAGE_SIZE 128  //EEPROM size 512x128=65536bytes
#define MAX_OFFSET       0xffff
#define DATA_SIZE 4096
BYTE eeprom_data[DATA_SIZE]= {0};
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;

bool byte_write_eeprom(kal_uint16 addr, BYTE data )
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(data & 0xFF)};
    if(addr > MAX_OFFSET)
		return false;
	kdSetI2CSpeed(I2C_SPEED);
    if(iWriteRegI2C(pu_send_cmd, 3, EEPROM_WRITE_ID)<0) {
		//printk("byte_write_eeprom fail, addr %x data %d\n",addr,data);
		return false;
    }
	Sleep(7);
    return true;
}



/********
Be noted that once your addr are not page-algned, some data may be covered
*/
bool page_write_eeprom(kal_uint16 addr, BYTE data[], kal_uint32 size)
{
	char pu_send_cmd[EEPROM_PAGE_SIZE+2];
	int i = 0;

    if( (addr+size) > MAX_OFFSET || size > EEPROM_PAGE_SIZE)
		return false;
	kdSetI2CSpeed(I2C_SPEED);


	pu_send_cmd[0] = (char)(addr >> 8);
	pu_send_cmd[1] = (char)(addr & 0xFF);

	for(i = 0; i< size; i++) {
		pu_send_cmd[i+2] = (char)(data[i] & 0xFF);
	}
	printk("before iBurstWriteReg_multi\n");
	if(1)//iBurstWriteReg_multi(pu_send_cmd , size, EEPROM_WRITE_ID, size)<0) //only support in K2 now
		return false;
	Sleep(10);
    return true;
}


bool selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > MAX_OFFSET)
        return false;
	kdSetI2CSpeed(I2C_SPEED);

	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, EEPROM_READ_ID)<0)
		return false;
    return true;
}

bool sequential_read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size)
{
    if( (addr+size) > MAX_OFFSET || size > EEPROM_PAGE_SIZE)
        return false;
	kdSetI2CSpeed(I2C_SPEED);
	if( iMultiReadReg(addr , (u8*)data , EEPROM_READ_ID, size) <0)
		return false;
    return true;
}

static bool _wrtie_eeprom(kal_uint16 addr, BYTE data[], kal_uint32 size ){
	int i = 0;
	int offset = addr;
	for(i = 0; i < size; i++) {
		//printk("wrtie_eeprom 0x%0x %d\n",offset, data[i]);
		if(!byte_write_eeprom( offset, data[i])){
			return false;
		}
		offset++;
	}
	get_done = false;
    return true;
}
static bool _read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size ){
	int i = 0;
	int offset = addr;
	for(i = 0; i < size; i++) {
		if(!selective_read_eeprom(offset, &data[i])){
			return false;
		}
		//printk("read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}
bool read_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size){

	if(!get_done || last_size != size || last_offset != addr) {
		if(!_read_eeprom(addr, eeprom_data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			return false;
		}
	}
	memcpy(data, eeprom_data, size);
    return true;
}
bool wrtie_eeprom(kal_uint16 addr, BYTE data[],kal_uint32 size ){
	return _wrtie_eeprom(addr, data, size);
}

bool wrtie_eeprom_fast(kal_uint16 addr, BYTE data[],kal_uint32 size ){
	bool ret = false;
	int size_to_send = size;
	printk("wrtie_eeprom_fast\n");
	if( (addr&0xff) == 0 ){//align page
		#if 0
		if(size < EEPROM_PAGE_SIZE+1) {
			ret = page_write_eeprom(addr,  data, size);
		} else
		#endif
		{
			printk("before page_write_eeprom\n");
    		for(; size_to_send > 0; size_to_send -= EEPROM_PAGE_SIZE) {
				ret = page_write_eeprom( addr,  data, size_to_send > EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : size_to_send);
				if(!ret) {
					break;
				}
				data+=EEPROM_PAGE_SIZE;
				printk("after page_write_eeprom %d\n",size_to_send);
			}
			printk("after page_write_eeprom\n");
		}
	} else {
        ret = _wrtie_eeprom(addr, data, size);
	}
	return ret;
}



