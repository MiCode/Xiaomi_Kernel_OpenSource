//********************************************************************************
//
//		LC89821x Interface module
//
//	    Program Name	: AfInter.c
//		Design			: Rex.Tang
//		History			: First edition						2013.07.13 Rex.Tang
//
//		Description		: User needs to complete the interface functions
//********************************************************************************

//#define DEBUG_LOG
#ifdef DEBUG_LOG
#include <linux/fs.h>
#endif
#include	<linux/i2c.h>
#include 	<linux/delay.h>
#define		DeviceAddr		0xE4  	// Device address of driver IC


#ifdef DEBUG_LOG
#define AF_REGDUMP "REGDUMP"
#define LOG_INF(format, args...) pr_info(AF_REGDUMP " " format, ##args)
#endif

extern int s4AF_WriteReg_LC898212XDAF(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int s4AF_ReadReg_LC898212XDAF(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

/*--------------------------------------------------------
  	IIC wrtie 2 bytes function
  	Parameters:	addr, data
--------------------------------------------------------*/
void RamWriteA(unsigned short addr, unsigned short data)
{
        #ifdef DEBUG_LOG
        LOG_INF("RAMW\t%x\t%x\n", addr, data );
        #endif
       // To call your IIC function here
	u8 puSendCmd[3] = {(u8)(addr & 0xFF) ,(u8)(data >> 8),(u8)(data & 0xFF)};        
	s4AF_WriteReg_LC898212XDAF(puSendCmd , sizeof(puSendCmd), DeviceAddr);
        
}


/*------------------------------------------------------
  	IIC read 2 bytes function
  	Parameters:	addr, *data
-------------------------------------------------------*/                   		
void RamReadA(unsigned short addr, unsigned short *data)
{
      // To call your IIC function here
	u8 buf[2];
	u8 puSendCmd[1] = {(u8)(addr & 0xFF) };
	s4AF_ReadReg_LC898212XDAF(puSendCmd , sizeof(puSendCmd), buf, 2, DeviceAddr);
	*data = (buf[0] << 8) | (buf[1] & 0x00FF);
        #ifdef DEBUG_LOG
        LOG_INF("RAMR\t%x\t%x\n", addr, *data );
        #endif
}


/*--------------------------------------------------------
  	IIC wrtie 1 byte function
  	Parameters:	addr, data
--------------------------------------------------------*/
void RegWriteA(unsigned short addr, unsigned char data)
{
        #ifdef DEBUG_LOG
        LOG_INF("REGW\t%x\t%x\n", addr, data );
        #endif
      // To call your IIC function here
	u8 puSendCmd[2] = {(u8)(addr & 0xFF) ,(u8)(data & 0xFF)};        
	s4AF_WriteReg_LC898212XDAF(puSendCmd , sizeof(puSendCmd), DeviceAddr);
}


/*--------------------------------------------------------
  	IIC read 1 byte function
  	Parameters:	addr, *data
--------------------------------------------------------*/
void RegReadA(unsigned short addr, unsigned char *data)
{
     // To call your IIC function here
	u8 puSendCmd[1] = {(u8)(addr & 0xFF) };
	s4AF_ReadReg_LC898212XDAF(puSendCmd , sizeof(puSendCmd), data, 1, DeviceAddr);     

        #ifdef DEBUG_LOG
        LOG_INF("REGR\t%x\t%x\n", addr, *data );
        #endif
}


/*--------------------------------------------------------
  	Wait function
  	Parameters:	msec
--------------------------------------------------------*/
void WaitTime(unsigned short msec)
{
     // To call your Wait function here
     usleep_range(msec * 1000, (msec + 1) * 1000);     
}

