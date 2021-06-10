/*
 * Copyright (C) 2017 MediaTek Inc.
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


#ifndef __MTK_PROJECT_PHY_H
#define __MTK_PROJECT_PHY_H

#define SSUSB_SIFSLV_SPLLC_BASE         (MTK_USB_PHY_SPLLC_BASE)
#define SSUSB_SIFSLV_FMREG              (MTK_USB_PHY_FMREG_BASE)
#define SSUSB_SIFSLV_U2PHY_COM_BASE     (MTK_USB_PHY_U2_BASE)
#define SSUSB_SIFSLV_U3PHYD_BASE        (MTK_USB_PHY_U3PHYD_BASE)
#define SSUSB_USB30_PHYA_SIV_B2_BASE    (MTK_USB_PHY_B2_BASE)
#define SSUSB_USB30_PHYA_SIV_B_BASE     (MTK_USB_PHY_PHYA_BASE)
#define SSUSB_SIFSLV_U3PHYA_DA_BASE     (MTK_USB_PHY_PHYA_DA_BASE)

/* ///////////////////////////////////////////// */
/**   SSUSB_SIFSLV_SPLLC_BASE **/
#define U3D_SPLLC_XTALCTL3              (SSUSB_SIFSLV_SPLLC_BASE + 0x18)

/* XTALCTL3 */
#define RG_SSUSB_XTALCTL_REV                      (0xf<<12)
#define RG_SSUSB_BIASIMR_EN                       (0x1<<11)
#define RG_SSUSB_FORCE_BIASIMR_EN                 (0x1<<10)
#define RG_SSUSB_XTAL_RX_PWD                      (0x1<<9)
#define RG_SSUSB_FRC_XTAL_RX_PWD                  (0x1<<8)
#define RG_SSUSB_CKBG_PROB_SEL                    (0x3<<6)
#define RG_SSUSB_XTAL_PROB_SEL                    (0x3<<4)
#define RG_SSUSB_XTAL_VREGBIAS_LPF_ENB            (0x1<<3)
#define RG_SSUSB_XTAL_FRC_VREGBIAS_LPF_ENB        (0x1<<2)
#define RG_SSUSB_XTAL_VREGBIAS_PWD                (0x1<<1)
#define RG_SSUSB_XTAL_FRC_VREGBIAS_PWD            (0x1<<0)

#define RG_SSUSB_XTALCTL_REV_OFST                 (12)
#define RG_SSUSB_BIASIMR_EN_OFST                  (11)
#define RG_SSUSB_FORCE_BIASIMR_EN_OFST            (10)
#define RG_SSUSB_XTAL_RX_PWD_OFST                 (9)
#define RG_SSUSB_FRC_XTAL_RX_PWD_OFST             (8)
#define RG_SSUSB_CKBG_PROB_SEL_OFST               (6)
#define RG_SSUSB_XTAL_PROB_SEL_OFST               (4)
#define RG_SSUSB_XTAL_VREGBIAS_LPF_ENB_OFST       (3)
#define RG_SSUSB_XTAL_FRC_VREGBIAS_LPF_ENB_OFST   (2)
#define RG_SSUSB_XTAL_VREGBIAS_PWD_OFST           (1)
#define RG_SSUSB_XTAL_FRC_VREGBIAS_PWD_OFST       (0)

/* ///////////////////////////////////////////// */
/**   SSUSB_SIFSLV_FMREG **/
#define RG_SSUSB_SIFSLV_FMCR0           (SSUSB_SIFSLV_FMREG + 0x00)
#define RG_SSUSB_SIFSLV_FMCR1           (SSUSB_SIFSLV_FMREG + 0x04)
#define RG_SSUSB_SIFSLV_FMCR2           (SSUSB_SIFSLV_FMREG + 0x08)
#define RG_SSUSB_SIFSLV_FMMONR0         (SSUSB_SIFSLV_FMREG + 0x0c)
#define RG_SSUSB_SIFSLV_FMMONR1         (SSUSB_SIFSLV_FMREG + 0x10)

/* RG_SSUSB_SIFSLV_FMCR0 */
#define RG_LOCKTH                       (0xf<<28)
#define RG_MONCLK_SEL                   (0x3<<26)
#define RG_FM_MODE                      (0x1<<25)
#define RG_FREQDET_EN                   (0x1<<24)
#define RG_CYCLECNT                     (0xffffff<<0)

#define RG_LOCKTH_OFST                  (28)
#define RG_MONCLK_SEL_OFST              (26)
#define RG_FM_MODE_OFST                 (25)
#define RG_FREQDET_EN_OFST              (24)
#define RG_CYCLECNT_OFST                (0)

/* RG_SSUSB_SIFSLV_FMCR1 */
#define RG_TARGET                       (0xffffffff<<0)

#define RG_TARGET_OFST                  (0)

/* RG_SSUSB_SIFSLV_FMCR2 */
#define RG_OFFSET                       (0xffffffff<<0)

#define RG_OFFSET_OFST                  (0)

/* RG_SSUSB_SIFSLV_FMMONR0 */
#define USB_FM_OUT                      (0xffffffff<<0)

#define USB_FM_OUT_OFST                 (0)

/* RG_SSUSB_SIFSLV_FMMONR1 */
#define RG_MONCLK_SEL_2                 (0x1<<9)
#define RG_FRCK_EN                      (0x1<<8)
#define USBPLL_LOCK                     (0x1<<1)
#define USB_FM_VLD                      (0x1<<0)

#define RG_MONCLK_SEL_2_OFST            (9)
#define RG_FRCK_EN_OFST                 (8)
#define USBPLL_LOCK_OFST                (1)
#define USB_FM_VLD_OFST                 (0)


#define U2_SR_COEF                      28
#define U3D_PHY_REF_CK                  26

