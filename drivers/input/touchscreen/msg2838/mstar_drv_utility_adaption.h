/**
 *
 * @file	mstar_drv_utility_adaption.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_UTILITY_ADAPTION_H__
#define __MSTAR_DRV_UTILITY_ADAPTION_H__ (1)


#include "mstar_drv_common.h"

#define BK_REG8_WL(addr, val)	(RegSetLByteValue(addr, val))
#define BK_REG8_WH(addr, val)	(RegSetHByteValue(addr, val))
#define BK_REG16_W(addr, val)	(RegSet16BitValue(addr, val))
#define BK_REG8_RL(addr)		(RegGetLByteValue(addr))
#define BK_REG8_RH(addr)		(RegGetHByteValue(addr))
#define BK_REG16_R(addr)		(RegGet16BitValue(addr))

#define PRINTF_EMERG(fmt, ...)  printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_ALERT(fmt, ...)  printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_CRIT(fmt, ...)   printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_ERR(fmt, ...)	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_WARN(fmt, ...)   printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_NOTICE(fmt, ...) printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_INFO(fmt, ...)   printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define PRINTF_DEBUG(fmt, ...)  printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)


#ifdef CONFIG_ENABLE_DMA_IIC
extern void DmaAlloc(void);
extern void DmaReset(void);
extern void DmaFree(void);
#endif
extern u16  RegGet16BitValue(u16 nAddr);
extern u8   RegGetLByteValue(u16 nAddr);
extern u8   RegGetHByteValue(u16 nAddr);
extern void RegGetXBitValue(u16 nAddr, u8 *pRxData, u16 nLength, u16 nMaxI2cLengthLimit);
extern void RegSet16BitValue(u16 nAddr, u16 nData);
extern void RegSetLByteValue(u16 nAddr, u8 nData);
extern void RegSetHByteValue(u16 nAddr, u8 nData);
extern void RegSet16BitValueOn(u16 nAddr, u16 nData);
extern void RegSet16BitValueOff(u16 nAddr, u16 nData);
extern u16  RegGet16BitValueByAddressMode(u16 nAddr, AddressMode_e eAddressMode);
extern void RegSet16BitValueByAddressMode(u16 nAddr, u16 nData, AddressMode_e eAddressMode);
extern void RegMask16BitValue(u16 nAddr, u16 nMask, u16 nData, AddressMode_e eAddressMode);
extern s32 DbBusEnterSerialDebugMode(void);
extern void DbBusExitSerialDebugMode(void);
extern void DbBusIICUseBus(void);
extern void DbBusIICNotUseBus(void);
extern void DbBusIICReshape(void);
extern void DbBusStopMCU(void);
extern void DbBusNotStopMCU(void);
extern void DbBusResetSlave(void);
extern void DbBusWaitMCU(void);
extern s32 IicWriteData(u8 nSlaveId, u8 *pBuf, u16 nSize);
extern s32 IicReadData(u8 nSlaveId, u8 *pBuf, u16 nSize);
extern s32 IicSegmentReadDataByDbBus(u8 nRegBank, u8 nRegAddr, u8 *pBuf, u16 nSize, u16 nMaxI2cLengthLimit);
extern s32 IicSegmentReadDataBySmBus(u16 nAddr, u8 *pBuf, u16 nSize, u16 nMaxI2cLengthLimit);
extern void mstpMemSet(void *pDst, s8 nVal, u32 nSize);
extern void mstpMemCopy(void *pDst, void *pSource, u32 nSize);
extern void mstpDelay(u32 nTime);

#endif
