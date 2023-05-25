/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __SC89601A_HEADER__
#define __SC89601A_HEADER__

/* Register 00h */
#define SC89601A_REG_00				0x00
#define SC89601A_ENHIZ_MASK			0x80
#define SC89601A_ENHIZ_SHIFT		7
#define SC89601A_HIZ_ENABLE			1
#define SC89601A_HIZ_DISABLE		0
//begin gerrit 203957
#define SC89601A_ENILIM_MASK	    0x40
#define SC89601A_ENILIM_SHIFT	    6
#define SC89601A_ILIM_ENABLE	    1
#define SC89601A_ILIM_DISABLE	    0
//end gerrit 203957
#define SC89601A_IINLIM_MASK		0x3F
#define SC89601A_IINLIM_SHIFT		0
#define SC89601A_IINLIM_BASE		100
#define SC89601A_IINLIM_LSB			50

/* Register 01h */
#define SC89601A_REG_01				0x01
#define SC89601A_VINDPMOS_MASK      0x01
#define SC89601A_VINDPMOS_SHIFT     0
#define	SC89601A_VINDPMOS_400MV		0
#define	SC89601A_VINDPMOS_600MV		1

/* Register 0x02 */
#define SC89601A_REG_02               0x02
#define SC89601A_FORCE_DPDM_MASK     0x02
#define SC89601A_FORCE_DPDM_SHIFT    1
#define SC89601A_FORCE_DPDM          1
#define SC89601A_ICO_EN_MASK         0x10
#define SC89601A_ICO_EN_SHIFT        4
#define SC89601A_ICO_DISABLE         0
#define SC89601A_ICO_ENABLE          1


/* Register 0x03 */
#define SC89601A_REG_03              0x03
#define SC89601A_WDT_RESET_MASK      0x40
#define SC89601A_WDT_RESET_SHIFT     6
#define SC89601A_WDT_RESET           1

#define SC89601A_OTG_CONFIG_MASK     0x20
#define SC89601A_OTG_CONFIG_SHIFT    5
#define SC89601A_OTG_ENABLE          1
#define SC89601A_OTG_DISABLE         0

#define SC89601A_CHG_CONFIG_MASK     0x10
#define SC89601A_CHG_CONFIG_SHIFT    4
#define SC89601A_CHG_ENABLE          1
#define SC89601A_CHG_DISABLE         0

#define SC89601A_SYS_MINV_MASK       0x0E
#define SC89601A_SYS_MINV_SHIFT      1
#define SC89601A_SYS_MINV_BASE       3000
#define SC89601A_SYS_MINV_LSB        100


/* Register 0x04*/
#define SC89601A_REG_04              0x04
#define SC89601A_ICHG_MASK           0x3F
#define SC89601A_ICHG_SHIFT          0
#define SC89601A_ICHG_BASE           0
#define SC89601A_ICHG_LSB            60

/* Register 0x05*/
#define SC89601A_REG_05              0x05
#define SC89601A_IPRECHG_MASK        0xF0
#define SC89601A_IPRECHG_SHIFT       4
#define SC89601A_IPRECHG_BASE        60
#define SC89601A_IPRECHG_LSB         60
#define SC89601A_ITERM_MASK          0x0F
#define SC89601A_ITERM_SHIFT         0
#define SC89601A_ITERM_BASE          30
#define SC89601A_ITERM_LSB           60

/* Register 0x06*/
#define SC89601A_REG_06              0x06
#define SC89601A_VREG_MASK           0xFC
#define SC89601A_VREG_SHIFT          2
#define SC89601A_VREG_BASE           3840
#define SC89601A_VREG_LSB            16
#define SC89601A_BATLOWV_MASK        0x02
#define SC89601A_BATLOWV_SHIFT       1
#define SC89601A_BATLOWV_2800MV      0
#define SC89601A_BATLOWV_3000MV      1
#define SC89601A_VRECHG_MASK         0x01
#define SC89601A_VRECHG_SHIFT        0
#define SC89601A_VRECHG_100MV        0
#define SC89601A_VRECHG_200MV        1


