#ifndef __REFCODE_H__
#define __REFCODE_H__

//#include "Validation.h"

#define F54_Porting

#ifdef F54_Porting
#include <linux/kernel.h>	//printk
#include <linux/string.h>	//memset
extern void device_I2C_read(unsigned char add, unsigned char *value, unsigned short len);
extern void device_I2C_write(unsigned char add, unsigned char *value, unsigned short len);
extern void InitPage(void);
extern void SetPage(unsigned char page);
extern void readRMI(unsigned short add, unsigned char *value, unsigned short len);
extern void longReadRMI(unsigned short add, unsigned char *value, unsigned short len);
extern void writeRMI(unsigned short add, unsigned char *value, unsigned short len);
extern void delayMS(int val);
extern void cleanExit(int code);
extern int waitATTN(int code, int time);
extern void write_log(char *data);
extern int get_limit( unsigned char Tx, unsigned char Rx);
extern int LimitFile[30][46*2];
#endif

//#define _F34_TEST_
#define _F54_TEST_
#define _FW_TESTING_
//#define _BUTTON_DELTA_IMAGE_TEST_
//#define _DS4_3_0_	// TM2000, TM2145, TM2195
#define _DS4_3_2_	// TM2371, TM2370, PLG137, PLG122

//(important) should be defined the value(=register address) according to register map
//'Multi Metric Noise Mitigation Control'
//#define NoiseMitigation 0x1A1	// TM2000 (~E025), TM2195
//#define NoiseMitigation 0x1B1	// TM2000 (E027~)
//#define NoiseMitigation 0x0196	// TM2145
//#define NoiseMitigation 0x15E	// TM2370, TM2371, PLG137, PLG122
#define NoiseMitigation 0x138	// PLG124 E008

//#define F54_CBCPolarity 0x1B6	// TM2000 (E027~)
//#define F54_CBCPolarity 0x163	// TM2370, TM2371, PLG137, PLG122
#define F54_CBCPolarity 0x16E	// PLG124 E008

#ifdef _DS4_3_2_
#define F55_PhysicalRx_Addr 0x301	// TM2371, TM2370, PLG137, PLG122
#endif

#ifdef _F54_TEST_
unsigned char F54_FullRawCap(int);
unsigned char F54_RxToRxReport(void);
unsigned char F54_TxToGndReport(void);
unsigned char F54_TxToTxReport(void);
unsigned char F54_TxOpenReport(void);
unsigned char F54_RxOpenReport(void);
unsigned char F54_HighResistance(void);

int F54_GetFullRawCap(int, char *);
int F54_GetRxToRxReport(char *);
int F54_GetTxToGndReport(char *);
int F54_GetTxToTxReport(char *);
int F54_GetTxOpenReport(char *);
int F54_GetRxOpenReport(char *);
int F54_GetHighResistance(char *);
#endif

#ifdef _BUTTON_DELTA_IMAGE_TEST_
unsigned char F54_ButtonDeltaImage();
#endif

#ifdef _FW_TESTING_
void HostImplementationTesting( void );
#endif

#ifdef _F34_TEST_
void CompleteReflash_OmitLockdown();
void CompleteReflash();
void CompleteReflash_Lockdown();
void ConfigBlockReflash();
#endif

void FirmwareCheck( void );
void AttentionTest( void );
void FirmwareCheck_temp( void );

#endif

