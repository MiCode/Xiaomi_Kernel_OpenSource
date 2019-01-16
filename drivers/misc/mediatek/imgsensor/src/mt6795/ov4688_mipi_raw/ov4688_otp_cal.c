#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <asm/system.h>

#include <linux/proc_fs.h> 


#include <linux/dma-mapping.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#include "ov4688mipiraw_Sensor.h"
#include "ov4688_otp.h"

#if 1
extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define PFX "ov4688_OTP"
#define LOG_INF(format, args...)	//xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)


#define Delay(ms)  mdelay(ms)
static unsigned char ov4688MIPI_WRITE_ID = 0X00;

kal_uint16 OV4688_read_i2c(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,ov4688MIPI_WRITE_ID);
    return get_byte;
}

kal_uint16 OV4688_write_i2c(addr, para)
{
		iWriteReg((u16) addr , (u32) para , 1, ov4688MIPI_WRITE_ID);
		return 1;
}
#endif







#if 0
void otp_cali(unsigned char writeid)
{
	struct otp_struct current_otp;
	ov4688MIPI_WRITE_ID = writeid;
	memset(&current_otp, 0, sizeof(struct otp_struct));
	read_otp(&current_otp);
	apply_otp(&current_otp);
}

int Decode_13850R2A(unsigned char*pInBuf, unsigned char* pOutBuf)
{
	if(pInBuf != NULL)
	{
		LumaDecoder(pInBuf, pOutBuf);
		ColorDecoder((pInBuf+86), (pOutBuf+120));
		ColorDecoder((pInBuf+136), (pOutBuf+240));
		LOG_INF(" OTP OK \n");
		return 1;
	}
	{
		LOG_INF(" OTP FAIL \n");
		return 0;	
	}

}
// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
int read_otp(struct otp_struct *otp_ptr)
{
	
	int otp_flag=0;
	int addr=0;
	int temp=0;
	int i=0;
	int checksumLSC = 0;
	int checksumOTP = 0;
	int checksumTotal = 0;
	//int 数组大小360会编译不过
	unsigned char lenc_out[360];
	//set 0x5002[1] to "0"
	int temp1=0;
	temp1 = ov4688_R2A_read_i2c(0x5002);
	ov4688_R2A_write_i2c(0x5002, (0x00 & 0x02) | (temp1 & (~0x02)));
	// read OTP into buffer
	ov4688_R2A_write_i2c(0x3d84, 0xC0);
	ov4688_R2A_write_i2c(0x3d88, 0x72); // OTP start address
	ov4688_R2A_write_i2c(0x3d89, 0x20);
	ov4688_R2A_write_i2c(0x3d8A, 0x73); // OTP end address
	ov4688_R2A_write_i2c(0x3d8B, 0xBE);
	ov4688_R2A_write_i2c(0x3d81, 0x01); // load otp into buffer
	Delay(10);
	// OTP base information and WB calibration data
	otp_flag = ov4688_R2A_read_i2c(0x7220);
	LOG_INF(" WB calibration data : %x \n", otp_flag);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {
		addr = 0x7221; // base address of info group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7229; // base address of info group 2
	}
	if(addr != 0) {
		(*otp_ptr).flag = 0xC0; // valid info and AWB in OTP
		(*otp_ptr).module_integrator_id = ov4688_R2A_read_i2c(addr);
		(*otp_ptr).lens_id = ov4688_R2A_read_i2c( addr + 1);
		(*otp_ptr).production_year = ov4688_R2A_read_i2c( addr + 2);
		(*otp_ptr).production_month = ov4688_R2A_read_i2c( addr + 3);
		(*otp_ptr).production_day = ov4688_R2A_read_i2c(addr + 4);
		temp = ov4688_R2A_read_i2c(addr + 7);
		(*otp_ptr).rg_ratio = (ov4688_R2A_read_i2c(addr + 5)<<2) + ((temp>>6) & 0x03);
		(*otp_ptr).bg_ratio = (ov4688_R2A_read_i2c(addr + 6)<<2) + ((temp>>4) & 0x03);	
	}
	else {
		(*otp_ptr).flag = 0x00; // not info in OTP
		(*otp_ptr).module_integrator_id = 0;
		(*otp_ptr).lens_id = 0;
		(*otp_ptr).production_year = 0;
		(*otp_ptr).production_month = 0;
		(*otp_ptr).production_day = 0;
	}
	// OTP VCM Calibration
	otp_flag = ov4688_R2A_read_i2c(0x73ac);
	LOG_INF(" VCM calibration data : %x \n", otp_flag);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {
		addr = 0x73ad; // base address of VCM Calibration group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x73b0; // base address of VCM Calibration group 2
	}
	if(addr != 0) {
		(*otp_ptr).flag |= 0x20;
		temp = ov4688_R2A_read_i2c(addr + 2);
		(* otp_ptr).VCM_start = (ov4688_R2A_read_i2c(addr)<<2) | ((temp>>6) & 0x03);
		(* otp_ptr).VCM_end = (ov4688_R2A_read_i2c(addr + 1) << 2) | ((temp>>4) & 0x03);
		(* otp_ptr).VCM_dir = (temp>>2) & 0x03;
	}
	else {
		(* otp_ptr).VCM_start = 0;
		(* otp_ptr).VCM_end = 0;
		(* otp_ptr).VCM_dir = 0;
	}
	// OTP Lenc Calibration
	otp_flag = ov4688_R2A_read_i2c(0x7231);
	LOG_INF(" Lenc calibration data : %x \n", otp_flag);
	addr = 0;
	//int checksumLSC = 0, checksumOTP = 0, checksumTotal = 0;
	if((otp_flag & 0xc0) == 0x40) {
		addr = 0x7232; // base address of Lenc Calibration group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x72ef; // base address of Lenc Calibration group 2
	}
	//
	LOG_INF(" Lenc calibration addr : %x \n", addr);
if(addr != 0) {
		for(i=0;i<186;i++) {
			(* otp_ptr).lenc[i]= ov4688_R2A_read_i2c(addr + i);
			checksumLSC += (* otp_ptr).lenc[i];
			LOG_INF(" Lenc (* otp_ptr).lenc[%d] : %x \n", i, (* otp_ptr).lenc[i]);
		}
		//Decode the lenc buffer from OTP , from 186 bytes to 360 bytes
		//int lenc_out[360];
		
		for(i=0;i<360;i++)
		{
			lenc_out[i] = 0;
		}
		if(Decode_13850R2A((*otp_ptr).lenc,  lenc_out))
		{
			for(i=0;i<360;i++) 
			{
				LOG_INF(" from OTP lenc_out[%d]:%x \n", i, lenc_out[i]);
				checksumOTP = checksumOTP + lenc_out[i]; 
			}
		}

		checksumLSC = (checksumLSC)%255 +1;	
		checksumOTP = (checksumOTP)%255 +1;
		checksumTotal = (checksumLSC) ^ (checksumOTP);
		(* otp_ptr).checksumLSC=ov4688_R2A_read_i2c(addr + 186);
		(* otp_ptr).checksumOTP=ov4688_R2A_read_i2c(addr + 187);
		(* otp_ptr).checksumTotal=ov4688_R2A_read_i2c(addr + 188);
		LOG_INF(" checksumLSC-OTP-Total:%x-%x-%x;read checksumLSC-OTP-Total:%x-%x-%x\n", checksumLSC, checksumOTP,checksumTotal,(* otp_ptr).checksumLSC,(* otp_ptr).checksumOTP,(* otp_ptr).checksumTotal);
		if((* otp_ptr).checksumLSC == checksumLSC && (* otp_ptr).checksumOTP == checksumOTP){
			(*otp_ptr).flag |= 0x10;
		}
		else if((* otp_ptr).checksumTotal == checksumTotal){
			(*otp_ptr).flag |= 0x10;
		}
	}
	else {
		for(i=0;i<186;i++) {
			(* otp_ptr).lenc[i]=0;
		}
	}
	for(i=0x7220;i<=0x73be;i++) 
	{
		ov4688_R2A_write_i2c(i,0); // clear OTP buffer, recommended use continuous write to accelarate
	}

	//set 0x5002[1] to "1"
	temp1 = ov4688_R2A_read_i2c(0x5002);
	ov4688_R2A_write_i2c(0x5002, (0x02 & 0x02) | (temp1 & (~0x02)));

	return (*otp_ptr).flag;
}


// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
int apply_otp(struct otp_struct *otp_ptr)
{
	int rg=0;
	int bg=0;
	int R_gain=0;
	int G_gain=0;
	int B_gain=0;
	int Base_gain=0;
	int temp=0;
	int i=0;
	unsigned char lenc_out[360];
	LOG_INF(" apply_otp (*otp_ptr).flag : %x \n", (*otp_ptr).flag);
	// apply OTP WB Calibration
	if ((*otp_ptr).flag & 0x40) {
		LOG_INF(" apply OTP WB Calibration : %x \n", (*otp_ptr).flag);
		rg = (*otp_ptr).rg_ratio;
		bg = (*otp_ptr).bg_ratio;

		//calculate G gain
		R_gain = (RG_Ratio_Typical*1000) / rg;
		B_gain = (BG_Ratio_Typical*1000) / bg;
		G_gain = 1000;

		if (R_gain < 1000 || B_gain < 1000)
		{
			if (R_gain < B_gain)
				Base_gain = R_gain;
			else
				Base_gain = B_gain;
		}
		else
		{
			Base_gain = G_gain;
		}

		R_gain = 0x400 * R_gain / (Base_gain);
		B_gain = 0x400 * B_gain / (Base_gain);
		G_gain = 0x400 * G_gain / (Base_gain);

		// update sensor WB gain
		if (R_gain>0x400) {
			ov4688_R2A_write_i2c(0x5056, R_gain>>8);
			ov4688_R2A_write_i2c(0x5057, R_gain & 0x00ff);
		}

		if (G_gain>0x400) {
			ov4688_R2A_write_i2c(0x5058, G_gain>>8);
			ov4688_R2A_write_i2c(0x5059, G_gain & 0x00ff);
		}

		if (B_gain>0x400) {
			ov4688_R2A_write_i2c(0x505A, B_gain>>8);
			ov4688_R2A_write_i2c(0x505B, B_gain & 0x00ff);
		}
	}

	// apply OTP Lenc Calibration
	if ((*otp_ptr).flag & 0x10) {
		
		LOG_INF(" apply OTP Lenc Calibration : %x \n", (*otp_ptr).flag);
		temp = ov4688_R2A_read_i2c(0x5000);
		temp = 0x01 | temp;
		ov4688_R2A_write_i2c(0x5000, temp);

		//Decode the lenc buffer from OTP , from 186 bytes to 360 bytes
		
		for(i=0;i<360;i++)
		{
			lenc_out[i] = 0;
		}
		/***For function Decode_13850R2A(unsigned char*pInBuf, unsigned char* pOutBuf),please refer to lc42.h***/
		if(Decode_13850R2A((*otp_ptr).lenc,  lenc_out))
		{
			for(i=0;i<360 ;i++) {
				LOG_INF(" apply OTP lenc_out[%d]:%x \n", i, lenc_out[i]);
				ov4688_R2A_write_i2c(0x5200 + i, lenc_out[i]);
			}
		}
	}
}