/* Register 0x07*/
#define SC89601A_REG_07              0x07
#define SC89601A_EN_TERM_MASK        0x80
#define SC89601A_EN_TERM_SHIFT       7
#define SC89601A_TERM_ENABLE         1
#define SC89601A_TERM_DISABLE        0

#define SC89601A_WDT_MASK            0x30
#define SC89601A_WDT_SHIFT           4
#define SC89601A_WDT_DISABLE         0
#define SC89601A_WDT_40S             1
#define SC89601A_WDT_80S             2
#define SC89601A_WDT_160S            3
#define SC89601A_WDT_BASE            0
#define SC89601A_WDT_LSB             40

#define SC89601A_EN_TIMER_MASK       0x08
#define SC89601A_EN_TIMER_SHIFT      3

#define SC89601A_CHG_TIMER_ENABLE    1
#define SC89601A_CHG_TIMER_DISABLE   0

#define SC89601A_CHG_TIMER_MASK      0x06
#define SC89601A_CHG_TIMER_SHIFT     1
#define SC89601A_CHG_TIMER_5HOURS    0
#define SC89601A_CHG_TIMER_8HOURS    1
#define SC89601A_CHG_TIMER_12HOURS   2
#define SC89601A_CHG_TIMER_20HOURS   3

#define SC89601A_JEITA_ISET_MASK     0x01
#define SC89601A_JEITA_ISET_SHIFT    0
#define SC89601A_JEITA_ISET_50PCT    0
#define SC89601A_JEITA_ISET_20PCT    1


/* Register 0x08*/
#define SC89601A_REG_08              0x08
#define SC89601A_TREG_MASK           0x03
#define SC89601A_TREG_SHIFT          0
#define SC89601A_TREG_60C            0
#define SC89601A_TREG_80C            1
#define SC89601A_TREG_100C           2
#define SC89601A_TREG_120C           3

#define SC89601A_BAT_COMP_BASE       0
#define SC89601A_BAT_COMP_LSB        20
#define SC89601A_VCLAMP_BASE         0
#define SC89601A_VCLAMP_LSB          32


/* Register 0x09*/
#define SC89601A_REG_09              0x09
#define SC89601A_TMR2X_EN_MASK       0x40
#define SC89601A_TMR2X_EN_SHIFT      6
#define SC89601A_BATFET_DIS_MASK     0x20
#define SC89601A_BATFET_DIS_SHIFT    5
#define SC89601A_BATFET_ON           0
#define SC89601A_BATFET_OFF          1

#define SC89601A_JEITA_VSET_MASK     0x10
#define SC89601A_JEITA_VSET_SHIFT    4
#define SC89601A_JEITA_VSET_N150MV   0
#define SC89601A_JEITA_VSET_VREG     1
#define SC89601A_BATFET_DLY_MASK     0x08
#define SC89601A_BATFET_DLY_SHIFT    3
#define SC89601A_BATFET_DLY_0S       0
#define SC89601A_BATFET_DLY_10S      1
#define SC89601A_BATFET_RST_EN_MASK  0x04
#define SC89601A_BATFET_RST_EN_SHIFT 2


/* Register 0x0A*/
#define SC89601A_REG_0A              0x0A
#define SC89601A_BOOSTV_MASK         0xF0
#define SC89601A_BOOSTV_SHIFT        4
#define SC89601A_BOOSTV_BASE         3900
#define SC89601A_BOOSTV_LSB          100

#define	SC89601A_PFM_OTG_DIS_MASK	0x08
#define	SC89601A_PFM_OTG_DIS_SHIFT	3


#define SC89601A_BOOST_LIM_MASK      0x07
#define SC89601A_BOOST_LIM_SHIFT     0
#define SC89601A_BOOST_LIM_500MA     0x00
#define SC89601A_BOOST_LIM_750MA     0x01
#define SC89601A_BOOST_LIM_1200MA    0x02
#define SC89601A_BOOST_LIM_1400MA    0x03