/* ///////////////////////////////////////////// */
/**   SSUSB_SIFSLV_U2PHY_COM_BASE **/
#define U3D_USBPHYACR0      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0000)
#define U3D_USBPHYACR1      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0004)
#define U3D_USBPHYACR2      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0008)
#define U3D_USBPHYACR5      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0014)
#define U3D_USBPHYACR6      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0018)
#define U3D_U2PHYACR3       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x001c)
#define U3D_U2PHYACR4       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0020)
#define U3D_U2PHYDCR0       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0060)
#define U3D_U2PHYDCR1       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0064)
#define U3D_U2PHYDTM0       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0068)
#define U3D_U2PHYDTM1       (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x006C)
#define U3D_U2PHYDMON1      (SSUSB_SIFSLV_U2PHY_COM_BASE + 0x0074)

/* U3D_USBPHYACR0 */
#define RG_USB20_MPX_OUT_SEL               (0x7<<28)
#define RG_USB20_TX_PH_ROT_SEL             (0x7<<24)
#define RG_USB20_PLL_DIVEN                 (0x7<<20)
#define RG_USB20_PLL_BR                    (0x1<<18)
#define RG_USB20_PLL_BP                    (0x1<<17)
#define RG_USB20_PLL_BLP                   (0x1<<16)
#define RG_USB20_USBPLL_FORCE_ON           (0x1<<15)
#define RG_USB20_PLL_FBDIV                 (0x7f<<8)
#define RG_USB20_PLL_PREDIV                (0x3<<6)
#define RG_USB20_INTR_EN                   (0x1<<5)
#define RG_USB20_REF_EN                    (0x1<<4)
#define RG_USB20_BGR_DIV                   (0x3<<2)
#define RG_SIFSLV_CHP_EN                   (0x1<<1)
#define RG_SIFSLV_BGR_EN                   (0x1<<0)

#define RG_USB20_MPX_OUT_SEL_OFST          (28)
#define RG_USB20_TX_PH_ROT_SEL_OFST        (24)
#define RG_USB20_PLL_DIVEN_OFST            (20)
#define RG_USB20_PLL_BR_OFST               (18)
#define RG_USB20_PLL_BP_OFST               (17)
#define RG_USB20_PLL_BLP_OFST              (16)
#define RG_USB20_USBPLL_FORCE_ON_OFST      (15)
#define RG_USB20_PLL_FBDIV_OFST            (8)
#define RG_USB20_PLL_PREDIV_OFST           (6)
#define RG_USB20_INTR_EN_OFST              (5)
#define RG_USB20_REF_EN_OFST               (4)
#define RG_USB20_BGR_DIV_OFST              (2)
#define RG_SIFSLV_CHP_EN_OFST              (1)
#define RG_SIFSLV_BGR_EN_OFST              (0)


/* U3D_USBPHYACR1 */
#define RG_USB20_INTR_CAL                  (0x1f<<19)
#define RG_USB20_OTG_VBUSTH                (0x7<<16)
#define RG_USB20_VRT_VREF_SEL              (0x7<<12)
#define RG_USB20_TERM_VREF_SEL             (0x7<<8)
#define RG_USB20_MPX_SEL                   (0xff<<0)

#define RG_USB20_INTR_CAL_OFST             (19)
#define RG_USB20_OTG_VBUSTH_OFST           (16)
#define RG_USB20_VRT_VREF_SEL_OFST         (12)
#define RG_USB20_TERM_VREF_SEL_OFST        (8)
#define RG_USB20_MPX_SEL_OFST              (0)

/* U3D_USBPHYACR2 */
#define RG_SIFSLV_USB20_PLL_FORCE_MODE		(0x1<<18)
#define RG_SIFSLV_MAC_BANDGAP_EN           (0x1<<17)
#define RG_SIFSLV_MAC_CHOPPER_EN           (0x1<<16)
#define RG_USB20_CLKREF_REV                (0xffff<<0)

#define RG_SIFSLV_USB20_PLL_FORCE_MODE_OFST	(18)
#define RG_SIFSLV_MAC_BANDGAP_EN_OFST      (17)
#define RG_SIFSLV_MAC_CHOPPER_EN_OFST      (16)
#define RG_USB20_CLKREF_REV_OFST           (0)

/* U3D_USBPHYACR5 */
#define RG_USB20_DISC_FIT_EN               (0x1<<28)
#define RG_USB20_INIT_SQ_EN_DG             (0x3<<26)
#define RG_USB20_HSTX_TMODE_SEL            (0x3<<24)
#define RG_USB20_SQD                       (0x3<<22)
#define RG_USB20_DISCD                     (0x3<<20)
#define RG_USB20_HSTX_TMODE_EN             (0x1<<19)
#define RG_USB20_PHYD_MONEN                (0x1<<18)
#define RG_USB20_INLPBK_EN                 (0x1<<17)
#define RG_USB20_CHIRP_EN                  (0x1<<16)
#define RG_USB20_HSTX_SRCAL_EN             (0x1<<15)
#define RG_USB20_HSTX_SRCTRL               (0x7<<12)
#define RG_USB20_HS_100U_U3_EN             (0x1<<11)
#define RG_USB20_GBIAS_ENB                 (0x1<<10)
#define RG_USB20_DM_ABIST_SOURCE_EN        (0x1<<7)
#define RG_USB20_DM_ABIST_SELE             (0xf<<0)

#define RG_USB20_DISC_FIT_EN_OFST          (28)
#define RG_USB20_INIT_SQ_EN_DG_OFST        (26)
#define RG_USB20_HSTX_TMODE_SEL_OFST       (24)
#define RG_USB20_SQD_OFST                  (22)
#define RG_USB20_DISCD_OFST                (20)
#define RG_USB20_HSTX_TMODE_EN_OFST        (19)
#define RG_USB20_PHYD_MONEN_OFST           (18)
#define RG_USB20_INLPBK_EN_OFST            (17)
#define RG_USB20_CHIRP_EN_OFST             (16)
#define RG_USB20_HSTX_SRCAL_EN_OFST        (15)
#define RG_USB20_HSTX_SRCTRL_OFST          (12)
#define RG_USB20_HS_100U_U3_EN_OFST        (11)
#define RG_USB20_GBIAS_ENB_OFST            (10)
#define RG_USB20_DM_ABIST_SOURCE_EN_OFST   (7)
#define RG_USB20_DM_ABIST_SELE_OFST        (0)