/***********Decode LENC Para Process Start*********************/
void LumaDecoder(uint8_t *pData, uint8_t *pPara)
{
	
	uint32_t Offset, Bit, Option;
	uint32_t i, k;
	uint8_t pCenter[16], pMiddle[32], pCorner[72];
	Offset = pData[0];
	Bit = pData[1]>>4;
	Option = pData[1]&0xf;
	LOG_INF("Offset:%x, Bit:%x, Option:%x \n",Offset, Bit, Option);
	for (i=0; i<180; i++)
	{
		LOG_INF("data-pData[%d]:%x \n", i, pData[i]);
	}
	if(Bit <= 5)
	{
		for(i=0,k=2; i<120; i+=8,k+=5)
		{
			//LOG_INF("pData[%d]:%x \n", i, pData[i]);
			pPara[i] = pData[k]>>3; // 7~3 (byte0)
			pPara[i+1] = ((pData[k]&0x7)<<2)|(pData[k+1]>>6); // 2~0 (byte0) and 7~6(byte1)
			pPara[i+2] = (pData[k+1]&0x3e)>>1; // 5~1 (byte1)
			pPara[i+3] = ((pData[k+1]&0x1)<<4)|(pData[k+2]>>4); // 0 (byte1) and 7~4(byte2)
			pPara[i+4] = ((pData[k+2]&0xf)<<1)|(pData[k+3]>>7); // 3~0 (byte2) and 7(byte3)
			pPara[i+5] = (pData[k+3]&0x7c)>>2; // 6~2 (byte3)
			pPara[i+6] = ((pData[k+3]&0x3)<<3)|(pData[k+4]>>5); // 1~0 (byte3) and 7~5(byte4)
			pPara[i+7] = pData[k+4]&0x1f; // 4~0 (byte4)

			LOG_INF("bit-pData[%d]:%x \n", k, pData[k]);
			LOG_INF("bit-pData[%d]:%x \n", k+1, pData[k+1]);
			LOG_INF("bit-pData[%d]:%x \n", k+2, pData[k+2]);
			LOG_INF("bit-pData[%d]:%x \n", k+3, pData[k+3]);
			LOG_INF("bit-pData[%d]:%x \n", k+4, pData[k+4]);

			
			LOG_INF("bit-pData[%d]:%x \n", i, pPara[i]);
			LOG_INF("bit-pData[%d]:%x \n", i+1, pPara[i+1]);
			LOG_INF("bit-pData[%d]:%x \n", i+2, pPara[i+2]);
			LOG_INF("bit-pData[%d]:%x \n", i+3, pPara[i+3]);
			LOG_INF("bit-pData[%d]:%x \n", i+4, pPara[i+4]);
			LOG_INF("bit-pData[%d]:%x \n", i+5, pPara[i+5]);
			LOG_INF("bit-pData[%d]:%x \n", i+6, pPara[i+6]);
			LOG_INF("bit-pData[%d]:%x \n", i+7, pPara[i+7]);
		}
	}
	else
	{
		for(i=0,k=2; i<48; i+=8,k+=5)
		{
			//LOG_INF(" apData[%d]:%x \n", i, pData[i]);
			pPara[i] = pData[k]>>3; // 7~3 (byte0)
			pPara[i+1] = ((pData[k]&0x7)<<2)|(pData[k+1]>>6); // 2~0 (byte0) and 7~6(byte1)
			pPara[i+2] = (pData[k+1]&0x3e)>>1; // 5~1 (byte1)
			pPara[i+3] = ((pData[k+1]&0x1)<<4)|(pData[k+2]>>4); // 0 (byte1) and 7~4(byte2)
			pPara[i+4] = ((pData[k+2]&0xf)<<1)|(pData[k+3]>>7); // 3~0 (byte2) and 7(byte3)
			pPara[i+5] = (pData[k+3]&0x7c)>>2; // 6~2 (byte3)
			pPara[i+6] = ((pData[k+3]&0x3)<<3)|(pData[k+4]>>5); // 1~0 (byte3) and 7~5(byte4)
			pPara[i+7] = pData[k+4]&0x1f; // 4~0 (byte4)

			LOG_INF("else -pData[%d]:%x \n", i, pData[i]);
			LOG_INF("else -pData[%d]:%x \n", i+1, pData[i+1]);
			LOG_INF("else -pData[%d]:%x \n", i+2, pData[i+2]);
			LOG_INF("else -pData[%d]:%x \n", i+3, pData[i+3]);
			LOG_INF("else -pData[%d]:%x \n", i+4, pData[i+4]);
			LOG_INF("else -pData[%d]:%x \n", i+5, pData[i+5]);
			LOG_INF("else -pData[%d]:%x \n", i+6, pData[i+6]);
			LOG_INF("else -pData[%d]:%x \n", i+7, pData[i+7]);
		}
		for(i=48,k=32; i<120; i+=4,k+=3)
		{
			//LOG_INF(" 48--pData[%d]:%x \n", i, pData[i]);
			pPara[i] = pData[k]>>2; // 7~2 (byte0)
			pPara[i+1] = ((pData[k]&0x3)<<4)|(pData[k+1]>>4); //1~0 (byte0) and7~4(byte1)
			pPara[i+2] = ((pData[k+1]&0xf)<<2)|(pData[k+2]>>6); //3~0 (byte1) and7~6(byte2)
			pPara[i+3] = pData[k+2]&0x3f; // 5~0 (byte2)

			LOG_INF("48--pData[%d]:%x \n", i, pData[i]);
			LOG_INF("48--pData[%d]:%x \n", i+1, pData[i+1]);
			LOG_INF("48--pData[%d]:%x \n", i+2, pData[i+2]);
			LOG_INF("48--pData[%d]:%x \n", i+3, pData[i+3]);
		}
		memcpy(pCenter, pPara, 16);
		memcpy(pMiddle, pPara+16, 32);
		memcpy(pCorner, pPara+48, 72);
		for(i=0; i<32; i++)
		{
			pMiddle[i] <<= (Bit-6);
		}
		for(i=0; i<72; i++)
		{
			pCorner[i] <<= (Bit-6);
		}
	if(Option == 0)
	{ // 10x12
		memcpy(pPara, pCorner, 26);
		memcpy(pPara+26, pMiddle, 8);
		memcpy(pPara+34, pCorner+26, 4);
		memcpy(pPara+38, pMiddle+8, 2);
		memcpy(pPara+40, pCenter, 4);
		memcpy(pPara+44, pMiddle+10, 2);
		memcpy(pPara+46, pCorner+30, 4);
		memcpy(pPara+50, pMiddle+12, 2);
		memcpy(pPara+52, pCenter+4, 4);
		memcpy(pPara+56, pMiddle+14, 2);
		memcpy(pPara+58, pCorner+34, 4);
		memcpy(pPara+62, pMiddle+16, 2);
		memcpy(pPara+64, pCenter+8, 4);
		memcpy(pPara+68, pMiddle+18, 2);
		memcpy(pPara+70, pCorner+38, 4);
		memcpy(pPara+74, pMiddle+20, 2);
		memcpy(pPara+76, pCenter+12, 4);
		memcpy(pPara+80, pMiddle+22, 2);
		memcpy(pPara+82, pCorner+42, 4);
		memcpy(pPara+86, pMiddle+24, 8);
		memcpy(pPara+94, pCorner+46, 26);
	}
	else
	{ // 12x10
		memcpy(pPara, pCorner, 22);
		memcpy(pPara+22, pMiddle, 6);
		memcpy(pPara+28, pCorner+22, 4);
		memcpy(pPara+32, pMiddle+6, 6);
		memcpy(pPara+38, pCorner+26, 4);
		memcpy(pPara+42, pMiddle+12, 1);
		memcpy(pPara+43, pCenter, 4);
		memcpy(pPara+47, pMiddle+13, 1);
		memcpy(pPara+48, pCorner+30, 4);
		memcpy(pPara+52, pMiddle+14, 1);
		memcpy(pPara+53, pCenter+4, 4);
		memcpy(pPara+57, pMiddle+15, 1);
		memcpy(pPara+58, pCorner+34, 4);
		memcpy(pPara+62, pMiddle+16, 1);
		memcpy(pPara+63, pCenter+8, 4);
		memcpy(pPara+67, pMiddle+17, 1);
		memcpy(pPara+68, pCorner+38, 4);
		memcpy(pPara+72, pMiddle+18, 1);
		memcpy(pPara+73, pCenter+12, 4);
		memcpy(pPara+77, pMiddle+19, 1);
		memcpy(pPara+78, pCorner+42, 4);
		memcpy(pPara+82, pMiddle+20, 6);
		memcpy(pPara+88, pCorner+46, 4);
		memcpy(pPara+92, pMiddle+26, 6);
		memcpy(pPara+98, pCorner+50, 22);
	}
 }
	for(i=0; i<120; i++)
	{
		LOG_INF(" pPara[%d]:%x \n", i, pPara[i]);
		pPara[i] += Offset;
	}

}
//
void ColorDecoder(uint8_t *pData, uint8_t *pPara)
{
	
	uint32_t Offset, Bit, Option;
	uint32_t i, k;
	uint8_t pBase[30];
	Offset = pData[0];
	Bit = pData[1]>>7;
	Option = (pData[1]&0x40)>>6;
	pPara[0] = (pData[1]&0x3e)>>1; // 5~1 (byte1)
	pPara[1] = ((pData[1]&0x1)<<4)|(pData[2]>>4); // 0 (byte1) and 7~4 (byte2)
	pPara[2] = ((pData[2]&0xf)<<1)|(pData[3]>>7); // 3~0 (byte2) and 7 (byte3)
	pPara[3] = (pData[3]&0x7c)>>2; // 6~2 (byte3)
	pPara[4] = ((pData[3]&0x3)<<3)|(pData[4]>>5); // 1~0 (byte3) and 7~5 (byte4)
	pPara[5] = pData[4]&0x1f; // 4~0 (byte4)
	for(i=6,k=5; i<30; i+=8,k+=5)
	{
		pPara[i] = pData[k]>>3; // 7~3 (byte0)
		pPara[i+1] = ((pData[k]&0x7)<<2)|(pData[k+1]>>6); // 2~0 (byte0) and 7~6 (byte1)
		pPara[i+2] = (pData[k+1]&0x3e)>>1; // 5~1 (byte1)
		pPara[i+3] = ((pData[k+1]&0x1)<<4)|(pData[k+2]>>4); // 0 (byte1) and 7~4 (byte2)
		pPara[i+4] = ((pData[k+2]&0xf)<<1)|(pData[k+3]>>7); // 3~0 (byte2) and 7 (byte3)
		pPara[i+5] = (pData[k+3]&0x7c)>>2; // 6~2 (byte3)
		pPara[i+6] = ((pData[k+3]&0x3)<<3)|(pData[k+4]>>5); // 1~0 (byte3) and 7~5 (byte4)
		pPara[i+7] = pData[k+4]&0x1f; // 4~0 (byte4)
	}
	memcpy(pBase, pPara, 30);
	for(i=0,k=20; i<120; i+=4,k++)
	{
		pPara[i] = pData[k]>>6;
		pPara[i+1] = (pData[k]&0x30)>>4;
		pPara[i+2] = (pData[k]&0xc)>>2;
		pPara[i+3] = pData[k]&0x3;
	}
	if(Option == 0)
	{ // 10x12
		for(i=0; i<5; i++)
		{
			for(k=0; k<6; k++)
			{
				pPara[i*24+k*2] += pBase[i*6+k];
				pPara[i*24+k*2+1] += pBase[i*6+k];
				pPara[i*24+k*2+12] += pBase[i*6+k];
				pPara[i*24+k*2+13] += pBase[i*6+k];
			}
		}
	}else
	{ // 12x10
		for(i=0; i<6; i++)
		{
			for(k=0; k<5; k++)
			{
				pPara[i*20+k*2] += pBase[i*5+k];
				pPara[i*20+k*2+1] += pBase[i*5+k];
				pPara[i*20+k*2+10] += pBase[i*5+k];
				pPara[i*20+k*2+11] += pBase[i*5+k];
			}
		}
	}
	for(i=0; i<120; i++)
	{
		pPara[i] = (pPara[i]<<Bit) + Offset;
	}
	
}
#else


