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

#ifndef _DAPC_H
#define _DAPC_H
#include <mach/mt_typedefs.h>
#define MOD_NO_IN_1_DEVAPC      16
#define DEVAPC_TAG              "DEVAPC"
/*For EMI API DEVAPC0_D0_VIO_STA_4, idx:150*/
#define ABORT_EMI               0x00400000
/*Define constants*/
#define DEVAPC_DEVICE_NUMBER    143	/* E1:143 */
#define DEVAPC_DOMAIN_AP        0
#define DEVAPC_DOMAIN_MD        1
#define DEVAPC_DOMAIN_CONN      2
#define DEVAPC_DOMAIN_MM        3

#define VIO_DBG_MSTID           0x00003FFF
#define VIO_DBG_DMNID           0x0000C000
#define VIO_DBG_RW              0x30000000
#define VIO_DBG_CLR             0x80000000

#define DAPC_CLK_BIT            0x1

/* This macro should be removed later */
#define _8173_EARLY_PORTING

/******************************************************************************
*REGISTER ADDRESS DEFINATION
******************************************************************************/

/* DAPC Clock Address */

/* Disable PDN_CLK  (SET[0]) */
#define INFRA_PDN0                  ((P_kal_uint32)(devapc_clk_base+0x0070))
/* Enable PDN_CLK   (CLR[0]) */
#define INFRA_PDN1                  ((P_kal_uint32)(devapc_clk_base+0x0074))
/* PDN_CLK Status   (STA[0]: 0 -> Enable, 1 -> Disable) */
#define INFRA_PDN_STA               ((P_kal_uint32)(devapc_clk_base+0x0078))

#define DEVAPC0_D0_APC_0            ((P_kal_uint32)(devapc_ao_base+0x0000))
#define DEVAPC0_D0_APC_1            ((P_kal_uint32)(devapc_ao_base+0x0004))
#define DEVAPC0_D0_APC_2            ((P_kal_uint32)(devapc_ao_base+0x0008))
#define DEVAPC0_D0_APC_3            ((P_kal_uint32)(devapc_ao_base+0x000C))
#define DEVAPC0_D0_APC_4            ((P_kal_uint32)(devapc_ao_base+0x0010))
#define DEVAPC0_D0_APC_5            ((P_kal_uint32)(devapc_ao_base+0x0014))
#define DEVAPC0_D0_APC_6            ((P_kal_uint32)(devapc_ao_base+0x0018))
#define DEVAPC0_D0_APC_7            ((P_kal_uint32)(devapc_ao_base+0x001C))
#define DEVAPC0_D0_APC_8            ((P_kal_uint32)(devapc_ao_base+0x0020))

#define DEVAPC0_D1_APC_0            ((P_kal_uint32)(devapc_ao_base+0x0100))
#define DEVAPC0_D1_APC_1            ((P_kal_uint32)(devapc_ao_base+0x0104))
#define DEVAPC0_D1_APC_2            ((P_kal_uint32)(devapc_ao_base+0x0108))
#define DEVAPC0_D1_APC_3            ((P_kal_uint32)(devapc_ao_base+0x010C))
#define DEVAPC0_D1_APC_4            ((P_kal_uint32)(devapc_ao_base+0x0110))
#define DEVAPC0_D1_APC_5            ((P_kal_uint32)(devapc_ao_base+0x0114))
#define DEVAPC0_D1_APC_6            ((P_kal_uint32)(devapc_ao_base+0x0118))
#define DEVAPC0_D1_APC_7            ((P_kal_uint32)(devapc_ao_base+0x011C))
#define DEVAPC0_D1_APC_8            ((P_kal_uint32)(devapc_ao_base+0x0120))

#define DEVAPC0_D2_APC_0            ((P_kal_uint32)(devapc_ao_base+0x0200))
#define DEVAPC0_D2_APC_1            ((P_kal_uint32)(devapc_ao_base+0x0204))
#define DEVAPC0_D2_APC_2            ((P_kal_uint32)(devapc_ao_base+0x0208))
#define DEVAPC0_D2_APC_3            ((P_kal_uint32)(devapc_ao_base+0x020C))
#define DEVAPC0_D2_APC_4            ((P_kal_uint32)(devapc_ao_base+0x0210))
#define DEVAPC0_D2_APC_5            ((P_kal_uint32)(devapc_ao_base+0x0214))
#define DEVAPC0_D2_APC_6            ((P_kal_uint32)(devapc_ao_base+0x0218))
#define DEVAPC0_D2_APC_7            ((P_kal_uint32)(devapc_ao_base+0x021C))
#define DEVAPC0_D2_APC_8            ((P_kal_uint32)(devapc_ao_base+0x0220))

