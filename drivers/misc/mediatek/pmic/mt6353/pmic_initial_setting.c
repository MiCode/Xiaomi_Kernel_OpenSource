/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "linux/delay.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_chip.h>
#ifdef CONFIG_OF
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#endif
#define PMIC_32K_LESS_DETECT_V1      0
#define PMIC_CO_TSX_V1               1

#define PMIC_READ_REGISTER_UINT32(reg)	(*(volatile uint32_t *const)(reg))
#define PMIC_INREG32(x)	PMIC_READ_REGISTER_UINT32((uint32_t *)((void *)(x)))
#define PMIC_WRITE_REGISTER_UINT32(reg, val)	((*(volatile uint32_t *const)(reg)) = (val))
#define PMIC_OUTREG32(x, y)	PMIC_WRITE_REGISTER_UINT32((uint32_t *)((void *)(x)), (uint32_t)(y))

#define PMIC_DRV_Reg32(addr)             PMIC_INREG32(addr)
#define PMIC_DRV_WriteReg32(addr, data)  PMIC_OUTREG32(addr, data)

int PMIC_MD_INIT_SETTING_V1(void)
{
	unsigned int ret = 0;
#if PMIC_32K_LESS_DETECT_V1
	unsigned int pmic_reg = 0;
#endif

#if PMIC_CO_TSX_V1
	struct device_node *modem_temp_node = NULL;
	void __iomem *modem_temp_base = NULL;
#endif

#if PMIC_32K_LESS_DETECT_V1

	/* 32k less crystal auto detect start */
	ret |= pmic_config_interface(0x701E, 0x1, 0x1, 0);
	ret |= pmic_config_interface(0x701E, 0x3, 0xF, 1);
	ret = pmic_read_interface(0x7000, &pmic_reg, 0xffff, 0);
	ret |= pmic_config_interface(0x701E, 0x0, 0x1, 0);
	ret = pmic_config_interface(0xA04, 0x1, 0x1, 3);
	if ((pmic_reg & 0x200) == 0x200) {
		/* VCTCXO on MT6176 , OFF XO on MT6353
		   HW control, use srclken_0 */

		ret = pmic_config_interface(0xA04, 0x0, 0x7, 11);
		pr_err("[PMIC] VCTCXO on MT6176 , OFF XO on MT6353\n");
	} else {
		/*  HW control, use srclken_1, for LP */
		ret = pmic_config_interface(0xA04, 0x1, 0x1, 4);
		ret = pmic_config_interface(0xA04, 0x1, 0x7, 11);
		pr_err("[PMIC] VCTCXO 0x7000=0x%x\n", pmic_reg);
	}
#endif

#if PMIC_CO_TSX_V1
	modem_temp_node = of_find_compatible_node(NULL, NULL, "mediatek,MODEM_TEMP_SHARE");

	if (modem_temp_node == NULL) {
		pr_err("PMIC get modem_temp_node failed\n");
		modem_temp_base = 0;
		return ret;
	}

	modem_temp_base = of_iomap(modem_temp_node, 0);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x011f);
	pr_err("[PMIC] TEMP_SHARE_CTRL:0x%x\n", PMIC_DRV_Reg32(modem_temp_base));
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base + 0x04, 0x013f);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x0);
	pr_err("[PMIC] TEMP_SHARE_CTRL:0x%x _RATIO:0x%x\n", PMIC_DRV_Reg32(modem_temp_base),
	       PMIC_DRV_Reg32(modem_temp_base + 0x04));
