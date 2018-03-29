#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>


/*===FEATURE SWITH===*/
/* #define FPTPDAFSUPPORT */
/* #define FANPENGTAO */
 #define LOG_INF LOG_INF_LOD
/*===FEATURE SWITH===*/

/****************************Modify Following Strings for Debug****************************/
#define PFX "S5K3L8PDAF"
#define LOG_INF_NEW(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF_LOD(format, args...)    pr_info(format, ##args)
#define LOG_1 LOG_INF("S5K3L8,MIPI 4LANE\n")
#define LOG_INF LOG_INF_LOD
#define SENSORDB LOG_INF
/****************************   Modify end    *******************************************/

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iMultiReadReg(u16 a_u2Addr , u8 *a_puBuff , u16 i2cId, u16 number);


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms)          mdelay(ms)

/**************  CONFIG BY SENSOR >>> ************/
#define EEPROM_WRITE_ID   0xa0
#define I2C_SPEED        400
#define MAX_OFFSET		    0xFFFF
#define DATA_SIZE         1406 /*1404*/
#define START_ADDR        0x0765
#define START_ADDR1       0x0765
#define START_ADDR2       0x0957
/*#define EEPROM_PAGE_SIZE  (512x128) // EEPROM size */


BYTE S5K3L8_eeprom_data[DATA_SIZE] = {0};

/**************  CONFIG BY SENSOR <<< ************/

static bool _sequential_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
/*char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };*/
	if ((addr+size) > MAX_OFFSET || size > 65536)
			return false;
	kdSetI2CSpeed(I2C_SPEED);

	if (iMultiReadReg(addr , (u8 *)data , EEPROM_WRITE_ID, size) < 0)
		return false;

	return true;
}

bool S5K3L8_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = START_ADDR;
	size = DATA_SIZE;
	printk("Read EEPROM, addr = 0x%x, size = %d\n", addr, size);

	if (!_sequential_read_eeprom(START_ADDR1, data, size)) {
		printk("error:read_eeprom from 0 fail!\n");
		return false;
	}

	memmove(data + 496, data + 498, 908);

	return true;
}
