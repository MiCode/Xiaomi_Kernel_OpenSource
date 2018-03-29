/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MMPROFILE_H__
#define __MMPROFILE_H__
#include "mmprofile_static_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MMProfileEventNameMaxLen 31

typedef unsigned int MMP_Event;

typedef enum {
	MMProfileFlagStart = 1,
	MMProfileFlagEnd = 2,
	MMProfileFlagPulse = 4,
	MMProfileFlagEventSeparator = 8,
	MMProfileFlagSystrace = 0x80000000,
	MMProfileFlagMax = 0xFFFFFFFF
} MMP_LogType;

typedef enum {
	MMProfileMetaStringMBS = 1,
	MMProfileMetaStringWCS,
	MMProfileMetaStructure,
	MMProfileMetaBitmap,
	MMProfileMetaRaw,
	MMProfileMetaUser = 0x10000000,
	MMProfileMetaUserM4UReg,
	MMProfileMetaMax = 0xFFFFFFFF
} MMP_MetaDataType;

typedef enum {
	MMProfileBitmapRGB565 = 1,
	MMProfileBitmapRGB888,
	MMProfileBitmapRGBA8888,
	MMProfileBitmapBGR888,
	MMProfileBitmapBGRA8888,
	MMProfileBitmapMax = 0xFFFFFFFF
} MMP_PixelFormat;

typedef struct {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	MMP_MetaDataType data_type; /* meta data type */
	unsigned int size;          /* meta data size */
	void *pData;                /* meta data pointer */
} MMP_MetaData_t;

#ifdef CONFIG_COMPAT
struct Compat_MMP_MetaData_t {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	MMP_MetaDataType data_type; /* meta data type */
	unsigned int size;          /* meta data size */
	unsigned int pData;        /* meta data pointer */
};
#endif

typedef struct {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	unsigned int struct_size;   /* structure size (bytes) */
	void *pData;                /* structure pointer */
	char struct_name[32];       /* structure name */
} MMP_MetaDataStructure_t;

typedef struct {
	unsigned int data1;         /* data1 (user defined) */
	unsigned int data2;         /* data2 (user defined) */
	unsigned int width;         /* image width */
	unsigned int height;        /* image height */
	MMP_PixelFormat format;     /* image pixel format */
	unsigned int start_pos;     /* start offset of image data (base on pData) */
	unsigned int bpp;           /* bits per pixel */
	int pitch;                  /* image pitch (bytes per line) */
	unsigned int data_size;     /* image data size (bytes) */
	unsigned int down_sample_x; /* horizontal down sample rate (>=1) */
	unsigned int down_sample_y; /* vertical down sample rate (>=1) */
	void *pData;                /* image buffer address */
} MMP_MetaDataBitmap_t;

#ifdef CONFIG_MMPROFILE
MMP_Event MMProfileRegisterEvent(MMP_Event parent, const char *name);
MMP_Event MMProfileFindEvent(MMP_Event parent, const char *name);
void MMProfileEnableEvent(MMP_Event event, long enable);
void MMProfileEnableFTraceEvent(MMP_Event event, long enable, long ftrace);
void MMProfileEnableEventRecursive(MMP_Event event, long enable);
void MMProfileEnableFTraceEventRecursive(MMP_Event event, long enable, long ftrace);
long MMProfileQueryEnable(MMP_Event event);
void MMProfileLog(MMP_Event event, MMP_LogType type);
void MMProfileLogEx(MMP_Event event, MMP_LogType type, unsigned long data1, unsigned long data2);
long MMProfileLogMeta(MMP_Event event, MMP_LogType type, MMP_MetaData_t *pMetaData);
long MMProfileLogMetaString(MMP_Event event, MMP_LogType type, const char *str);
long MMProfileLogMetaStringEx(MMP_Event event, MMP_LogType type,
		unsigned long data1, unsigned long data2, const char *str);
long MMProfileLogMetaStructure(MMP_Event event, MMP_LogType type, MMP_MetaDataStructure_t *pMetaData);
long MMProfileLogMetaBitmap(MMP_Event event, MMP_LogType type, MMP_MetaDataBitmap_t *pMetaData);
#endif

#define MMProfileLogStructure(event, type, pStruct, struct_type) \
{ \
	MMP_MetaDataStructure_t MetaData; \
	MetaData.data1 = 0; \
	MetaData.data2 = 0; \
	strcpy(MetaData.struct_name, #struct_type); \
	MetaData.struct_size = sizeof(struct_type); \
	MetaData.pData = (void *)(pStruct); \
	MMProfileLogMetaStructure(event, type, &MetaData); \
}

/*
 * if in kernel config CONFIG_MMPROFILE is not set, and the kernel makefile had define
 * obj-$(CONFIG_MMPROFILE) += mmp/
 * , the mmp/ driver is compiled but not built-in. Put dummy API implementation here.
 */
#ifndef CONFIG_MMPROFILE
static inline MMP_Event MMProfileRegisterEvent(MMP_Event parent, const char *name)
{
	return 0;
}

static inline MMP_Event MMProfileFindEvent(MMP_Event parent, const char *name)
{
	return 0;
}

static inline void MMProfileEnableEvent(MMP_Event event, long enable)
{
}

static inline void MMProfileEnableEventRecursive(MMP_Event event, long enable)
{
}

static inline void MMProfileEnableFTraceEvent(MMP_Event event, long enable, long ftrace)
{
}

static inline void MMProfileEnableFTraceEventRecursive(MMP_Event event, long enable, long ftrace)
{
}

static inline long MMProfileQueryEnable(MMP_Event event)
{
	return 0;
}

static inline void MMProfileLogEx(MMP_Event event, MMP_LogType type, unsigned long data1, unsigned long data2)
{
}

static inline void MMProfileLog(MMP_Event event, MMP_LogType type)
{
}

static inline long MMProfileLogMeta(MMP_Event event, MMP_LogType type, MMP_MetaData_t *pMetaData)
{
	return 0;
}

static inline long MMProfileLogMetaStructure(MMP_Event event, MMP_LogType type,
				MMP_MetaDataStructure_t *pMetaData)
{
	return 0;
}

static inline long MMProfileLogMetaStringEx(MMP_Event event, MMP_LogType type, unsigned long data1,
				unsigned long data2, const char *str)
{
	return 0;
}

static inline long MMProfileLogMetaString(MMP_Event event, MMP_LogType type, const char *str)
{
	return 0;
}

static inline long MMProfileLogMetaBitmap(MMP_Event event, MMP_LogType type, MMP_MetaDataBitmap_t *pMetaData)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif
#endif