// call this function before access OTP
int OV4688_OTP_access_start()
{
	int temp;
	temp = OV4688_read_i2c(0x5000);
	temp = temp & 0xdf;                                    // set bit 5 to '0'
	OV4688_write_i2c(0x5000, temp);
	return 0;
}
// call this function after access OTP
int OV4688_OTP_access_end()
{
	int temp;
	temp = OV4688_read_i2c(0x5000);
	temp = temp | 0x20;                                   // set bit 5 to '1'
	OV4688_write_i2c(0x5000, temp);
	return 0;
}
// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
int check_otp_wb(int index)
{
	int flag, i;
	int address_start, address_end;
	if(index==1) 
	{
		address_start = 0x7110;
		address_end = 0x7110;
	}
	else if(index==2) 
	{
		address_start = 0x7120;
		address_end = 0x7120;
	}
	else 
	{
		address_start = 0x7130;
		address_end = 0x7130;
	}
	// read otp into buffer
	OV4688_OTP_access_start();
	OV4688_write_i2c(0x3d84, 0xc0); // program disable, manual mode
	
	//partial mode OTP write start address
	OV4688_write_i2c(0x3d88, (address_start>>8));
	OV4688_write_i2c(0x3d89, (address_start & 0xff));
	
	// partial mode OTP write end address
	OV4688_write_i2c(0x3d8A, (address_end>>8));
	OV4688_write_i2c(0x3d8B, (address_end & 0xff));
	OV4688_write_i2c(0x3d81, 0x01); // read OTP
	Delay(5);
	//select group

	flag = OV4688_read_i2c(address_start);
	
	// clear otp buffer
	for (i=address_start;i<=address_end;i++)
	{
		OV4688_write_i2c(i, 0x00);
	}
	OV4688_OTP_access_end();
	
	if (flag == 0x00) 
	{
		return 0;
	}
	else if (flag & 0x02)
	{
		return 1;
	}
	else 
	{
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// code: 0 for start, 1 for end
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
int check_otp_VCM(int index, int code)
{
	int flag, i;
	int address_start, address_end;
	if(index==1) 
	{
		address_start = 0x7140 + code*2;
		address_end = 0x7140 + code*2;
	}
	else if(index==2) 
	{
		address_start = 0x7144 + code*2;
		address_end = 0x7144 + code*2;
	}
	else
	{
		address_start = 0x7148 + code*2;
		address_end = 0x7148 + code*2;
	}
	
	// read otp into buffer
	OV4688_OTP_access_start();
	OV4688_write_i2c(0x3d84, 0xc0); // program disable, manual mode
	
	//partial mode OTP write start address
	OV4688_write_i2c(0x3d88, (address_start>>8));
	OV4688_write_i2c(0x3d89, (address_start & 0xff));
	
	// partial mode OTP write end address
	OV4688_write_i2c(0x3d8A, (address_end>>8));
	OV4688_write_i2c(0x3d8B, (address_end & 0xff));
	OV4688_write_i2c(0x3d81, 0x01);
	Delay(5);
	
	//select group
	flag = OV4688_read_i2c(address_start);
	flag = flag & 0xc0;
	
	// clear otp buffer
	for (i=address_start;i<=address_end;i++)
	{
		OV4688_write_i2c(i, 0x00);
	}

	OV4688_OTP_access_end();
	if (flag == 0x00)
	{
		return 0;
	}
	else if (flag & 0x80) 
	{
		return 1;
	}
	else
	{
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
int read_otp_wb(int index, struct otp_struct *otp_ptr)
{
	int i, temp;
	int address_start, address_end;
	
	// read otp into buffer
	OV4688_write_i2c(0x3d84, 0xc0); // program disable, manual mode
	
	//select group
	if(index==1)
	{
		address_start = 0x7111;
		address_end = 0x711f;
	}
	else if(index==2)
	{
		address_start = 0x7121;
		address_end = 0x712f;
	}
	else
	{
		address_start = 0x7131;
		address_end = 0x713f;
	}
	
	//partial mode OTP write start address
	OV4688_OTP_access_start();
	OV4688_write_i2c(0x3d88, (address_start>>8));
	OV4688_write_i2c(0x3d89, (address_start & 0xff));
	
	// partial mode OTP write end address
	OV4688_write_i2c(0x3d8A, (address_end>>8));
	OV4688_write_i2c(0x3d8B, (address_end & 0xff));
	OV4688_write_i2c(0x3d81, 0x01); // load otp into buffer
	Delay(5);
	(*otp_ptr).module_integrator_id = OV4688_read_i2c(address_start);
	(*otp_ptr).lens_id = OV4688_read_i2c(address_start + 1);
	(*otp_ptr).production_year = OV4688_read_i2c(address_start + 2);
	(*otp_ptr).production_month = OV4688_read_i2c( address_start + 3);
	(*otp_ptr).production_day = OV4688_read_i2c(address_start + 4);
	temp = OV4688_read_i2c(address_start + 9);
	(*otp_ptr).rg_ratio = (OV4688_read_i2c(address_start + 5)<<2) + ((temp>>6) & 0x03);
	(*otp_ptr).bg_ratio = (OV4688_read_i2c(address_start + 6)<<2) + ((temp>>4) & 0x03);
	(*otp_ptr).light_rg = (OV4688_read_i2c(address_start + 7) <<2) + ((temp>>2) & 0x03);
	(*otp_ptr).light_bg = (OV4688_read_i2c(address_start + 8)<<2) + (temp & 0x03);
	(*otp_ptr).user_data[0] = OV4688_read_i2c(address_start + 10);

	(*otp_ptr).user_data[1] = OV4688_read_i2c(address_start + 11);
	(*otp_ptr).user_data[2] = OV4688_read_i2c(address_start + 12);
	(*otp_ptr).user_data[3] = OV4688_read_i2c(address_start + 13);
	(*otp_ptr).user_data[4] = OV4688_read_i2c(address_start + 14);
	// clear otp buffer
	for (i=address_start;i<=address_end;i++) 
	{
		OV4688_write_i2c(i, 0x00);
	}
	OV4688_OTP_access_end();
	return 0;
}
// index: index of otp group. (1, 2, 3)
// code: 0 start code, 1 stop code
// return: 0
int read_otp_VCM(int index, int code, struct otp_struct * otp_ptr)
{
	int i, temp, vcm;
	int address;
	int address_start, address_end;
	//check group
	if(index==1)
	{
		address_start = 0x7140;
		address_end = 0x7143;
	}
	else if(index==2)
	{
		address_start = 0x7144;
		address_end = 0x7147;
	}
	else
	{
		address_start = 0x7148;
		address_end = 0x714b;
	}
	
	// read otp into buffer
	OV4688_OTP_access_start();
	OV4688_write_i2c(0x3d84, 0xc0); // program disable, manual mode
	
	//partial mode OTP write start address
	OV4688_write_i2c(0x3d88, (address_start>>8));
	OV4688_write_i2c(0x3d89, (address_start & 0xff));
	
	// partial mode OTP write end address
	OV4688_write_i2c(0x3d8A, (address_end>>8));
	OV4688_write_i2c(0x3d8B, (address_end & 0xff));
	OV4688_write_i2c(0x3d81, 0x01); // load otp into buffer
	Delay(5);
	
	//flag and lsb of VCM start code
	address = address_start + code*2;
	temp = OV4688_read_i2c(address);
	vcm = (OV4688_read_i2c(address_start + 1) << 2) | (temp & 0x03);
	if(code==1)
	{
		(* otp_ptr).VCM_end = vcm;
	}
	else 
	{
		(* otp_ptr).VCM_start = vcm;
	}
	// clear otp buffer
	for (i=address_start;i<=address_end;i++) 
	{
		OV4688_write_i2c(i, 0x00);
	}
	OV4688_OTP_access_end();
	return 0;
}
// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
int update_awb_gain(int R_gain, int G_gain, int B_gain)
{
	if (R_gain>0x400) 
	{
		//long WB R gain
		OV4688_write_i2c(0x500c, R_gain>>8);
		OV4688_write_i2c(0x500d, R_gain & 0x00ff);
		//middle WB R gain
		OV4688_write_i2c(0x5012, R_gain>>8);
		OV4688_write_i2c(0x5013, R_gain & 0x00ff);
		//short WB R gain
		OV4688_write_i2c(0x5018, R_gain>>8);
		OV4688_write_i2c(0x5019, R_gain & 0x00ff);
	}
	if (G_gain>0x400) 
	{
		//long WB G gain
		OV4688_write_i2c(0x500e, G_gain>>8);
		OV4688_write_i2c(0x500f, G_gain & 0x00ff);
		//middle WB G gain
		OV4688_write_i2c(0x5014, G_gain>>8);
		OV4688_write_i2c(0x5015, G_gain & 0x00ff);
		//short WB G gain
		OV4688_write_i2c(0x501A, G_gain>>8);
		OV4688_write_i2c(0x501B, G_gain & 0x00ff);
	}
	if (B_gain>0x400) 
	{
		//long WB B gain
		OV4688_write_i2c(0x5010, B_gain>>8);
		OV4688_write_i2c(0x5011, B_gain & 0x00ff);
		//middle WB B gain
		OV4688_write_i2c(0x5016, B_gain>>8);
		OV4688_write_i2c(0x5017, B_gain & 0x00ff);
		//short WB B gain
		OV4688_write_i2c(0x501C, B_gain>>8);
		OV4688_write_i2c(0x501D, B_gain & 0x00ff);
	}
	return 0;
}
// call this function after OV4688 initialization
// return value: 0 update success
// 1, no OTP
int update_otp_wb()
{
	struct otp_struct current_otp;
	int i;
	int otp_index;// bank 1,2,3

	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=3;i++)
	{
		temp = check_otp_wb(i);
		if (temp == 2) 
		{
			otp_index = i;
			break;
		}
	}

	
	if (i>3) 
	{
		// no valid wb OTP data
		return 1;
	}
	// set right bank
	read_otp_wb(otp_index, &current_otp);
	if(current_otp.light_rg==0) 
	{
		// no light source information in OTP, light factor = 1
		rg = current_otp.rg_ratio;
	}
	else 
	{
		rg = current_otp.rg_ratio * (current_otp.light_rg + 512) / 1024;
	}
	
	if(current_otp.light_bg==0)
	{
		// not light source information in OTP, light factor = 1
		bg = current_otp.bg_ratio;
	}
	else
	{
		bg = current_otp.bg_ratio * (current_otp.light_bg + 512) / 1024;
	}
	//calculate G gain
	//0x400 = 1x gain
	if(bg < BG_Ratio_Typical)
	{
		if (rg< RG_Ratio_Typical) 
		{
			// current_otp.bg_ratio < BG_Ratio_typical &&
			// current_otp.rg_ratio < RG_Ratio_typical
			G_gain = 0x400;
			B_gain = 0x400 * BG_Ratio_Typical / bg;
			R_gain = 0x400 * RG_Ratio_Typical / rg;
		}
		else 
		{
			// current_otp.bg_ratio < BG_Ratio_typical &&
			// current_otp.rg_ratio >= RG_Ratio_typical
			R_gain = 0x400;
			G_gain = 0x400 * rg / RG_Ratio_Typical;
			B_gain = G_gain * BG_Ratio_Typical /bg;
		}
	}
	else 
	{
		if (rg < RG_Ratio_Typical) 
		{
			// current_otp.bg_ratio >= BG_Ratio_typical &&
			// current_otp.rg_ratio < RG_Ratio_typical

			B_gain = 0x400;
			G_gain = 0x400 * bg / BG_Ratio_Typical;
			R_gain = G_gain * RG_Ratio_Typical / rg;
		}
		else 
		{
			// current_otp.bg_ratio >= BG_Ratio_typical &&
			// current_otp.rg_ratio >= RG_Ratio_typical
			G_gain_B = 0x400 * bg / BG_Ratio_Typical;
			G_gain_R = 0x400 * rg / RG_Ratio_Typical;
			
			if(G_gain_B > G_gain_R ) 
			{
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * RG_Ratio_Typical /rg;
			}
			else 
			{
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * BG_Ratio_Typical / bg;
			}
		}
	}
	update_awb_gain(R_gain, G_gain, B_gain);
	return 0;
}

void ov4688_otp_cali(unsigned char writeid)
{
	ov4688MIPI_WRITE_ID = writeid;	
	update_otp_wb();

}
#endif
//
