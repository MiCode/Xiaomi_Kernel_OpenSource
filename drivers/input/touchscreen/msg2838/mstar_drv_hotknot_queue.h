/*
 *
 *Copyright (C) 2016 XiaoMi, Inc.
 *
 *@file	mstar_drv_hotknot_queue.h
 *
 *@brief   This file defines the queue structure for hotknot
 *
 *
 */

#ifndef __MSTAR_DRV_HOTKNOT_QUEUE_H__
#define __MSTAR_DRV_HOTKNOT_QUEUE_H__


#include "mstar_drv_common.h"


#ifdef CONFIG_ENABLE_HOTKNOT
#define HOTKNOT_QUEUE_SIZE			   1024

extern void CreateQueue(void);
extern void ClearQueue(void);
extern int PushQueue(u8 *pBuf, u16 nLength);
extern int PopQueue(u8 *pBuf, u16 nLength);
extern int ShowQueue(u8 *pBuf, u16 nLength);
extern void ShowAllQueue(u8 *pBuf, u16 *pFront, u16 *pRear);
extern void DeleteQueue(void);


#endif
#endif