/* U3D_USBPHYACR6 */
#define RG_USB20_PHY_REV_6                 (0x3<<30)
#define RG_USB20_PHY_REV                   (0xef<<24)
#define RG_USB20_BC11_SW_EN                (0x1<<23)
#define RG_USB20_SR_CLK_SEL                (0x1<<22)
#define RG_USB20_OTG_VBUSCMP_EN            (0x1<<20)
#define RG_USB20_OTG_ABIST_EN              (0x1<<19)
#define RG_USB20_OTG_ABIST_SELE            (0x7<<16)
#define RG_USB20_HSRX_MMODE_SELE           (0x3<<12)
#define RG_USB20_HSRX_BIAS_EN_SEL          (0x3<<9)
#define RG_USB20_HSRX_TMODE_EN             (0x1<<8)
#define RG_USB20_DISCTH                    (0xf<<4)
#define RG_USB20_SQTH                      (0xf<<0)

#define RG_USB20_PHY_REV_6_OFST            (30)
#define RG_USB20_PHY_REV_OFST              (24)
#define RG_USB20_BC11_SW_EN_OFST           (23)
#define RG_USB20_SR_CLK_SEL_OFST           (22)
#define RG_USB20_OTG_VBUSCMP_EN_OFST       (20)
#define RG_USB20_OTG_ABIST_EN_OFST         (19)
#define RG_USB20_OTG_ABIST_SELE_OFST       (16)
#define RG_USB20_HSRX_MMODE_SELE_OFST      (12)
#define RG_USB20_HSRX_BIAS_EN_SEL_OFST     (9)
#define RG_USB20_HSRX_TMODE_EN_OFST        (8)
#define RG_USB20_DISCTH_OFST               (4)
#define RG_USB20_SQTH_OFST                 (0)

/* U3D_U2PHYACR3 */
#define RG_USB20_HSTX_DBIST                (0xf<<28)
#define RG_USB20_HSTX_BIST_EN              (0x1<<26)
#define RG_USB20_HSTX_I_EN_MODE            (0x3<<24)
#define RG_USB20_USB11_TMODE_EN            (0x1<<19)
#define RG_USB20_TMODE_FS_LS_TX_EN         (0x1<<18)
#define RG_USB20_TMODE_FS_LS_RCV_EN        (0x1<<17)
#define RG_USB20_TMODE_FS_LS_MODE          (0x1<<16)
#define RG_USB20_HS_TERM_EN_MODE           (0x3<<13)
#define RG_USB20_PUPD_BIST_EN              (0x1<<12)
#define RG_USB20_EN_PU_DM                  (0x1<<11)
#define RG_USB20_EN_PD_DM                  (0x1<<10)
#define RG_USB20_EN_PU_DP                  (0x1<<9)
#define RG_USB20_EN_PD_DP                  (0x1<<8)

#define RG_USB20_HSTX_DBIST_OFST           (28)
#define RG_USB20_HSTX_BIST_EN_OFST         (26)
#define RG_USB20_HSTX_I_EN_MODE_OFST       (24)
#define RG_USB20_USB11_TMODE_EN_OFST       (19)
#define RG_USB20_TMODE_FS_LS_TX_EN_OFST    (18)
#define RG_USB20_TMODE_FS_LS_RCV_EN_OFST   (17)
#define RG_USB20_TMODE_FS_LS_MODE_OFST     (16)
#define RG_USB20_HS_TERM_EN_MODE_OFST      (13)
#define RG_USB20_PUPD_BIST_EN_OFST         (12)
#define RG_USB20_EN_PU_DM_OFST             (11)
#define RG_USB20_EN_PD_DM_OFST             (10)
#define RG_USB20_EN_PU_DP_OFST             (9)
#define RG_USB20_EN_PD_DP_OFST             (8)

/* U3D_U2PHYACR4 */
#define RG_USB20_DP_100K_MODE              (0x1<<18)
#define RG_USB20_DM_100K_EN                (0x1<<17)
#define USB20_DP_100K_EN                   (0x1<<16)
#define USB20_GPIO_DM_I                    (0x1<<15)
#define USB20_GPIO_DP_I                    (0x1<<14)
#define USB20_GPIO_DM_OE                   (0x1<<13)
#define USB20_GPIO_DP_OE                   (0x1<<12)
#define RG_USB20_GPIO_CTL                  (0x1<<9)
#define USB20_GPIO_MODE                    (0x1<<8)
#define RG_USB20_TX_BIAS_EN                (0x1<<5)
#define RG_USB20_TX_VCMPDN_EN              (0x1<<4)
#define RG_USB20_HS_SQ_EN_MODE             (0x3<<2)
#define RG_USB20_HS_RCV_EN_MODE            (0x3<<0)

#define RG_USB20_DP_100K_MODE_OFST         (18)
#define RG_USB20_DM_100K_EN_OFST           (17)
#define USB20_DP_100K_EN_OFST              (16)
#define USB20_GPIO_DM_I_OFST               (15)
#define USB20_GPIO_DP_I_OFST               (14)
#define USB20_GPIO_DM_OE_OFST              (13)
#define USB20_GPIO_DP_OE_OFST              (12)
#define RG_USB20_GPIO_CTL_OFST             (9)
#define USB20_GPIO_MODE_OFST               (8)
#define RG_USB20_TX_BIAS_EN_OFST           (5)
#define RG_USB20_TX_VCMPDN_EN_OFST         (4)
#define RG_USB20_HS_SQ_EN_MODE_OFST        (2)
#define RG_USB20_HS_RCV_EN_MODE_OFST       (0)