#endif
	return ret;
}

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	chip_version = pmic_get_register_value(PMIC_SWCID);

	/*1.UVLO off */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_TOP_RST_STATUS, upmu_get_reg_value(MT6353_TOP_RST_STATUS));
	/*2.thermal shutdown 150 */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_THERMALSTATUS, upmu_get_reg_value(MT6353_THERMALSTATUS));
	/*3.power not good */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_PGSTATUS0, upmu_get_reg_value(MT6353_PGSTATUS0));
	/*4.buck oc */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_OCSTATUS1, upmu_get_reg_value(MT6353_OCSTATUS1));
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_OCSTATUS2, upmu_get_reg_value(MT6353_OCSTATUS2));
	/*5.long press shutdown */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_STRUP_CON4, upmu_get_reg_value(MT6353_STRUP_CON4));
	/*6.WDTRST */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n",
		 MT6353_TOP_RST_MISC, upmu_get_reg_value(MT6353_TOP_RST_MISC));
	/*7.CLK TRIM */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] Reg[0x%x]=0x%x\n", 0x250,
		 upmu_get_reg_value(0x250));

	/* This flag is used for fg to judge if battery even removed manually */
	is_battery_remove = !pmic_get_register_value(PMIC_STRUP_PWROFF_SEQ_EN);
	is_wdt_reboot_pmic = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	is_wdt_reboot_pmic_chk = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);
	/*--------------------------------------------------------*/

	pr_err("[PMIC] 6353 PMIC Chip = 0x%x\n", chip_version);
	pr_err("[PMIC] 2015-06-17...\n");
	pr_err("[PMIC] is_battery_remove =%d is_wdt_reboot=%d\n",
	       is_battery_remove, is_wdt_reboot_pmic);
	pr_err("[PMIC] is_wdt_reboot_chk=%d\n", is_wdt_reboot_pmic_chk);

	pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x1);

/* [0:0]: DDUVLO_DEB_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x6, 0x1, 0x1, 0);
/* [2:2]: RG_STRUP_AUXADC_START_SEL; Filby, ZF, SW Huang, 12/9;
On 4/6 OPPO gauge issue meeting, conflict on GPS and Gauge HWOCV setting in sleep,
change to 1'b0 HW control, confirm with Peter_SW, SW Haung, Sam Chen, Ricky Wu, Argus, Anderson and Mitch. */
	ret = pmic_config_interface(0xC, 0x0, 0x1, 2);
/* [3:3]: RG_STRUP_AUXADC_RSTB_SEL; Filby, ZF, SW Huang, 12/9 */
	ret = pmic_config_interface(0xC, 0x1, 0x1, 3);
/* [0:0]: STRUP_PWROFF_SEQ_EN; 12/1 , Kim */
	ret = pmic_config_interface(0xE, 0x1, 0x1, 0);
/* [1:1]: STRUP_PWROFF_PREOFF_EN; 12/1 , Kim */
	ret = pmic_config_interface(0xE, 0x1, 0x1, 1);
/* [15:15]: RG_STRUP_ENVTEM_CTRL; 12/1 , Kim */
	ret = pmic_config_interface(0x18, 0x1, 0x1, 15);
/* [4:4]: RG_STRUP_VCORE2_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 4);
/* [5:5]: RG_STRUP_VMCH_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 5);
/* [6:6]: RG_STRUP_VMC_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 6);
/* [7:7]: RG_STRUP_VUSB33_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 7);
/* [8:8]: RG_STRUP_VSRAM_PROC_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 8);
/* [9:9]: RG_STRUP_VPROC_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 9);
/* [10:10]: RG_STRUP_VDRAM_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 10);
/* [11:11]: RG_STRUP_VAUD28_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 11);
/* [12:12]: RG_STRUP_VEMC_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 12);
/* [13:13]: RG_STRUP_VS1_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 13);
/* [14:14]: RG_STRUP_VCORE_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 14);
/* [15:15]: RG_STRUP_VAUX18_PG_H2L_EN; 12/1 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 15);
/* [12:12]: RG_RST_DRVSEL; 12/1 , Kim */
	ret = pmic_config_interface(0x24, 0x1, 0x1, 12);
/* [13:13]: RG_EN_DRVSEL; 12/1 , Kim */
	ret = pmic_config_interface(0x24, 0x1, 0x1, 13);
/* [4:4]: RG_SRCLKEN_IN0_HW_MODE; Lan, 12/7 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 4);
/* [5:5]: RG_SRCLKEN_IN1_HW_MODE; Lan, 12/7 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 5);
/* [6:6]: RG_OSC_SEL_HW_MODE; Lan, 12/7 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 6);
/* [0:0]: RG_SMT_WDTRSTB_IN; 12/1 , Kim */
	ret = pmic_config_interface(0x222, 0x1, 0x1, 0);
/* [1:1]: RG_SMT_HOMEKEY; 12/1 , Kim */
	ret = pmic_config_interface(0x222, 0x1, 0x1, 1);
/* [2:2]: RG_SMT_SRCLKEN_IN0; 12/1 , Kim */
	ret = pmic_config_interface(0x222, 0x1, 0x1, 2);
