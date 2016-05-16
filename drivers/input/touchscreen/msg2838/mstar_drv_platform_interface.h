/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_platform_interface.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_PLATFORM_INTERFACE_H__
#define __MSTAR_DRV_PLATFORM_INTERFACE_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE															 */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"

/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION											  */
/*--------------------------------------------------------------------------*/

extern s32 /*__devinit*/ MsDrvInterfaceTouchDeviceProbe(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId);
extern s32 /*__devexit*/ MsDrvInterfaceTouchDeviceRemove(struct i2c_client *pClient);
#ifdef CONFIG_ENABLE_NOTIFIER_FB
extern int MsDrvInterfaceTouchDeviceFbNotifierCallback(struct notifier_block *pSelf, unsigned long nEvent, void *pData);
#else
extern void MsDrvInterfaceTouchDeviceResume(struct early_suspend *pSuspend);
extern void MsDrvInterfaceTouchDeviceSuspend(struct early_suspend *pSuspend);
#endif
extern void MsDrvInterfaceTouchDeviceSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate);

#endif  /* __MSTAR_DRV_PLATFORM_INTERFACE_H__ */
