/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>

/* file system */
//#include <stdio.h>
//#include <sys/stat.h>

#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include "kd_camera_typedef.h"
#include <linux/dma-mapping.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

#include "gc08a3_sub_otp.h"



#define Sleep(ms) mdelay(ms)

static int debug_log;

#define PFX "gc08a3_main_otp"
#define LOG_INF(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF_IF(...)      do { if ((debug_log)) LOG_INF(__VA_ARGS__); } while (0)

//static DEFINE_SPINLOCK(gc08a3_otp_lock);

static struct i2c_client *g_pstI2CclientG;

struct gc08a3_otp {
	kal_uint8 module[GC08A3_DATA_LEN_MODULE];
	kal_uint8 lsc[GC08A3_DATA_LEN_LSC];
	kal_uint8 awb[GC08A3_DATA_LEN_AWB];
	kal_uint8 af[GC08A3_DATA_LEN_AF];
};

static struct gc08a3_otp gc08a3_otp_data = {0};

static int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData)
{
	int  i4RetValue = 0;

	i4RetValue = i2c_master_send(g_pstI2CclientG,
		a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
		pr_debug("I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
		return -1;
	}
	i4RetValue = i2c_master_recv(g_pstI2CclientG,
		(char *)a_pRecvData, a_sizeRecvData);
	if (i4RetValue != a_sizeRecvData) {
		pr_debug("I2C read failed!!\n");
		return -1;
	}
	return 0;
}
static int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData)
{
	int  i4RetValue = 0;

	i4RetValue = i2c_master_send(g_pstI2CclientG,
		a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
		pr_debug("I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
		return -1;
	}
	return 0;

}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1); //liudongkai

	return get_byte;
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3);
}

static void  gc08a3_otp_enable(void)
{
	LOG_INF("start");
	write_cmos_sensor_8(0x0324, 0x44);
	write_cmos_sensor_8(0x0316, 0x09); // [3] otpclk_en
	write_cmos_sensor_8(0x0A67, 0x80); // [7] otp_en
	write_cmos_sensor_8(0x0313, 0x00);
	write_cmos_sensor_8(0x0A53, 0x0E);
	write_cmos_sensor_8(0x0A65, 0X17);
	write_cmos_sensor_8(0x0A68, 0xA1);
	write_cmos_sensor_8(0x0A47, 0x00);
	write_cmos_sensor_8(0x0A58, 0x00);
	write_cmos_sensor_8(0x0ACE, 0x0C);

	Sleep(10);
}

static void  gc08a3_otp_disable(void)
{
	LOG_INF("start");
	write_cmos_sensor_8(0x0316, 0x01);
	write_cmos_sensor_8(0x0A67, 0x00);
	Sleep(1);
}
static kal_uint16 gc08a3_otp_read_flag(kal_uint32 addr)
{
	kal_uint16 flag = 0x100;

	write_cmos_sensor_8(0x0313, 0x00);
	write_cmos_sensor_8(0x0a69, (addr>>8) & 0xff);// start address H
	write_cmos_sensor_8(0x0a6a, addr & 0xff);// start address L
	write_cmos_sensor_8(0x0313, 0x20);
	write_cmos_sensor_8(0x0313, 0x12);

	flag = read_cmos_sensor(GC08A3_DATA_READ_REG);
	LOG_INF("addr = 0x%x, flag = 0x%x", addr, flag);
	return flag;
}

static kal_uint16 gc08a3_otp_read_data(kal_uint32 addr, kal_uint8 *buf, kal_uint16 len)
{
	kal_uint16 i = 0;
	kal_uint32 checksum = 0;

	LOG_INF("addr=0x%x, size=0x%x", addr, len);
	for (i = 0; i < len; i++) {
		//write_cmos_sensor_8 (0x0313, 0x00);
		write_cmos_sensor_8(0x0a69, (addr>>8) & 0xff);  // start address H
		write_cmos_sensor_8(0x0a6a, addr & 0xff);       // start address L
		write_cmos_sensor_8(0x0313, 0x20);
		//write_cmos_sensor_8 (0x0313, 0x12);
		buf[i] = (kal_uint8)read_cmos_sensor(GC08A3_DATA_READ_REG);
		checksum += buf[i];
		LOG_INF(" 0x%04x, 0x%02x, checksum[0x%04x]=0x%04x",
			addr, buf[i], addr, checksum);
		addr = addr + 8;
}

	LOG_INF("checksum=0x%x, checkvalue=0x%x, i=0x%x(%d)", checksum, buf[len-1], i, i);
	if (addr == GC08A3_DATA_ADDR_MODULE1)
		checksum += GC08A3_FLAG_GROUP1;

	if ((((checksum-buf[len-1])%255)+1) == buf[len-1]) {
		LOG_INF("checksum check pass");
	} else {
		LOG_INF("checksum check fail!");
		i = 0;
	}

	return i;
}