/* U3D_U2PHYDCR0 */
#define RG_USB20_CDR_TST                   (0x3<<30)
#define RG_USB20_GATED_ENB                 (0x1<<29)
#define RG_USB20_TESTMODE                  (0x3<<26)
#define RG_SIFSLV_USB20_PLL_STABLE         (0x1<<25)
#define RG_SIFSLV_USB20_PLL_FORCE_ON       (0x1<<24)
#define RG_USB20_PHYD_RESERVE              (0xffff<<8)
#define RG_USB20_EBTHRLD                   (0x1<<7)
#define RG_USB20_EARLY_HSTX_I              (0x1<<6)
#define RG_USB20_TX_TST                    (0x1<<5)
#define RG_USB20_NEGEDGE_ENB               (0x1<<4)
#define RG_USB20_CDR_FILT                  (0xf<<0)

#define RG_USB20_CDR_TST_OFST              (30)
#define RG_USB20_GATED_ENB_OFST            (29)
#define RG_USB20_TESTMODE_OFST             (26)
#define RG_SIFSLV_USB20_PLL_STABLE_OFST    (25)
#define RG_SIFSLV_USB20_PLL_FORCE_ON_OFST  (24)
#define RG_USB20_PHYD_RESERVE_OFST         (8)
#define RG_USB20_EBTHRLD_OFST              (7)
#define RG_USB20_EARLY_HSTX_I_OFST         (6)
#define RG_USB20_TX_TST_OFST               (5)
#define RG_USB20_NEGEDGE_ENB_OFST          (4)
#define RG_USB20_CDR_FILT_OFST             (0)

/* U3D_U2PHYDCR1 */
#define RG_USB20_PROBE_SEL                 (0xff<<24)
#define RG_USB20_DRVVBUS                   (0x1<<23)
#define RG_DEBUG_EN                        (0x1<<22)
#define RG_USB20_OTG_PROBE                 (0x3<<20)
#define RG_USB20_SW_PLLMODE                (0x3<<18)
#define E60802_RG_USB20_SW_PLLMODE         (0x3<<18)
#define RG_USB20_BERTH                     (0x3<<16)
#define RG_USB20_LBMODE                    (0x3<<13)
#define RG_USB20_FORCE_TAP                 (0x1<<12)
#define RG_USB20_TAPSEL                    (0xfff<<0)

/* U3D_U2PHYDCR1 */
#define RG_USB20_PROBE_SEL_OFST            (24)
#define RG_USB20_DRVVBUS_OFST              (23)
#define RG_DEBUG_EN_OFST                   (22)
#define RG_USB20_OTG_PROBE_OFST            (20)
#define RG_USB20_SW_PLLMODE_OFST           (18)
#define E60802_RG_USB20_SW_PLLMODE_OFST    (18)
#define RG_USB20_BERTH_OFST                (16)
#define RG_USB20_LBMODE_OFST               (13)
#define RG_USB20_FORCE_TAP_OFST            (12)
#define RG_USB20_TAPSEL_OFST               (0)

/* U3D_U2PHYDTM0 */
#define RG_UART_MODE                       (0x3<<30)
#define FORCE_UART_I                       (0x1<<29)
#define FORCE_UART_BIAS_EN                 (0x1<<28)
#define FORCE_UART_TX_OE                   (0x1<<27)
#define FORCE_UART_EN                      (0x1<<26)
#define FORCE_USB_CLKEN                    (0x1<<25)
#define FORCE_DRVVBUS                      (0x1<<24)
#define FORCE_DATAIN                       (0x1<<23)
#define FORCE_TXVALID                      (0x1<<22)
#define FORCE_DM_PULLDOWN                  (0x1<<21)
#define FORCE_DP_PULLDOWN                  (0x1<<20)
#define FORCE_XCVRSEL                      (0x1<<19)
#define FORCE_SUSPENDM                     (0x1<<18)
#define FORCE_TERMSEL                      (0x1<<17)
#define FORCE_OPMODE                       (0x1<<16)
#define UTMI_MUXSEL                        (0x1<<15)
#define RG_RESET                           (0x1<<14)
#define RG_DATAIN                          (0xf<<10)
#define RG_TXVALIDH                        (0x1<<9)
#define RG_TXVALID                         (0x1<<8)
#define RG_DMPULLDOWN                      (0x1<<7)
#define RG_DPPULLDOWN                      (0x1<<6)
#define RG_XCVRSEL                         (0x3<<4)
#define RG_SUSPENDM                        (0x1<<3)
#define RG_TERMSEL                         (0x1<<2)
#define RG_OPMODE                          (0x3<<0)

#define RG_UART_MODE_OFST                  (30)
#define FORCE_UART_I_OFST                  (29)
#define FORCE_UART_BIAS_EN_OFST            (28)
#define FORCE_UART_TX_OE_OFST              (27)
#define FORCE_UART_EN_OFST                 (26)
#define FORCE_USB_CLKEN_OFST               (25)
#define FORCE_DRVVBUS_OFST                 (24)
#define FORCE_DATAIN_OFST                  (23)
#define FORCE_TXVALID_OFST                 (22)
#define FORCE_DM_PULLDOWN_OFST             (21)
#define FORCE_DP_PULLDOWN_OFST             (20)
#define FORCE_XCVRSEL_OFST                 (19)
#define FORCE_SUSPENDM_OFST                (18)
#define FORCE_TERMSEL_OFST                 (17)
#define FORCE_OPMODE_OFST                  (16)
#define UTMI_MUXSEL_OFST                   (15)
#define RG_RESET_OFST                      (14)
#define RG_DATAIN_OFST                     (10)
#define RG_TXVALIDH_OFST                   (9)
#define RG_TXVALID_OFST                    (8)
#define RG_DMPULLDOWN_OFST                 (7)
#define RG_DPPULLDOWN_OFST                 (6)
#define RG_XCVRSEL_OFST                    (4)
#define RG_SUSPENDM_OFST                   (3)
#define RG_TERMSEL_OFST                    (2)
#define RG_OPMODE_OFST                     (0)

