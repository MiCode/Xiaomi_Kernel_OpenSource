/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslTDA9989_local_otp.h
 *
 * \version       %version: 1 %
 *
*/

#ifndef TMBSLTDA9989_LOCAL_OTP_H
#define TMBSLTDA9989_LOCAL_OTP_H


/*============================================================================*/
/*                       ENUM OR TYPE DEFINITIONS                             */
/*============================================================================*/

#define BINARY(d7, d6, d5, d4, d3, d2, d1, d0) \
    (((d7)<<7)|((d6)<<6)|((d5)<<5)|((d4)<<4)|((d3)<<3)|((d2)<<2)|((d1)<<1)|(d0))

enum _eRegOtp {
#ifdef TMFL_HDCP_SUPPORT
	E_REG_P12_CTRL_W = SPA(E_SP12_CTRL, E_PAGE_12, 0x40),
#ifdef BCAPS_REPEATER
	E_REG_P12_BCAPS_W = SPA(E_SP12_BCAPS, E_PAGE_12, 0x49),
#else
	E_REG_P12_BCAPS_W = SPA(E_SNONE, E_PAGE_12, 0x49),
#endif				/* BCAPS_REPEATER */
#endif
	E_REG_P12_TX0_RW = SPA(E_SNONE, E_PAGE_12, 0x97),
	E_REG_P12_TX3_RW = SPA(E_SNONE, E_PAGE_12, 0x9A),
#ifdef TMFL_HDCP_OPTIMIZED_POWER
	E_REG_P12_TX4_RW = SPA(E_SNONE, E_PAGE_12, 0x9B),
#endif
	E_REG_P12_TX33_RW = SPA(E_SNONE, E_PAGE_12, 0xB8),
};

enum _eMaskRegOtp {
	E_MASKREG_P12_TX33_hdmi = BINARY(0, 0, 0, 0, 0, 0, 1, 0),
	E_MASKREG_P12_TX0_sr_hdcp = BINARY(0, 0, 0, 0, 0, 0, 0, 1),
#ifdef TMFL_HDCP_OPTIMIZED_POWER
	E_MASKREG_P12_TX4_pd_rg = BINARY(0, 0, 0, 0, 0, 1, 0, 0),
	E_MASKREG_P12_TX4_pd_ram = BINARY(0, 0, 0, 0, 0, 0, 1, 0),
	E_MASKREG_P12_TX4_pd_hdcp = BINARY(0, 0, 0, 0, 0, 0, 0, 1),
	E_MASKREG_P12_TX4_pd_all = BINARY(0, 0, 0, 0, 0, 1, 1, 1),
#endif
};

#endif				/* TMBSLTDA9989_LOCAL_OTP_H */