/* [3:3]: RG_SMT_SRCLKEN_IN1; 12/1 , Kim */
	ret = pmic_config_interface(0x222, 0x1, 0x1, 3);
/* [2:2]: CLK_75K_32K_SEL; Angela/ IC, 12/10 */
	ret = pmic_config_interface(0x268, 0x1, 0x1, 2);
/* [0:0]: CLK_LDO_CALI_75K_CK_PDN; 12/24, ZF, power down,align fly mode.
LDO auto calibration, so far PMIC not operate the function.  */
	ret = pmic_config_interface(0x27A, 0x1, 0x1, 0);
/* [1:1]: CLK_G_SMPS_AUD_CK_PDN_HWEN; 12/24, ZF,
type-C work around function.( Sleep mode need operate, need this clk) */
	ret = pmic_config_interface(0x286, 0x0, 0x1, 1);
/* [9:9]: CLK_BUCK_VPA_9M_CK_PDN_HWEN; 12/24, ZF,
need SW mode for MD/PA control */
	ret = pmic_config_interface(0x286, 0x0, 0x1, 9);
/* [4:4]: BUCK_VPA_EN_OC_SDN_SEL; Willy, 12/7 */
	ret = pmic_config_interface(0x422, 0x1, 0x1, 4);
/* [4:3]: RG_VSRAM_PROC_VSLEEP_VOLTAGE; Chc, 12/9,  Design 00: 0.7V */
	ret = pmic_config_interface(0x43E, 0x0, 0x3, 3);
/* [1:0]: RG_VPA_RZSEL; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x440, 0x1, 0x3, 0);
/* [7:6]: RG_VPA_CSMIR; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x440, 0x2, 0x3, 6);
/* [11:10]: RG_VPA_SLP; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x440, 0x2, 0x3, 10);
/* [7:6]: RG_VPA_ZX_OS; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x442, 0x3, 0x3, 6);
/* [6:4]: RG_VCORE_RZSEL; 1/20, Tim/ YT/ Ice,
for VCORE/ VCORE2 transient improve & stability confirm in Corner. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x448, 0x4, 0x7, 4);
/* [10:8]: DA_QI_VS1_BURST; YT, 12/7 */
	ret = pmic_config_interface(0x466, 0x5, 0x7, 8);
/* [6:4]: RG_VCORE2_RZSEL; 1/20, Tim/ YT/ Ice,
for VCORE/ VCORE2 transient improve & stability confirm in Corner. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x46A, 0x4, 0x7, 4);
/* [1:1]: BUCK_VPROC_VOSEL_CTRL; 12/22 Align with fly suspend mode,
ZF/ Kashi/ Willy/Chia-Lin/ Mark. No DVS VOSEL in SW mode. */
	ret = pmic_config_interface(0x600, 0x1, 0x1, 1);
/* [5:3]: BUCK_VPROC_VOSEL_SEL; ZF, align with default if no FS mode, 12/15 */
	ret = pmic_config_interface(0x602, 0x0, 0x7, 3);
/* [6:0]: BUCK_VPROC_SFCHG_FRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x606, 0x11, 0x7F, 0);
/* [14:8]: BUCK_VPROC_SFCHG_RRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x606, 0x4, 0x7F, 8);
/* [6:0]: BUCK_VPROC_VOSEL_SLEEP; Tim Lee, CC lee, 4/28,
the digital setting need align analog setting for sleep voltage. */
	ret = pmic_config_interface(0x60E, 0x10, 0x7F, 0);
/* [1:0]: BUCK_VPROC_TRANS_TD; Tim, 12/9 */
	ret = pmic_config_interface(0x610, 0x3, 0x3, 0);
/* [5:4]: BUCK_VPROC_TRANS_CTRL; Tim, 12/9 */
	ret = pmic_config_interface(0x610, 0x1, 0x3, 4);
/* [8:8]: BUCK_VPROC_VSLEEP_EN; Willy, 12/9 */
	ret = pmic_config_interface(0x610, 0x1, 0x1, 8);
/* [1:1]: BUCK_VS1_VOSEL_CTRL; Lan, 12/7, VOSEL HW mode */
	ret = pmic_config_interface(0x612, 0x1, 0x1, 1);
/* [6:0]: BUCK_VS1_VOSEL; Willy, 12/9 */
	ret = pmic_config_interface(0x620, 0x50, 0x7F, 0);
