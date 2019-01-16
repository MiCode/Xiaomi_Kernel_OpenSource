#ifndef __SI_PLATFORM_H__
#define __SI_PLATFORM_H__
#include <linux/types.h>
#include "osal/include/osal.h"
typedef int int_t;
typedef unsigned int uint_t;
typedef unsigned char prefuint_t;
typedef signed char prefint_t;
#define PLACE_IN_DATA_SEG
#define PLACE_IN_CODE_SEG
#if 0
typedef enum {
	FALSE = 0,
	TRUE = !(FALSE)
} bool_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed long int32_t;
#endif
#define ROM
#define XDATA

/* typedef unsigned char BOOL; */
#define LOW                     0
#define HIGH                    1
#ifndef BIT0
#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80
#endif
#ifndef BIT_0
#define BIT_0                    0x01
#define BIT_1                    0x02
#define BIT_2                    0x04
#define BIT_3                    0x08
#define BIT_4                    0x10
#define BIT_5                    0x20
#define BIT_6                    0x40
#define BIT_7                    0x80
#endif
#define MSG_ALWAYS              0x00
#define MSG_STAT                0x01
#define MSG_DBG                 0x02
#define SET_BITS    0xFF
#define CLEAR_BITS  0x00
#define RX_BOARD        (CLEAR_BITS)
#define SB_NONE				(0)
#define SB_EPV5_MARK_II		(1)
#define SB_STARTER_KIT_X01	(2)
#define SYSTEM_BOARD		(SB_STARTER_KIT_X01)

#if (SYSTEM_BOARD == SB_EPV5_MARK_II)
#define SiI_TARGET_STRING       "SiI8334 EPV5 MARK II"
#elif (SYSTEM_BOARD == SB_STARTER_KIT_X01)
#define SiI_TARGET_STRING       "SiI8334 Starter Kit X01"
#else
#error "Unknown SYSTEM_BOARD definition."
#endif
#define pinDbgMsgs_HIGH   BIT0
#define pinOverrideTiming_HIGH BIT1
#define pinDbgSw3_HIGH   BIT2
#define pinDbgSw4_HIGH BIT3
#define pinDbgSw5_HIGH   BIT4
#define pinDbgSw6_HIGH   BIT5
#define pinSw_HIGH   BIT6
#define pinPwSw1aEn_HIGH BIT7
extern unsigned char pinDbgMsgs;
extern unsigned char pinOverrideTiming;
extern unsigned char pinDbgSw3;
extern unsigned char pinDbgSw4;
extern unsigned char pinDbgSw5;
extern unsigned char pinDbgSw6;
extern unsigned char pinSw;
extern unsigned char pinPwSw1aEn;
extern unsigned char pinM2uVbusCtrlM;
extern unsigned char pinMhlUsb;
#define HalI2cReadByte I2C_ReadByte
#define PlatformGPIOGet(a) a
#endif