static kal_uint16 gc08a3_otp_read_data_retry(kal_uint32 addr, kal_uint8 *buf, kal_uint16 len)
{
	kal_uint16 retry_times = 0;

	for (retry_times = 0; retry_times < 2; retry_times++) {
		LOG_INF("len == %d", len);
		if (len == gc08a3_otp_read_data(addr, buf, len))
			break;

		LOG_INF("retry_times=%d", retry_times);
		gc08a3_otp_disable();
		gc08a3_otp_enable();

	}

	if (retry_times >= 2) {
		LOG_INF("read af error! retry_times=%d", retry_times);
		return 0;
	}
	return len;
}

static kal_bool gc08a3_otp_read_module_info(void)
{
	kal_uint32 addr = 0;
	kal_uint16 flag1 = 0x100;
	kal_uint16 flag2 = 0x100;

	LOG_INF("start");
	flag1 = gc08a3_otp_read_flag(GC08A3_FLAG_MODULE1_ADDR);
	flag2 = gc08a3_otp_read_flag(GC08A3_FLAG_MODULE2_ADDR);
	if (GC08A3_FLAG_GROUP1 == 0x100) {
		addr = GC08A3_DATA_ADDR_MODULE1;
	} else if (GC08A3_FLAG_GROUP2 == 0x100) {
		addr = GC08A3_DATA_ADDR_MODULE2;
	} else {
		LOG_INF("read flag1(0x%x) error!", flag1);
		return KAL_FALSE;
	}

	gc08a3_otp_read_data_retry(addr,
		(kal_uint8 *)&(gc08a3_otp_data.module), GC08A3_DATA_LEN_MODULE);
	return KAL_TRUE;
}

static kal_bool gc08a3_otp_read_lsc(void)
{
	kal_uint32 addr = 0;
	kal_uint16 flag1 = 0x100;
	kal_uint16 flag2 = 0x100;

	LOG_INF("start");
	flag1 = gc08a3_otp_read_flag(GC08A3_FLAG_LSC1_ADDR);
	flag2 = gc08a3_otp_read_flag(GC08A3_FLAG_LSC2_ADDR);
	LOG_INF("read flag1(0x%x)  flag2(0x%x)error!", flag1, flag2);
	if (GC08A3_FLAG_GROUP1 == 0x100) {
		addr = GC08A3_DATA_ADDR_LSC1;
	} else if (GC08A3_FLAG_GROUP2 == 0x100) {
		addr = GC08A3_DATA_ADDR_LSC2;
	} else {
		LOG_INF("read flag1(0x%x)  flag2(0x%x)error!", flag1, flag2);
		return KAL_FALSE;
	}

	gc08a3_otp_read_data_retry(addr,
		(kal_uint8 *)&(gc08a3_otp_data.lsc), GC08A3_DATA_LEN_LSC);
	return KAL_TRUE;
}

kal_bool gc08a3sub_otp_read_awb(void)
{
	kal_uint32 addr = 0;
	kal_uint16 flag = 0x100;

	LOG_INF("start");
	flag = gc08a3_otp_read_flag(GC08A3_FLAG_MODULE1_ADDR);

	if (GC08A3_FLAG_GROUP1 == 0x100) {
		addr = GC08A3_DATA_ADDR_AWB1;
	} else if (GC08A3_FLAG_GROUP2 == 0x100) {
		addr = GC08A3_DATA_ADDR_AWB2;
	} else {
		LOG_INF("read flag(0x%x) error!", flag);
		return KAL_FALSE;
	}

	gc08a3_otp_read_data_retry(addr,
			(kal_uint8 *)&(gc08a3_otp_data.awb), GC08A3_DATA_LEN_AWB);
	return KAL_TRUE;
}