/* U3D_U2PHYDTM1 */
#define RG_USB20_PRBS7_EN                  (0x1<<31)
#define RG_USB20_PRBS7_BITCNT              (0x3f<<24)
#define RG_USB20_CLK48M_EN                 (0x1<<23)
#define RG_USB20_CLK60M_EN                 (0x1<<22)
#define RG_UART_I                          (0x1<<19)
#define RG_UART_BIAS_EN                    (0x1<<18)
#define RG_UART_TX_OE                      (0x1<<17)
#define RG_UART_EN                         (0x1<<16)
#define RG_IP_U2_PORT_POWER                (0x1<<15)
#define FORCE_IP_U2_PORT_POWER             (0x1<<14)
#define FORCE_VBUSVALID                    (0x1<<13)
#define FORCE_SESSEND                      (0x1<<12)
#define FORCE_BVALID                       (0x1<<11)
#define FORCE_AVALID                       (0x1<<10)
#define FORCE_IDDIG                        (0x1<<9)
#define FORCE_IDPULLUP                     (0x1<<8)
#define RG_VBUSVALID                       (0x1<<5)
#define RG_SESSEND                         (0x1<<4)
#define RG_BVALID                          (0x1<<3)
#define RG_AVALID                          (0x1<<2)
#define RG_IDDIG                           (0x1<<1)
#define RG_IDPULLUP                        (0x1<<0)

#define RG_USB20_PRBS7_EN_OFST             (31)
#define RG_USB20_PRBS7_BITCNT_OFST         (24)
#define RG_USB20_CLK48M_EN_OFST            (23)
#define RG_USB20_CLK60M_EN_OFST            (22)
#define RG_UART_I_OFST                     (19)
#define RG_UART_BIAS_EN_OFST               (18)
#define RG_UART_TX_OE_OFST                 (17)
#define RG_UART_EN_OFST                    (16)
#define RG_IP_U2_PORT_POWER_OFST           (15)
#define FORCE_IP_U2_PORT_POWER_OFST        (14)
#define FORCE_VBUSVALID_OFST               (13)
#define FORCE_SESSEND_OFST                 (12)
#define FORCE_BVALID_OFST                  (11)
#define FORCE_AVALID_OFST                  (10)
#define FORCE_IDDIG_OFST                   (9)
#define FORCE_IDPULLUP_OFST                (8)
#define RG_VBUSVALID_OFST                  (5)
#define RG_SESSEND_OFST                    (4)
#define RG_BVALID_OFST                     (3)
#define RG_AVALID_OFST                     (2)
#define RG_IDDIG_OFST                      (1)
#define RG_IDPULLUP_OFST                   (0)

/* U3D_U2PHYDMON1 */
#define USB20_UART_O                       (0x1<<31)
#define RGO_USB20_LB_PASS                  (0x1<<30)
#define RGO_USB20_LB_DONE                  (0x1<<29)
#define AD_USB20_BVALID                    (0x1<<28)
#define USB20_IDDIG                        (0x1<<27)
#define AD_USB20_VBUSVALID                 (0x1<<26)
#define AD_USB20_SESSEND                   (0x1<<25)
#define AD_USB20_AVALID                    (0x1<<24)
#define USB20_LINE_STATE                   (0x3<<22)
#define USB20_HST_DISCON                   (0x1<<21)
#define USB20_TX_READY                     (0x1<<20)
#define USB20_RX_ERROR                     (0x1<<19)
#define USB20_RX_ACTIVE                    (0x1<<18)
#define USB20_RX_VALIDH                    (0x1<<17)
#define USB20_RX_VALID                     (0x1<<16)
#define USB20_DATA_OUT                     (0xffff<<0)

#define USB20_UART_O_OFST                  (31)
#define RGO_USB20_LB_PASS_OFST             (30)
#define RGO_USB20_LB_DONE_OFST             (29)
#define AD_USB20_BVALID_OFST               (28)
#define USB20_IDDIG_OFST                   (27)
#define AD_USB20_VBUSVALID_OFST            (26)
#define AD_USB20_SESSEND_OFST              (25)
#define AD_USB20_AVALID_OFST               (24)
#define USB20_LINE_STATE_OFST              (22)
#define USB20_HST_DISCON_OFST              (21)
#define USB20_TX_READY_OFST                (20)
#define USB20_RX_ERROR_OFST                (19)
#define USB20_RX_ACTIVE_OFST               (18)
#define USB20_RX_VALIDH_OFST               (17)
#define USB20_RX_VALID_OFST                (16)
#define USB20_DATA_OUT_OFST                (0)

/* ///////////////////////////////////////////// */
/**   SSUSB_SIFSLV_U3PHYD_BASE **/
#define U3D_PHYD_MIX0		(SSUSB_SIFSLV_U3PHYD_BASE + 0x00)
#define U3D_PHYD_IMPCAL0	(SSUSB_SIFSLV_U3PHYD_BASE + 0x10)
#define U3D_PHYD_IMPCAL1	(SSUSB_SIFSLV_U3PHYD_BASE + 0x14)
#define U3D_PHYD_RX0		(SSUSB_SIFSLV_U3PHYD_BASE + 0x2c)
#define U3D_PHYD_T2RLB		(SSUSB_SIFSLV_U3PHYD_BASE + 0x30)
#define U3D_PHYD_PIPE0		(SSUSB_SIFSLV_U3PHYD_BASE + 0x40)
#define U3D_PHYD_CDR0		(SSUSB_SIFSLV_U3PHYD_BASE + 0x58)
#define U3D_PHYD_CDR1		(SSUSB_SIFSLV_U3PHYD_BASE + 0x5c)
#define U3D_PHYD_EQ_EYE3	(SSUSB_SIFSLV_U3PHYD_BASE + 0xdc)
#define U3D_PHYD_MIX6		(SSUSB_SIFSLV_U3PHYD_BASE + 0xe8)



