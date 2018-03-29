/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Copyright ?2012 Synaptics Incorporated. All rights reserved. */
/*  */
/* The information in this file is confidential under the terms */
/* of a non-disclosure agreement with Synaptics and is provided */
/* AS IS. */
/*  */
/* The information in this file shall remain the exclusive property */
/* of Synaptics and may be the subject of Synaptics?patents, in */
/* whole or part. Synaptics?intellectual property rights in the */
/* information in this file are not expressly or implicitly licensed */
/* or otherwise transferred to you as a result of such information */
/* being made available to you. */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#include <linux/kernel.h>	/* printk */
#include <linux/delay.h>	/* msleep */
#include <linux/time.h>		/* struct timeval t_interval[TIME_PROFILE_MAX]; */
#include <linux/string.h>	/* memset */
#include <linux/i2c.h>
/* #include <linux/input/lge_touch_core.h> */
#if 0
extern const int DefaultTimeout;

int F54Test(int input);
bool switchPage(int page);
void RunQueries(void);
void DeltaImageReport(void);
void RawImageReport(void);
void SensorSpeed(void);
void ADCRange(void);
void TxTxTest(void);
int RxRxShortTest(void);
void HighResistanceTest(void);
void MaxMinTest(void);
int ImageTest(void);
void DeltaImageReport(void);

void SCAN_PDT(void);
#endif

#define TRX_max 32
#define CAP_FILE_PATH "/sns/touch/cap_diff_test.txt"
#define DS5_BUFFER_SIZE 6000

extern int UpperImage[32][32];
extern int LowerImage[32][32];
extern int SensorSpeedUpperImage[32][32];
extern int SensorSpeedLowerImage[32][32];
extern int ADCUpperImage[32][32];
extern int ADCLowerImage[32][32];
extern unsigned char RxChannelCount;
extern unsigned char TxChannelCount;
extern void SCAN_PDT(void);
/* mode:0 => write_log, mode:1 && buf => cat, mode:2 && buf => delta */
extern int F54Test(int input, int mode, char *buf);
extern int GetImageReport(char *buf);
extern int diffnode(unsigned short *ImagepTest);
extern int write_file(char *filename, char *data);
extern int write_log_DS5(char *filename, char *data);
/* tern void read_log(char *filename, const struct touch_platform_data *pdata); */
extern int Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length);
extern int Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length);
extern int ReadF54BitRegisters(unsigned short regAddr, unsigned char *data, int length);

/* extern int get_limit(unsigned char Tx, unsigned char Rx, struct i2c_client client,
		     const struct touch_platform_data *pdata, char *breakpoint,
		     int limit_data[32][32]);*/

#define TPD_TAG                  "[S3320] "
#define TPD_FUN(f)               pr_err("[S3320] %s\n", __func__)
#define TPD_ERR(fmt, args...)    pr_err("[S3320] %s %d : "fmt, __func__, __LINE__, ##args)
#define TPD_LOG(fmt, args...)    pr_err("[S3320] %s %d : "fmt, __func__, __LINE__, ##args)

#if 0
#define TOUCH_INFO_MSG(fmt, args...) \
	pr_err("[S3320][Touch] %s" fmt, ##args)

#define TOUCH_ERR_MSG(fmt, args...) \
	pr_err("[S3320][Touch E] [%s %d] " fmt,	\
		__func__, __LINE__, ##args)

#define TOUCH_DEBUG_MSG(fmt, args...)			\
	pr_err("[S3320] [Touch D] [%s %d] " fmt,	\
		__func__, __LINE__, ##args)
#else
#define TOUCH_INFO_MSG(fmt, args...)

#define TOUCH_ERR_MSG(fmt, args...)

#define TOUCH_DEBUG_MSG(fmt, args...)

#endif				/* 0 */
