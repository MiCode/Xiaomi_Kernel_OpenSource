/*******************************************************************

                  Generic Type Definitions

********************************************************************
 FileName:        FSCTypes.h
 Dependencies:    Linux kernel
 Processor:       ARM, ARM64, x86, x64
 Compiler:        GCC 4.8.3 or greater
********************************************************************/

#ifndef _FUSB30X_FSCTYPES_H_
#define _FUSB30X_FSCTYPES_H_

#define FSC_HOSTCOMM_BUFFER_SIZE    64  // Length of the hostcomm buffer, needed in both core and platform

#if defined(FSC_PLATFORM_LINUX)

/* Specify an extension for GCC based compilers */
#if defined(__GNUC__)
#define __EXTENSION __extension__
#else
#define __EXTENSION
#endif

#if !defined(__PACKED)
    #define __PACKED
#endif

/* get linux-specific type definitions (NULL, size_t, etc) */
#include <linux/types.h>
#include <linux/printk.h>

#if !defined(BOOL) && !defined(FALSE) && !defined(TRUE)
typedef enum _BOOL { FALSE = 0, TRUE } FSC_BOOL;    /* Undefined size */
#endif // !BOOL && !FALSE && !TRUE

#ifndef FSC_S8
typedef __s8                FSC_S8;                                            // 8-bit signed
#endif // FSC_S8

#ifndef FSC_S16
typedef __s16               FSC_S16;                                           // 16-bit signed
#endif // FSC_S16

#ifndef FSC_S32
typedef __s32               FSC_S32;                                           // 32-bit signed
#endif // FSC_S32

#ifndef FSC_S64
typedef __s64               FSC_S64;                                           // 64-bit signed
#endif // FSC_S64

#ifndef FSC_U8
typedef __u8                FSC_U8;                                            // 8-bit unsigned
#endif // FSC_U8

#ifndef FSC_U16
typedef __u16               FSC_U16;                                           // 16-bit unsigned
#endif // FSC_U16

#ifndef FSC_U32
typedef __u32               FSC_U32;                                           // 32-bit unsigned
#endif // FSC_U32

#undef __EXTENSION

#endif // FSC_PLATFORM_LINUX

#endif /* _FUSB30X_FSCTYPES_H_ */