kal_bool gc08a3_otp_read_af(void)
{
	kal_uint32 addr = 0;
	kal_uint16 flag1 = 0x100;
	kal_uint16 flag2 = 0x100;

	LOG_INF("start");
	flag1 = gc08a3_otp_read_flag(GC08A3_FLAG_AF1_ADDR);
	flag2 = gc08a3_otp_read_flag(GC08A3_FLAG_AF2_ADDR);

	if (GC08A3_FLAG_GROUP1 == 0x100) {
		addr = GC08A3_DATA_ADDR_AF1;
	} else if (GC08A3_FLAG_GROUP2 == 0x100) {
		addr = GC08A3_DATA_ADDR_AF2;
	} else {
		LOG_INF("read flag1(0x%x) error!", flag1);
		return KAL_FALSE;
	}
	gc08a3_otp_read_data_retry(addr,
		(kal_uint8 *)&(gc08a3_otp_data.af), GC08A3_DATA_LEN_AF);
	return KAL_TRUE;
}

void gc08a3sub_dump_otp(kal_uint8 *data, kal_uint32 OtpSize, unsigned int sensor_id)
{
	UINT32 idx = 0;

#if 1
	if (debug_log == 0) {
		LOG_INF("dump otp, dump enable=%d, return", debug_log);
		return;
	}
	LOG_INF("dump otp, sensor_id=0x%x, OtpSize=0x%x", sensor_id, OtpSize);
	for (idx = 0; idx < OtpSize; idx++)
		LOG_INF(" 0x%04x, 0x%02x", idx, data[idx]);

#else
	INT32 ioctlerr, ret;
	char info[OtpSize];

	// open file
	char targetFile[50];

	ioctlerr = snprintf(targetFile, sizeof(targetFile),
		"/data/vendor/camera_dump/eeprom_sensor_%d",
		sensor_id);
	if (ioctlerr < 0 || ioctlerr > sizeof(targetFile)) {
		LOG_INF("generate path fail!");
		return;
	}

	FILE * fp = fopen(targetFile, "w");

	if (fp == NULL) {
		LOG_INF("open file fail!");
		return;
	}

	// get data
	info = new char[OtpSize];
	ret = fprintf(fp, "SensorID=0x%x\n", sensor_id);
	if (ret < 0) {
		LOG_INF("fprintf sensorID err=%d", ret);
		ret = fclose(fp);
		if (ret != 0)
			CAM_CAL_ERR("fclose err\n");
		return;
	}
	for (idx = 0 ; idx < OtpSize; idx++) {
		ret = fprintf(fp, "0x%04x,0x%02x\n", idx, info[idx]);
		if (ret < 0) {
			LOG_INF("fprintf otp err=%d, idx=%d", ret, idx);
			ret = fclose(fp);
				if (ret != 0)
					LOG_INF("fclose err\n");

			return;
		}
	}
	LOG_INF("dump otp success! SensorID=0x%x, OTP Size=0x%x,\n",
			OtpSize, sensor_id, idx);

	ret = fclose(fp);
	if (ret != 0)
		LOG_INF("fclose err\n");

#endif

}

static void gc08a3_read_otp(void)
{
	LOG_INF_IF("start");
	gc08a3_otp_enable();

	//spin_lock(&gc08a3_otp_lock);
	gc08a3_otp_read_module_info();
	gc08a3_otp_read_lsc();
	//gc08a3_otp_read_awb();
	//gc08a3_otp_read_af();
	//spin_unlock(&gc08a3_otp_lock);

	gc08a3_otp_disable();
}

unsigned int gc08a3_sub_read_region(struct i2c_client *client,
			unsigned int addr,
			unsigned char *data, unsigned int size)
{
	unsigned char *buffer_temp = (unsigned char *)data;

	g_pstI2CclientG = client;
	if (g_pstI2CclientG == NULL) {
		LOG_INF("g_pstI2CclientG==NULL");
		return 0;
	}

	LOG_INF("addr=0x%x, input size=%d, buffer_temp=0x%p", addr, size, buffer_temp);
	LOG_INF_IF(
		"gc08a3_otp_data size=0x%x, GC08A3_OTP_SIZE=0x%x, GC08A3_OTP_SIZE_VALID=0x%x",
		sizeof(gc08a3_otp_data), GC08A3_OTP_SIZE, GC08A3_OTP_SIZE_VALID);

	memset((void *)&gc08a3_otp_data, 0, sizeof(gc08a3_otp_data));
	gc08a3_read_otp();

	memcpy(buffer_temp, gc08a3_otp_data.module, 41*sizeof(kal_uint8));
	buffer_temp += 41;

	memcpy(buffer_temp, gc08a3_otp_data.af, 5*sizeof(kal_uint8));
	buffer_temp += 5;

	memcpy(buffer_temp, gc08a3_otp_data.lsc, 1868*sizeof(kal_uint8));


	//gc08a3_dump_otp((kal_uint8 *)&gc08a3_otp_data, sizeof(gc08a3_otp_data), 0x08a3);

	return size;
}