/* [8:8]: BUCK_VS1_VSLEEP_EN; Willy, 12/9, sleep HW mode */
	ret = pmic_config_interface(0x626, 0x1, 0x1, 8);
/* [1:1]: BUCK_VCORE_VOSEL_CTRL; Willy, 12/9, VOSEL HW mode */
	ret = pmic_config_interface(0x628, 0x1, 0x1, 1);
/* [5:3]: BUCK_VCORE_VOSEL_SEL; ZF, align with default if no FS mode, 12/15 */
	ret = pmic_config_interface(0x62A, 0x0, 0x7, 3);
/* [6:0]: BUCK_VCORE_SFCHG_FRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x62E, 0x11, 0x7F, 0);
/* [14:8]: BUCK_VCORE_SFCHG_RRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x62E, 0x4, 0x7F, 8);
/* [6:0]: BUCK_VCORE_VOSEL_SLEEP; Tim Lee, CC lee, 4/28,
the digital setting need align analog setting for sleep voltage. */
	ret = pmic_config_interface(0x636, 0x10, 0x7F, 0);
/* [1:0]: BUCK_VCORE_TRANS_TD; Tim, 12/9 */
	ret = pmic_config_interface(0x638, 0x3, 0x3, 0);
/* [8:8]: BUCK_VCORE_VSLEEP_EN; Tim, 12/9, sleep HW mode */
	ret = pmic_config_interface(0x638, 0x1, 0x1, 8);
/* [1:1]: BUCK_VCORE2_VOSEL_CTRL; Tim, 12/9, VOSEL HW mode */
	ret = pmic_config_interface(0x63A, 0x1, 0x1, 1);
/* [5:3]: BUCK_VCORE2_VOSEL_SEL; ZF, align with default if no FS mode, 12/15 */
	ret = pmic_config_interface(0x63C, 0x0, 0x7, 3);
/* [6:0]: BUCK_VCORE2_SFCHG_FRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x640, 0x11, 0x7F, 0);
/* [14:8]: BUCK_VCORE2_SFCHG_RRATE; Tim, 12/9 */
	ret = pmic_config_interface(0x640, 0x4, 0x7F, 8);
/* [6:0]: BUCK_VCORE2_VOSEL_SLEEP; Tim Lee, CC lee, 4/28,
the digital setting need align analog setting for sleep voltage. */
	ret = pmic_config_interface(0x648, 0x10, 0x7F, 0);
/* [1:0]: BUCK_VCORE2_TRANS_TD; Tim, 12/9 */
	ret = pmic_config_interface(0x64A, 0x3, 0x3, 0);
/* [8:8]: BUCK_VCORE2_VSLEEP_EN; Tim, 12/9, Sleep HW mode */
	ret = pmic_config_interface(0x64A, 0x1, 0x1, 8);
/* [6:0]: BUCK_VPA_SFCHG_FRATE; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x64E, 0x0, 0x7F, 0);
/* [14:8]: BUCK_VPA_SFCHG_RRATE; 2/2, Mason/ CW/ Yichia/ SJ/ Ice,
VPA corner issue. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0x64E, 0x1, 0x7F, 8);
/* [9:8]: BUCK_VPA_DVS_BW_TD; Mason, 12/9 */
	ret = pmic_config_interface(0x654, 0x1, 0x3, 8);
/* [8:8]: LDO_VLDO28_FAST_TRAN_CL_EN; Willy: No Constant load */
	ret = pmic_config_interface(0xA1E, 0x0, 0x1, 8);
/* [1:1]: LDO_VSRAM_PROC_VOSEL_CTRL; Willy, 12/9, VOSEL HW mode */
	ret = pmic_config_interface(0xA20, 0x1, 0x1, 1);
/* [5:3]: LDO_VSRAM_PROC_VOSEL_SEL; ZF, align with default if no FS mode, 12/15 */
	ret = pmic_config_interface(0xA20, 0x0, 0x7, 3);
/* [6:0]: LDO_VSRAM_PROC_VOSEL_SLEEP; CC lee, 4/28,
the digital setting need align analog setting for sleep voltage. */
	ret = pmic_config_interface(0xA2A, 0x10, 0x7F, 0);
/* [8:8]: LDO_VRF12_FAST_TRAN_CL_EN; Willy, 12/9, off fast transient */
	ret = pmic_config_interface(0xA46, 0x0, 0x1, 8);
