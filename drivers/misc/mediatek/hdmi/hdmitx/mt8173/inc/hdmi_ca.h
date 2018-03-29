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

#ifndef _HDMI_CA_H_
#define _HDMI_CA_H_
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT

bool fgCaHDMICreate(void);
bool fgCaHDMIClose(void);
void vCaHDMIWriteReg(unsigned int u4addr, unsigned int u4data);
void vCaHDMIWriteProReg(unsigned int u4addr, unsigned int u4data);
bool fgCaHDMIInstallHdcpKey(unsigned char *pdata, unsigned int u4Len);
bool fgCaHDMIGetAKsv(unsigned char *pdata);
bool fgCaHDMILoadHDCPKey(void);
bool fgCaHDMIHDCPReset(bool fgen);
bool fgCaHDMIHDCPEncEn(bool fgen);
bool fgCaHDMIVideoUnMute(bool fgen);
bool fgCaHDMIAudioUnMute(bool fgen);
void vCaDPI1WriteReg(unsigned int u4addr, unsigned int u4data);
bool fgCaHDMILoadROM(void);
void vCaHDCPFailState(unsigned int u4addr, unsigned int u4data);
void vCaHDCPOffState(unsigned int u4addr, unsigned int u4data);

#endif
#endif
