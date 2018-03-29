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

#ifndef __MJC_KERNEL_DRIVER_H__
#define __MJC_KERNEL_DRIVER_H__

extern u32 get_devinfo_with_index(u32 index);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);


typedef struct _MJC_EVENT_T {
	void *pvWaitQueue;	/* /< [IN]     The waitqueue discription */
	unsigned int u4TimeoutMs;	/* /< [IN]     The timeout ms */
	void *pvFlag;		/* /< [IN/OUT] flag */
} MJC_EVENT_T;

typedef struct {
	MJC_EVENT_T rEvent;
} MJC_CONTEXT_T;

/* ************************************ */
/* IO control structure */
/* ************************************ */
typedef struct {
	unsigned int u4StructSize;
} MJC_IOCTL_LOCK_HW_T;

typedef struct {
	unsigned int u4StructSize;
	unsigned int u4TimeoutMs;
} MJC_IOCTL_ISR_T;

typedef struct {
	unsigned long reg;
	unsigned int val;
	unsigned int mask;
} MJC_WRITE_REG_T;

typedef struct {
	unsigned long reg;
	unsigned int val;
	unsigned int mask;
} MJC_READ_REG_T;

typedef struct {
	unsigned int u4StructSize;
	unsigned short u2OutputFramerate;
} MJC_IOCTL_SRC_CLK_T;

typedef struct {
	unsigned int u4StructSize;
	unsigned long ulRegPAddress;
	unsigned long ulRegPSize;
} MJC_IOCTL_REG_INFO_T;

#define MJC_IOC_MAGIC    'N'

#define MJC_LOCKHW                           _IOW(MJC_IOC_MAGIC, 0x00, MJC_IOCTL_LOCK_HW_T)
#define MJC_WAITISR                          _IOW(MJC_IOC_MAGIC, 0x01, MJC_IOCTL_ISR_T)
#define MJC_READ_REG                         _IOW(MJC_IOC_MAGIC, 0x02, MJC_READ_REG_T)
#define MJC_WRITE_REG                        _IOW(MJC_IOC_MAGIC, 0x03, MJC_WRITE_REG_T)
#define MJC_WRITE_REG_TBL                   _IOW(MJC_IOC_MAGIC, 0x04, int)
#define MJC_CLEAR_REG_TBL                   _IOW(MJC_IOC_MAGIC, 0x05, int)
#define MJC_SOURCE_CLK                      _IOW(MJC_IOC_MAGIC, 0x06, MJC_IOCTL_SRC_CLK_T)
#define MJC_REG_INFO                      _IOW(MJC_IOC_MAGIC, 0x07, MJC_IOCTL_REG_INFO_T)

#endif				/* __MJC_KERNEL_DRIVER_H__ */