#define DEVAPC0_D3_APC_0            ((P_kal_uint32)(devapc_ao_base+0x0300))
#define DEVAPC0_D3_APC_1            ((P_kal_uint32)(devapc_ao_base+0x0304))
#define DEVAPC0_D3_APC_2            ((P_kal_uint32)(devapc_ao_base+0x0308))
#define DEVAPC0_D3_APC_3            ((P_kal_uint32)(devapc_ao_base+0x030C))
#define DEVAPC0_D3_APC_4            ((P_kal_uint32)(devapc_ao_base+0x0310))
#define DEVAPC0_D3_APC_5            ((P_kal_uint32)(devapc_ao_base+0x0314))
#define DEVAPC0_D3_APC_6            ((P_kal_uint32)(devapc_ao_base+0x0318))
#define DEVAPC0_D3_APC_7            ((P_kal_uint32)(devapc_ao_base+0x031C))
#define DEVAPC0_D3_APC_8            ((P_kal_uint32)(devapc_ao_base+0x0320))

#define DEVAPC0_MAS_DOM_0           ((P_kal_uint32)(devapc_ao_base+0x0400))
#define DEVAPC0_MAS_DOM_1           ((P_kal_uint32)(devapc_ao_base+0x0404))
#define DEVAPC0_MAS_SEC             ((P_kal_uint32)(devapc_ao_base+0x0500))
#define DEVAPC0_APC_CON             ((P_kal_uint32)(devapc_ao_base+0x0F00))
#define DEVAPC0_APC_LOCK_0          ((P_kal_uint32)(devapc_ao_base+0x0F04))
#define DEVAPC0_APC_LOCK_1          ((P_kal_uint32)(devapc_ao_base+0x0F08))
#define DEVAPC0_APC_LOCK_2          ((P_kal_uint32)(devapc_ao_base+0x0F0C))
#define DEVAPC0_APC_LOCK_3          ((P_kal_uint32)(devapc_ao_base+0x0F10))
#define DEVAPC0_APC_LOCK_4          ((P_kal_uint32)(devapc_ao_base+0x0F14))

#define DEVAPC0_PD_APC_CON          ((P_kal_uint32)(devapc_pd_base+0x0F00))
#define DEVAPC0_D0_VIO_MASK_0       ((P_kal_uint32)(devapc_pd_base+0x0000))
#define DEVAPC0_D0_VIO_MASK_1       ((P_kal_uint32)(devapc_pd_base+0x0004))
#define DEVAPC0_D0_VIO_MASK_2       ((P_kal_uint32)(devapc_pd_base+0x0008))
#define DEVAPC0_D0_VIO_MASK_3       ((P_kal_uint32)(devapc_pd_base+0x000C))
#define DEVAPC0_D0_VIO_MASK_4       ((P_kal_uint32)(devapc_pd_base+0x0010))
#define DEVAPC0_D0_VIO_STA_0        ((P_kal_uint32)(devapc_pd_base+0x0400))
#define DEVAPC0_D0_VIO_STA_1        ((P_kal_uint32)(devapc_pd_base+0x0404))
#define DEVAPC0_D0_VIO_STA_2        ((P_kal_uint32)(devapc_pd_base+0x0408))
#define DEVAPC0_D0_VIO_STA_3        ((P_kal_uint32)(devapc_pd_base+0x040C))
#define DEVAPC0_D0_VIO_STA_4        ((P_kal_uint32)(devapc_pd_base+0x0410))
#define DEVAPC0_VIO_DBG0            ((P_kal_uint32)(devapc_pd_base+0x0900))
#define DEVAPC0_VIO_DBG1            ((P_kal_uint32)(devapc_pd_base+0x0904))
#define DEVAPC0_DEC_ERR_CON         ((P_kal_uint32)(devapc_pd_base+0x0F80))
#define DEVAPC0_DEC_ERR_ADDR        ((P_kal_uint32)(devapc_pd_base+0x0F84))
#define DEVAPC0_DEC_ERR_ID          ((P_kal_uint32)(devapc_pd_base+0x0F88))

struct DEVICE_INFO {
	const char *device;
	bool forbidden;
};

#ifdef CONFIG_MTK_HIBERNATION
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
#endif

#endif
