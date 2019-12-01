/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name		: user_test_type_def.h
* Author		: Bob Huang
* Version		: V1.0.0
* Date			: 07/26/2017
* Description	: The basic type in user test module
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef USER_TEST_TYPE_DEF_H
#define USER_TEST_TYPE_DEF_H

#include <asm-generic/int-ll64.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KERNEL_CODE_OPEN
/*//--------------------Platform Micro--------------------//*/
#if defined(PC_CODE_OPEN)
#define PC_CODE			1
#else
#define PC_CODE			0
#endif

#if defined(ARM_CODE_OPEN)
#define ARM_CODE		1
#else
#define ARM_CODE		0
#endif

#if defined(ANDROID_CODE_OPEN)
#define ANDROID_CODE		1
#else
#define ANDROID_CODE		0
#endif

#if defined(QNX_CODE_OPEN)
#define QNX_CODE			1
#else
#define QNX_CODE			0
#endif

#if defined(WCE_CODE_OPEN)
#define WCE_CODE		1
#else
#define WCE_CODE		0
#endif

#if defined(KERNEL_CODE_OPEN)
#define LINUX_KERNEL	1
#else
#define LINUX_KERNEL	0
#endif

/*//----------------------------------------//*/
#define IN
#define OUT
#define IN_OUT

#if WCE_CODE == 1
	typedef unsigned char u8;
	typedef const unsigned char cu8;
	typedef unsigned short u16;
	typedef const unsigned short cu16;
	typedef unsigned int u32;
	typedef const unsigned int cu32;
	typedef int s32;
	typedef const int cs32;
	typedef signed short s16;
	typedef const signed short cs16;
	typedef void *ptr32;
	typedef void const *cptr32;
	typedef unsigned long vu32;
	typedef const unsigned long cvu32;
#define NULL 0
	typedef const char *cstr;

#elif PC_CODE == 1
	typedef char s8;
	typedef unsigned char u8;
	typedef const unsigned char cu8;
	typedef unsigned short u16;
	typedef const unsigned short cu16;
	typedef unsigned int u32;
	typedef const unsigned int cu32;
	typedef int s32;
	typedef const int cs32;
	typedef signed short s16;
	typedef const signed short cs16;
	typedef void *ptr32;
	typedef void const *cptr32;
	typedef unsigned long vu32;
	typedef const unsigned long cvu32;

#define NULL 0

	typedef const char *cstr;
#elif ARM_CODE == 1
#include "type.h"
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef int s32;

	typedef const unsigned short cu16;
	typedef const signed short cs16;
	typedef signed short s16;
	typedef void *ptr32;
	typedef void const *cptr32;

#ifdef STM32F1XX
	typedef unsigned int uint32_t;
	typedef int int32_t;
	typedef const char *cstr;
#endif

#elif ANDROID_CODE == 1 || QNX_CODE || LINUX_KERNEL == 1
	//typedef unsigned char u8;
	//typedef signed char s8;
	typedef const unsigned char cu8;
	//typedef unsigned short u16;
	typedef const unsigned short cu16;
	//typedef unsigned int u32;
	typedef const unsigned int cu32;
	//typedef int s32;
	typedef const int cs32;
	//typedef signed short s16;
	typedef const signed short cs16;
	typedef void *ptr32;
	typedef void const *cptr32;
	typedef unsigned long vu32;
	typedef const unsigned long cvu32;
	typedef const char *cstr;

#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