/* U3D_PHYD_IMPCAL0 */
#define RG_SSUSB_FORCE_TX_IMPSEL           (0x1<<31)
#define RG_SSUSB_TX_IMPCAL_EN              (0x1<<30)
#define RG_SSUSB_FORCE_TX_IMPCAL_EN        (0x1<<29)
#define RG_SSUSB_TX_IMPSEL                 (0x1f<<24)
#define RG_SSUSB_TX_IMPCAL_CALCYC          (0x3f<<16)
#define RG_SSUSB_TX_IMPCAL_STBCYC          (0x1f<<10)
#define RG_SSUSB_TX_IMPCAL_CYCCNT          (0x3ff<<0)

#define RG_SSUSB_FORCE_TX_IMPSEL_OFST      (31)
#define RG_SSUSB_TX_IMPCAL_EN_OFST         (30)
#define RG_SSUSB_FORCE_TX_IMPCAL_EN_OFST   (29)
#define RG_SSUSB_TX_IMPSEL_OFST            (24)
#define RG_SSUSB_TX_IMPCAL_CALCYC_OFST     (16)
#define RG_SSUSB_TX_IMPCAL_STBCYC_OFST     (10)
#define RG_SSUSB_TX_IMPCAL_CYCCNT_OFST     (0)

/* U3D_PHYD_IMPCAL1 */
#define RG_SSUSB_FORCE_RX_IMPSEL           (0x1<<31)
#define RG_SSUSB_RX_IMPCAL_EN              (0x1<<30)
#define RG_SSUSB_FORCE_RX_IMPCAL_EN        (0x1<<29)
#define RG_SSUSB_RX_IMPSEL                 (0x1f<<24)
#define RG_SSUSB_RX_IMPCAL_CALCYC          (0x3f<<16)
#define RG_SSUSB_RX_IMPCAL_STBCYC          (0x1f<<10)
#define RG_SSUSB_RX_IMPCAL_CYCCNT          (0x3ff<<0)

#define RG_SSUSB_FORCE_RX_IMPSEL_OFST      (31)
#define RG_SSUSB_RX_IMPCAL_EN_OFST         (30)
#define RG_SSUSB_FORCE_RX_IMPCAL_EN_OFST   (29)
#define RG_SSUSB_RX_IMPSEL_OFST            (24)
#define RG_SSUSB_RX_IMPCAL_CALCYC_OFST     (16)
#define RG_SSUSB_RX_IMPCAL_STBCYC_OFST     (10)
#define RG_SSUSB_RX_IMPCAL_CYCCNT_OFST     (0)

/* U3D_PHYD_CDR0 */
#define RG_SSUSB_CDR_BIC_LTR               (0xf<<28)

#define RG_SSUSB_CDR_BIC_LTR_OFST     (28)

/* U3D_PHYD_CDR1 */
#define RG_SSUSB_CDR_BIR_LTD1              (0x1f<<24)
#define RG_SSUSB_CDR_BIR_LTR               (0x1f<<16)
#define RG_SSUSB_CDR_BIR_LTD0              (0x1f<<8)
#define RG_SSUSB_CDR_BW_SEL                (0x3<<6)
#define RG_SSUSB_CDR_BIC_LTD1              (0xf<<0)

#define RG_SSUSB_CDR_BIR_LTD1_OFST         (24)
#define RG_SSUSB_CDR_BIR_LTR_OFST          (16)
#define RG_SSUSB_CDR_BIR_LTD0_OFST         (8)
#define RG_SSUSB_CDR_BW_SEL_OFST           (6)
#define RG_SSUSB_CDR_BIC_LTD1_OFST         (0)

/* U3D_PHYD_MIX6 */
#define RG_SSUSB_IDEMSEL                   (0x1f<<21)
#define RG_SSUSB_FORCE_IDEMSEL             (0x1<<20)
#define RG_SSUSB_IDRVSEL                   (0x1f<<15)
#define RG_SSUSB_FORCE_IDRVSEL             (0x1<<14)

#define RG_SSUSB_IDEMSEL_OFST              (21)
#define RG_SSUSB_FORCE_IDEMSEL_OFST        (20)
#define RG_SSUSB_IDRVSEL_OFST              (15)
#define RG_SSUSB_FORCE_IDRVSEL_OFST        (14)


/* ///////////////////////////////////////////// */
/**   SSUSB_USB30_PHYA_SIV_B2_BASE **/
#define U3D_B2_PHYD_RXDET2		(SSUSB_USB30_PHYA_SIV_B2_BASE + 0x2c)

/* U3D_B2_PHYD_RXDET2 */
#define RG_SSUSB_PHYD_TRAINDEC_FORCE_CGEN  (0x1<<31)
#define RG_SSUSB_PHYD_BERTLB_FORCE_CGEN    (0x1<<30)
#define RG_SSUSB_PHYD_T2RLB_FORCE_CGEN     (0x1<<29)
#define RG_SSUSB_LCK2REF_EXT_EN            (0x1<<28)
#define RG_SSUSB_G2_LCK2REF_EXT_SEL        (0xf<<24)
#define RG_SSUSB_LCK2REF_EXT_SEL           (0xf<<20)
#define RG_SSUSB_PDN_T_SEL                 (0x3<<18)
#define RG_SSUSB_RXDET_STB3_SET_P3         (0x1ff<<9)
#define RG_SSUSB_RXDET_STB2_SET_P3         (0x1ff<<0)

#define RG_SSUSB_PHYD_TRAINDEC_FORCE_CGEN_OFST (31)
#define RG_SSUSB_PHYD_BERTLB_FORCE_CGEN_OFST (30)
#define RG_SSUSB_PHYD_T2RLB_FORCE_CGEN_OFST (29)
#define RG_SSUSB_LCK2REF_EXT_EN_OFST       (28)
#define RG_SSUSB_G2_LCK2REF_EXT_SEL_OFST   (24)
#define RG_SSUSB_LCK2REF_EXT_SEL_OFST      (20)
#define RG_SSUSB_PDN_T_SEL_OFST            (18)
#define RG_SSUSB_RXDET_STB3_SET_P3_OFST    (9)
#define RG_SSUSB_RXDET_STB2_SET_P3_OFST    (0)