/* [8:8]: LDO_VDRAM_FAST_TRAN_CL_EN; Willy, 12/9, off constant load */
	ret = pmic_config_interface(0xA6C, 0x0, 0x1, 8);
/* [9:9]: LDO_VMC_OCFB_EN; Willy, 12/7, Turn on OCFB */
	ret = pmic_config_interface(0xA7C, 0x1, 0x1, 9);
/* [8:8]: LDO_VMC_FAST_TRAN_CL_EN; Willy, 12/25, disable constant load for VMC */
	ret = pmic_config_interface(0xA7E, 0x0, 0x1, 8);
/* [9:9]: LDO_VMCH_OCFB_EN; Willy, 12/7, Turn on OCFB */
	ret = pmic_config_interface(0xA82, 0x1, 0x1, 9);
/* [8:8]: LDO_VMCH_FAST_TRAN_CL_EN; Willy, 12/7, turm off constant load */
	ret = pmic_config_interface(0xA84, 0x0, 0x1, 8);
/* [8:8]: LDO_VEMC33_FAST_TRAN_CL_EN; 12/25, disable constant load for fast transient. */
	ret = pmic_config_interface(0xA8E, 0x0, 0x1, 8);
/* [9:8]: RG_VMCH_VOSEL; 12/16, VMCH=3.0V */
	ret = pmic_config_interface(0xAB2, 0x1, 0x3, 8);
/* [9:9]: RG_VEMC33_CL_EN; 12/25, turn off dummy load */
	ret = pmic_config_interface(0xAB8, 0x0, 0x1, 9);
/* [10:8]: RG_VMC_VOSEL; 12/16, VMCH=3.0V */
	ret = pmic_config_interface(0xAC2, 0x6, 0x7, 8);
/* [13:12]: RG_VDRAM_PCUR_CAL; 1/28,Robert/ Chc/ Ice, for VDRAM SS Corner DCS Ringing Issue.
2/16, Arnold_Y/ Ice, DRAM stress evaluation. 2/18, Jade Minus HW Project meeting. */
	ret = pmic_config_interface(0xAD4, 0x3, 0x3, 12);
/* [5:4]: AUXADC_TRIM_CH2_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x1, 0x3, 4);
/* [7:6]: AUXADC_TRIM_CH3_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x1, 0x3, 6);
/* [9:8]: AUXADC_TRIM_CH4_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x1, 0x3, 8);
/* [11:10]: AUXADC_TRIM_CH5_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x1, 0x3, 10);
/* [13:12]: AUXADC_TRIM_CH6_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x1, 0x3, 12);
/* [15:14]: AUXADC_TRIM_CH7_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB4, 0x2, 0x3, 14);
/* [1:0]: AUXADC_TRIM_CH8_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB6, 0x1, 0x3, 0);
/* [3:2]: AUXADC_TRIM_CH9_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB6, 0x1, 0x3, 2);
/* [5:4]: AUXADC_TRIM_CH10_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB6, 0x1, 0x3, 4);
/* [7:6]: AUXADC_TRIM_CH11_SEL; 12/9, Filby, Chuan-hung */
	ret = pmic_config_interface(0xEB6, 0x1, 0x3, 6);
/* [14:14]: AUXADC_START_SHADE_EN; 12/9, Chuan-Hung */
	ret = pmic_config_interface(0xEC8, 0x1, 0x1, 14);
/* [9:0]: AUXADC_MDBG_DET_PRD; Filby, ZF, SW Huang, 12/9 */
	ret = pmic_config_interface(0xF18, 0xC, 0x3FF, 0);
/* [9:0]: AUXADC_MDRT_DET_PRD; Filby, ZF, SW Huang, 12/9 */
	ret = pmic_config_interface(0xF1E, 0xC, 0x3FF, 0);
/* [15:15]: AUXADC_MDRT_DET_EN; Filby, ZF, SW Huang, 12/9 */
	ret = pmic_config_interface(0xF1E, 0x1, 0x1, 15);
/* [2:2]: AUXADC_MDRT_DET_WKUP_EN; Filby, ZF, SW Huang, 12/9 */
	ret = pmic_config_interface(0xF22, 0x1, 0x1, 2);

#ifdef MT6351
	ret = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH11, 5, 0);
#endif

/*****************************************************
 * below programming is used for MD setting
 *****************************************************/
	PMIC_MD_INIT_SETTING_V1();
}