/* Register 0x0B*/
#define SC89601A_REG_0B              0x0B
#define SC89601A_VBUS_STAT_MASK      0xE0
#define SC89601A_VBUS_STAT_SHIFT     5
#define SC89601A_VBUS_TYPE_NONE		0
#define SC89601A_VBUS_TYPE_SDP		1
#define SC89601A_VBUS_TYPE_CDP		2
#define SC89601A_VBUS_TYPE_DCP		3
#define SC89601A_VBUS_TYPE_HVDCP		4
#define SC89601A_VBUS_TYPE_UNKNOWN	5
#define SC89601A_VBUS_TYPE_NON_STD	6
#define SC89601A_VBUS_TYPE_OTG		7

#define SC89601A_CHRG_STAT_MASK      0x18
#define SC89601A_CHRG_STAT_SHIFT     3
#define SC89601A_CHRG_STAT_IDLE      0
#define SC89601A_CHRG_STAT_PRECHG    1
#define SC89601A_CHRG_STAT_FASTCHG   2
#define SC89601A_CHRG_STAT_CHGDONE   3

#define SC89601A_PG_STAT_MASK        0x04
#define SC89601A_PG_STAT_SHIFT       2
#define SC89601A_VSYS_STAT_MASK      0x01
#define SC89601A_VSYS_STAT_SHIFT     0


/* Register 0x0C*/
#define SC89601A_REG_0C              0x0c
#define SC89601A_FAULT_WDT_MASK      0x80
#define SC89601A_FAULT_WDT_SHIFT     7
#define SC89601A_FAULT_BOOST_MASK    0x40
#define SC89601A_FAULT_BOOST_SHIFT   6
#define SC89601A_FAULT_CHRG_MASK     0x30
#define SC89601A_FAULT_CHRG_SHIFT    4
#define SC89601A_FAULT_CHRG_NORMAL   0
#define SC89601A_FAULT_CHRG_INPUT    1
#define SC89601A_FAULT_CHRG_THERMAL  2
#define SC89601A_FAULT_CHRG_TIMER    3

#define SC89601A_FAULT_BAT_MASK      0x08
#define SC89601A_FAULT_BAT_SHIFT     3
#define SC89601A_FAULT_NTC_MASK      0x07
#define SC89601A_FAULT_NTC_SHIFT     0
#define SC89601A_FAULT_NTC_TSCOLD    1
#define SC89601A_FAULT_NTC_TSHOT     2

#define SC89601A_FAULT_NTC_WARM      2
#define SC89601A_FAULT_NTC_COOL      3
#define SC89601A_FAULT_NTC_COLD      5
#define SC89601A_FAULT_NTC_HOT       6


/* Register 0x0D*/
#define SC89601A_REG_0D              0x0D
#define SC89601A_FORCE_VINDPM_MASK   0x80
#define SC89601A_FORCE_VINDPM_SHIFT  7
#define SC89601A_FORCE_VINDPM_ENABLE 1
#define SC89601A_FORCE_VINDPM_DISABLE 0
#define SC89601A_VINDPM_MASK         0x7F
#define SC89601A_VINDPM_SHIFT        0
#define SC89601A_VINDPM_BASE         2600
#define SC89601A_VINDPM_LSB          100


/* Register 0x0E*/
#define SC89601A_REG_0E              0x0E
#define SC89601A_THERM_STAT_MASK     0x80
#define SC89601A_THERM_STAT_SHIFT    7

/* Register 0x11*/
#define SC89601A_REG_11              0x11
#define SC89601A_VBUS_GD_MASK        0x80
#define SC89601A_VBUS_GD_SHIFT       7


/* Register 0x12*/
#define SC89601A_REG_12              0x12
#define SC89601A_ICHGR_MASK          0x7F
#define SC89601A_ICHGR_SHIFT         0
#define SC89601A_ICHGR_BASE          0
#define SC89601A_ICHGR_LSB           50


/* Register 0x13*/
#define SC89601A_REG_13              0x13
#define SC89601A_VDPM_STAT_MASK      0x80
#define SC89601A_VDPM_STAT_SHIFT     7


/* Register 0x14*/
#define SC89601A_REG_14              0x14
#define SC89601A_RESET_MASK          0x80
#define SC89601A_RESET_SHIFT         7
#define SC89601A_RESET               1
#define SC89601A_PN_MASK             0x38
#define SC89601A_PN_SHIFT            3
#define SC89601A_DEV_REV_MASK        0x03
#define SC89601A_DEV_REV_SHIFT       0




#endif