/* ///////////////////////////////////////////// */
/**   SSUSB_USB30_PHYA_SIV_B_BASE **/
#define U3D_USB30_PHYA_REG0 (SSUSB_USB30_PHYA_SIV_B_BASE + 0x0000)
#define U3D_USB30_PHYA_REG1 (SSUSB_USB30_PHYA_SIV_B_BASE + 0x0004)
#define U3D_USB30_PHYA_REG6 (SSUSB_USB30_PHYA_SIV_B_BASE + 0x0018)
#define U3D_USB30_PHYA_REG8 (SSUSB_USB30_PHYA_SIV_B_BASE + 0x0020)
#define U3D_USB30_PHYA_REGB (SSUSB_USB30_PHYA_SIV_B_BASE + 0x002C)


/* U3D_USB30_PHYA_REG0 */
#define RG_SSUSB_BGR_EN                    (0x1<<31)
#define RG_SSUSB_CHPEN                     (0x1<<30)
#define RG_SSUSB_BG_DIV                    (0x3<<28)
#define RG_SSUSB_INTR_EN                   (0x1<<26)
#define RG_SSUSB_MPX_EN                    (0x1<<24)
#define RG_SSUSB_MPX_SEL                   (0xff<<16)
#define RG_SSUSB_IEXT_INTR_CTRL            (0x3f<<10)
#define RG_SSUSB_VRT_VREF_SEL              (0xf<<6)
#define RG_SSUSB_REF_EN                    (0x1<<5)
#define RG_SSUSB_BG_MONEN                  (0x1<<4)
#define RG_SSUSB_INT_BIAS_SEL              (0x1<<3)
#define RG_SSUSB_EXT_BIAS_SEL              (0x1<<2)

#define RG_SSUSB_BGR_EN_OFST               (31)
#define RG_SSUSB_CHPEN_OFST                (30)
#define RG_SSUSB_BG_DIV_OFST               (28)
#define RG_SSUSB_INTR_EN_OFST              (26)
#define RG_SSUSB_MPX_EN_OFST               (24)
#define RG_SSUSB_MPX_SEL_OFST              (16)
#define RG_SSUSB_IEXT_INTR_CTRL_OFST       (10)
#define RG_SSUSB_RG_SSUSB_VRT_VREF_SEL     (6)
#define RG_SSUSB_REF_EN_OFST               (5)
#define RG_SSUSB_BG_MONEN_OFST             (4)
#define RG_SSUSB_INT_BIAS_SEL_OFST         (3)
#define RG_SSUSB_EXT_BIAS_SEL_OFST         (2)

/* U3D_USB30_PHYA_REG1 */
#define RG_SSUSB_VUSB10_ON (1<<29)
#define RG_SSUSB_VUSB10_ON_OFST (29)

/* U3D_USB30_PHYA_REG6 */
#define RG_SSUSB_TX_EIDLE_CM               (0xf<<28)
#define RG_SSUSB_RXLBTX_EN                 (0x1<<27)
#define RG_SSUSB_TXLBRX_EN                 (0x1<<26)
#define RG_SSUSB_RESERVE6                  (0x1<<22)
#define RG_SSUSB_RESERVE                   (0x3ff<<16)
#define RG_SSUSB_PLL_POSDIV                (0x3<<14)
#define RG_SSUSB_PLL_AUTOK_LOAD            (0x1<<13)
#define RG_SSUSB_PLL_VOD_EN                (0x1<<12)
#define RG_SSUSB_PLL_MONREF_EN             (0x1<<11)
#define RG_SSUSB_PLL_MONCK_EN              (0x1<<10)
#define RG_SSUSB_PLL_MONVC_EN              (0x1<<9)
#define RG_SSUSB_PLL_RLH_EN                (0x1<<8)
#define RG_SSUSB_PLL_AUTOK_KS              (0x3<<6)
#define RG_SSUSB_PLL_AUTOK_KF              (0x3<<4)
#define RG_SSUSB_PLL_RST_DLY               (0x3<<2)

#define RG_SSUSB_TX_EIDLE_CM_OFST          (28)
#define RG_SSUSB_RXLBTX_EN_OFST            (27)
#define RG_SSUSB_TXLBRX_EN_OFST            (26)
#define RG_SSUSB_RESERVE6_OFST             (22)
#define RG_SSUSB_RESERVE_OFST              (16)
#define RG_SSUSB_PLL_POSDIV_OFST           (14)
#define RG_SSUSB_PLL_AUTOK_LOAD_OFST       (13)
#define RG_SSUSB_PLL_VOD_EN_OFST           (12)
#define RG_SSUSB_PLL_MONREF_EN_OFST        (11)
#define RG_SSUSB_PLL_MONCK_EN_OFST         (10)
#define RG_SSUSB_PLL_MONVC_EN_OFST         (9)
#define RG_SSUSB_PLL_RLH_EN_OFST           (8)
#define RG_SSUSB_PLL_AUTOK_KS_OFST         (6)
#define RG_SSUSB_PLL_AUTOK_KF_OFST         (4)
#define RG_SSUSB_PLL_RST_DLY_OFST          (2)

/* U3D_USB30_PHYA_REG8 */
#define RG_SSUSB_CDR_BYPASS                (0x3<<3)

#define RG_SSUSB_CDR_BYPASS_OFST           (3)

/* U3D_USB30_PHYA_REGB */
#define RG_SSUSB_RESERVE10                 (0x1<<10)

#define RG_SSUSB_RESERVE10_OFST                 (10)


/* ///////////////////////////////////////////// */
/**   SSUSB_SIFSLV_U3PHYA_DA_BASE **/
#define U3D_U3PHYA_DA_REG0	(SSUSB_SIFSLV_U3PHYA_DA_BASE + 0x0)
#define U3D_U3PHYA_DA_REG32 (SSUSB_SIFSLV_U3PHYA_DA_BASE + 0x0060)
#define U3D_U3PHYA_DA_REG36 (SSUSB_SIFSLV_U3PHYA_DA_BASE + 0x0070)

