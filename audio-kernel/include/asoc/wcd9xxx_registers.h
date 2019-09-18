/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _WCD9XXX_REGISTERS_H
#define _WCD9XXX_REGISTERS_H

#define WCD9XXX_BASE_ADDRESS 0x3000

#define WCD9XXX_ANA_RX_SUPPLIES                     (WCD9XXX_BASE_ADDRESS+0x008)
#define WCD9XXX_ANA_HPH                             (WCD9XXX_BASE_ADDRESS+0x009)
#define WCD9XXX_CLASSH_MODE_2                       (WCD9XXX_BASE_ADDRESS+0x098)
#define WCD9XXX_CLASSH_MODE_3                       (WCD9XXX_BASE_ADDRESS+0x099)
#define WCD9XXX_FLYBACK_VNEG_CTRL_1                 (WCD9XXX_BASE_ADDRESS+0x0A5)
#define WCD9XXX_FLYBACK_VNEG_CTRL_4                 (WCD9XXX_BASE_ADDRESS+0x0A8)
#define WCD9XXX_FLYBACK_VNEGDAC_CTRL_2              (WCD9XXX_BASE_ADDRESS+0x0AF)
#define WCD9XXX_RX_BIAS_HPH_LOWPOWER                (WCD9XXX_BASE_ADDRESS+0x0BF)
#define WCD9XXX_RX_BIAS_FLYB_BUFF                   (WCD9XXX_BASE_ADDRESS+0x0C7)
#define WCD9XXX_HPH_PA_CTL1                         (WCD9XXX_BASE_ADDRESS+0x0D1)
#define WCD9XXX_HPH_NEW_INT_PA_MISC2                (WCD9XXX_BASE_ADDRESS+0x138)

#endif
