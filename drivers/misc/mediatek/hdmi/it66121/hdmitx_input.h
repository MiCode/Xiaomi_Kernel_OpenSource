/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _HDMITX_DEBUG_H_
#define _HDMITX_DEBUG_H_

#ifdef HDMITX_INPUT_INFO
extern HDMITXDEV hdmiTxDev[HDMITX_MAX_DEV_COUNT];

LONG CalcRCLK(void);
LONG CalcPCLK(void);
LONG CalcAudFS(void);
LONG CalcRCLK(void);
unsigned char hdmitx_getAudioCount(void);

unsigned short hdmitx_getInputHTotal(void);
unsigned short hdmitx_getInputVTotal(void);
bool hdmitx_isInputInterlace(void);
#endif

#endif