/* U3D_U3PHYA_DA_REG0 */
#define RG_PCIE_SPEED_PE2D                 (0x1<<24)
#define RG_PCIE_SPEED_PE2H                 (0x1<<23)
#define RG_PCIE_SPEED_PE1D                 (0x1<<22)
#define RG_PCIE_SPEED_PE1H                 (0x1<<21)
#define RG_PCIE_SPEED_U3                   (0x1<<20)
#define RG_SSUSB_XTAL_EXT_EN_PE2D          (0x3<<18)
#define RG_SSUSB_XTAL_EXT_EN_PE2H          (0x3<<16)
#define RG_SSUSB_XTAL_EXT_EN_PE1D          (0x3<<14)
#define RG_SSUSB_XTAL_EXT_EN_PE1H          (0x3<<12)
#define RG_SSUSB_XTAL_EXT_EN_U3            (0x3<<10)
#define RG_SSUSB_CDR_REFCK_SEL_PE2D        (0x3<<8)
#define RG_SSUSB_CDR_REFCK_SEL_PE2H        (0x3<<6)
#define RG_SSUSB_CDR_REFCK_SEL_PE1D        (0x3<<4)
#define RG_SSUSB_CDR_REFCK_SEL_PE1H        (0x3<<2)
#define RG_SSUSB_CDR_REFCK_SEL_U3          (0x3<<0)

#define RG_PCIE_SPEED_PE2D_OFST            (24)
#define RG_PCIE_SPEED_PE2H_OFST            (23)
#define RG_PCIE_SPEED_PE1D_OFST            (22)
#define RG_PCIE_SPEED_PE1H_OFST            (21)
#define RG_PCIE_SPEED_U3_OFST              (20)
#define RG_SSUSB_XTAL_EXT_EN_PE2D_OFST     (18)
#define RG_SSUSB_XTAL_EXT_EN_PE2H_OFST     (16)
#define RG_SSUSB_XTAL_EXT_EN_PE1D_OFST     (14)
#define RG_SSUSB_XTAL_EXT_EN_PE1H_OFST     (12)
#define RG_SSUSB_XTAL_EXT_EN_U3_OFST       (10)
#define RG_SSUSB_CDR_REFCK_SEL_PE2D_OFST   (8)
#define RG_SSUSB_CDR_REFCK_SEL_PE2H_OFST   (6)
#define RG_SSUSB_CDR_REFCK_SEL_PE1D_OFST   (4)
#define RG_SSUSB_CDR_REFCK_SEL_PE1H_OFST   (2)
#define RG_SSUSB_CDR_REFCK_SEL_U3_OFST     (0)

/* U3D_U3PHYA_DA_REG32  */
#define RG_SSUSB_EQ_RSTEP1_PE2D_OFST       (30)
#define RG_SSUSB_EQ_RSTEP1_PE2H_OFST       (28)
#define RG_SSUSB_EQ_RSTEP1_PE1D_OFST       (26)
#define RG_SSUSB_EQ_RSTEP1_PE1H_OFST       (24)
#define RG_SSUSB_EQ_RSTEP1_U3_OFST         (22)
#define RG_SSUSB_LFPS_DEGLITCH_PE2D_OFST   (20)
#define RG_SSUSB_LFPS_DEGLITCH_PE2H_OFST   (18)
#define RG_SSUSB_LFPS_DEGLITCH_PE1D_OFST   (16)
#define RG_SSUSB_LFPS_DEGLITCH_PE1H_OFST   (14)
#define RG_SSUSB_LFPS_DEGLITCH_U3_OFST     (12)
#define RG_SSUSB_CDR_KVSEL_PE2D_OFST       (11)
#define RG_SSUSB_CDR_KVSEL_PE2H_OFST       (10)
#define RG_SSUSB_CDR_KVSEL_PE1D_OFST       (9)
#define RG_SSUSB_CDR_KVSEL_PE1H_OFST       (8)
#define RG_SSUSB_CDR_KVSEL_U3_OFST         (7)
#define RG_SSUSB_CDR_FBDIV_PE2D_OFST       (0)

/* U3D_reg32 */
#define RG_SSUSB_EQ_RSTEP1_PE2D            (0x3<<30)
#define RG_SSUSB_EQ_RSTEP1_PE2H            (0x3<<28)
#define RG_SSUSB_EQ_RSTEP1_PE1D            (0x3<<26)
#define RG_SSUSB_EQ_RSTEP1_PE1H            (0x3<<24)
#define RG_SSUSB_EQ_RSTEP1_U3              (0x3<<22)
#define RG_SSUSB_LFPS_DEGLITCH_PE2D        (0x3<<20)
#define RG_SSUSB_LFPS_DEGLITCH_PE2H        (0x3<<18)
#define RG_SSUSB_LFPS_DEGLITCH_PE1D        (0x3<<16)
#define RG_SSUSB_LFPS_DEGLITCH_PE1H        (0x3<<14)
#define RG_SSUSB_LFPS_DEGLITCH_U3          (0x3<<12)
#define RG_SSUSB_CDR_KVSEL_PE2D            (0x1<<11)
#define RG_SSUSB_CDR_KVSEL_PE2H            (0x1<<10)
#define RG_SSUSB_CDR_KVSEL_PE1D            (0x1<<9)
#define RG_SSUSB_CDR_KVSEL_PE1H            (0x1<<8)
#define RG_SSUSB_CDR_KVSEL_U3              (0x1<<7)
#define RG_SSUSB_CDR_FBDIV_PE2D            (0x7f<<0)

/* U3D_U3PHYA_DA_REG36*/
#define RG_SSUSB_DA_SSUSB_PLL_BAND         (0x3F<<11)
#define RG_SSUSB_DA_SSUSB_PLL_BAND_OFST    (11)

/* ///////////////////////////////////////////// */
extern u32 get_devinfo_with_index(u32 index);

#endif

